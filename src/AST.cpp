﻿#include "pch.h"
#include "AST.h"

#include "Types.h"
#include "Error.h"
#include "Instructions.h"
#include "Context.h"
#include "Parser.h"

namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace slljit;
	using namespace std;
	// type from, type to -> cast op
	//const map<std::pair<llvm::Type::TypeID, llvm::Type::TypeID>, llvm::Instruction::CastOps> llvm_types_to_cast_op = {{{Type::DoubleTyID, Type::IntegerTyID}, Instruction::FPToSI}, {{Type::IntegerTyID, Type::DoubleTyID}, Instruction::SIToFP}};

	Expected<Value*> BinaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Special case '=' because we don't want to emit the LHS as an expression.
		if (Op == '=')
		{
			// Assignment requires the LHS to be an identifier.
			// This assume we're building without RTTI because LLVM builds that way by
			// default.  If you build LLVM with RTTI this can be changed to a
			// dynamic_cast for automatic error checking.
			VariableExprAST* LHSE = static_cast<VariableExprAST*>(LHS.get());
			if (!LHSE)
				return make_error<CompileError>("destination of '=' must be a variable");
			// Codegen the RHS.
			auto Val_ = RHS->codegen(m_context, m_local_context);
			if (!Val_)
				return Val_.takeError();
			Value* Val = *Val_;
			auto ValTy = RHS->getType();

			// Look up global
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(LHSE->getName());
			if (gVar)
			{
				if (gVar->isConstant())
					return make_error<CompileError>("Trying to store in constant global");

				auto ptr = m_local_context.LLVM_Builder->CreateLoad(gVar->getValueType(), gVar, LHSE->getName());
				//	Value* gep = m_local_context.LLVM_Builder->CreateGEP(Type::getDoubleTy(*m_local_context.LLVM_Context), ptr, m_local_context.LLVM_Builder->getInt32(0), "ptr_");
				m_local_context.LLVM_Builder->CreateStore(Val, ptr);
				return Val;
			}

			// Look up variable.
			AllocaInst* Variable = m_local_context.NamedValues[LHSE->getName()];
			if (!Variable)
				return make_error<CompileError>("unknown variable name: "s + LHSE->getName());
			auto VariableTy = LHSE->getType();

			// if types differ cast val to variable type
			CreateExplictCast(Val, ValTy, VariableTy, m_local_context);

			m_local_context.LLVM_Builder->CreateStore(Val, Variable);
			return Val;
		}

		auto L_ = LHS->codegen(m_context, m_local_context);
		auto R_ = RHS->codegen(m_context, m_local_context);
		if (!L_)
			return L_.takeError();
		if (!R_)
			return R_.takeError();

		Value* L = *L_;
		Value* R = *R_;

		const auto LTy = LHS->getType();
		const auto RTy = RHS->getType();

		// implict type conversion
		// if any of the operands is floating type and other is not, convert all of them to floating type
		// Currently there is single floating type, in case of multiple convert to the widest.
		const auto implType = CreateImplictCast(L, R, LTy, RTy, m_local_context);
		// diferentiate floating type and integer ops

		switch (Op)
		{
		case '+': return CreateAdd(L, R, implType, m_local_context, "addtmp");
		case '-': return CreateSub(L, R, implType, m_local_context, "subtmp");
		case '*': return CreateMul(L, R, implType, m_local_context, "multmp");
		case '/': return CreateDiv(L, R, implType, m_local_context, "divtmp");
		case '<':
			L           = CtreateCMP(less_then, L, R, implType, m_local_context, "cmptmp");
			this->type_ = boolTyID;
			return L;
		case '>':
			L           = CtreateCMP(greater_than, L, R, implType, m_local_context, "cmptmp");
			this->type_ = boolTyID;
			return L;
		default: break;
		}

		// If it wasn't a builtin binary operator, it must be a user defined one. Emit
		// a call to it.
		auto F = getFunction(std::string("binary") + Op, m_context, m_local_context);
		if (!F)
			return F.takeError();

		// TODO: verify this is safe
		ArrayRef<Value*> Ops = {L, R}; // Should be safe in this case
		return m_local_context.LLVM_Builder->CreateCall(*F, Ops, "binop");
	}

	Expected<Function*> getFunction(std::string Name, Context& m_context, LocalContext& m_local_context)
	{
		// First, see if the function has already been added to the current module.
		if (auto* F = m_local_context.LLVM_Module->getFunction(Name))
			return F;

		// If not, check whether we can codegen the declaration from some existing
		// prototype.
		auto FI = m_local_context.FunctionProtos.find(Name);
		if (FI != m_local_context.FunctionProtos.end())
		{
			auto val_ = FI->second->codegen(m_context, m_local_context);
			if (!val_)
				return val_.takeError();

			return static_cast<Function*>(*val_);
		}

		// If no existing prototype exists, return null.
		return make_error<CompileError>("prototype for function '"s + Name + "' not found"s);
	}

	AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, Type* var_type, Context& m_context, LocalContext& m_local_context)
	{
		IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(var_type, nullptr, VarName);
	}
	/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	/// the function.  This is used for mutable variables etc.
	AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, TypeID VarType, Context& m_context, LocalContext& m_local_context)
	{
		auto var_type = get_llvm_type(VarType, m_local_context);
		IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(var_type, nullptr, VarName);
	}

	Expected<Value*> NoOpAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
	}

	Expected<Value*> NumberExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		const auto llvm_type = get_llvm_type(type_, m_local_context);

		switch (type_)
		{
		case slljit::doubleTyID:
			return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(ValD));
			break;
		case slljit::boolTyID:
			return ConstantInt::get(llvm_type, APInt(llvm_type->getIntegerBitWidth(), ValB, isSigned(type_)));
			break;
		case slljit::int64TyID:
			return ConstantInt::get(llvm_type, APInt(llvm_type->getIntegerBitWidth(), ValSI64, isSigned(type_)));
			break;
		}
	}

	Expected<Value*> VariableExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Look this variable up in the function.
		GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(Name);
		if (gVar)
		{
			if (gVar->isConstant())
				return m_local_context.LLVM_Builder->CreateLoad(gVar->getValueType(), gVar, Name);
			else
			{
				// TODO: load a single time
				auto ptr     = m_local_context.LLVM_Builder->CreateLoad(gVar->getValueType(), gVar, Name);
				auto valType = static_cast<PointerType*>(gVar->getValueType())->getElementType();
				return m_local_context.LLVM_Builder->CreateLoad(valType, ptr, Name);
			}
		}

		auto V = m_local_context.NamedValues[Name];
		if (!V)
			return make_error<CompileError>("unknown variable name: "s + Name);

		// Load the value.
		return m_local_context.LLVM_Builder->CreateLoad(V->getAllocatedType(), V, Name.c_str());
	}

	Expected<Value*> UnaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto OperandV = Operand->codegen(m_context, m_local_context);
		if (!OperandV)
			return OperandV.takeError();

		auto F = getFunction(std::string("unary") + Opcode, m_context, m_local_context);
		if (!F)
			return F.takeError();

		return m_local_context.LLVM_Builder->CreateCall(*F, *OperandV, "unop");
	}

	Expected<Value*> ReturnExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		if (auto RetVal = Operand->codegen(m_context, m_local_context))
		{
			const auto OperandTy = Operand->getType();
			CreateExplictCast(*RetVal, OperandTy, this->getType(), m_local_context);
			// Finish off the function.
			m_local_context.LLVM_Builder->CreateRet(*RetVal);

			return *RetVal;
		}
		else
			return RetVal.takeError();
	}

	Expected<Value*> CallExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Look up the name in the global module table.
		auto CalleeF = getFunction(Callee, m_context, m_local_context);
		if (!CalleeF)
			return CalleeF.takeError();

		// If argument mismatch error.
		if ((*CalleeF)->arg_size() != Args.size())
			return make_error<CompileError>("Incorrect # arguments passed");

		std::vector<Value*> ArgsV;
		for (const auto& [arg_, arg_type_expected] : zip(Args, ArgTypes))
		{
			auto val_ = arg_->codegen(m_context, m_local_context);
			if (!val_)
				return val_.takeError();
			CreateExplictCast(*val_, arg_->getType(), arg_type_expected, m_local_context);
			ArgsV.push_back(*val_);
		}

		return m_local_context.LLVM_Builder->CreateCall(*CalleeF, ArgsV, "calltmp");
	}

	Expected<Value*> IfExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = m_local_context.LLVM_Builder->GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the
		// end of the function.
		//	BasicBlock* cont_calcBB = BasicBlock::Create(*m_local_context.LLVM_Context, "cond_calc", TheFunction);
		BasicBlock* IfBB    = BasicBlock::Create(*m_local_context.LLVM_Context, "if", TheFunction);
		BasicBlock* ElseBB  = BasicBlock::Create(*m_local_context.LLVM_Context, "else", TheFunction);
		BasicBlock* MergeBB = BasicBlock::Create(*m_local_context.LLVM_Context, "after", TheFunction);

		//	m_local_context.LLVM_Builder->CreateBr(cont_calcBB);

		//	m_local_context.LLVM_Builder->SetInsertPoint(cont_calcBB);

		auto CondV = Cond->codegen(m_context, m_local_context);
		if (!CondV)
			return CondV.takeError();

		//TODO: verify
		CreateExplictCast(*CondV, Cond->getType(), boolTyID, m_local_context);
		//CondV = m_local_context.LLVM_Builder->CreateFCmpONE(CondV, ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0)), "ifcond");

		m_local_context.LLVM_Builder->CreateCondBr(*CondV, IfBB, ElseBB);

		// Emit then value.
		m_local_context.LLVM_Builder->SetInsertPoint(IfBB);

		{
			bool isTerminated = false;
			for (auto& then_expr : Then)
			{
				auto the_ = then_expr->codegen(m_context, m_local_context);
				if (!the_)
					return the_;

				if (then_expr->bIsTerminator())
				{
					isTerminated = true;
					break;
				}
			}

			if (!isTerminated)
				m_local_context.LLVM_Builder->CreateBr(MergeBB);
		}
		// Codegen of 'Then' can change the current block, update IfBB for the PHI.
		//	IfBB = m_local_context.LLVM_Builder->GetInsertBlock();

		// Emit else block.
		//	TheFunction->getBasicBlockList().push_back(ElseBB);
		m_local_context.LLVM_Builder->SetInsertPoint(ElseBB);

		{
			bool isTerminated = false;
			for (auto& else_expr : Else)
			{
				auto ee_ = else_expr->codegen(m_context, m_local_context);
				if (!ee_)
					return ee_;

				if (else_expr->bIsTerminator())
				{
					isTerminated = true;
					break;
				}
			}

			//	Value* ElseV = Else->codegen(m_context, m_local_context);
			//	if (!ElseV)
			//	return nullptr;
			if (!isTerminated)
				m_local_context.LLVM_Builder->CreateBr(MergeBB);
		}
		// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
		//	ElseBB = m_local_context.LLVM_Builder->GetInsertBlock();

		// Emit merge block.
		//	TheFunction->getBasicBlockList().push_back(MergeBB);
		m_local_context.LLVM_Builder->SetInsertPoint(MergeBB);
		//	PHINode* PN = LLVM_Builder.CreatePHI(Type::getDoubleTy(LLVM_Context), 2, "iftmp");

		//	PN->addIncoming(ThenV, IfBB);
		//	PN->addIncoming(ElseV, ElseBB);
		return *CondV;
	}

	// Output for-loop as:
	//   g_var = alloca double
	//   ...
	//   start = startexpr
	//   store start -> g_var
	//   goto loop
	// loop:
	//   ...
	//   bodyexpr
	//   ...
	// loopend:
	//   step = stepexpr
	//   endcond = endexpr
	//
	//   curvar = load g_var
	//   nextvar = curvar + step
	//   store nextvar -> g_var
	//   br endcond, loop, endloop
	// outloop:

	Expected<Value*> ForExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = m_local_context.LLVM_Builder->GetInsertBlock()->getParent();

		// generate init variable
		if (VarInit)
		{
			auto init_ = VarInit->codegen(m_context, m_local_context);
			if (!init_)
				return init_.takeError();
		}

		// Make the new basic block for the loop header, inserting after current
		// block.
		BasicBlock* LoopBB = BasicBlock::Create(*m_local_context.LLVM_Context, "loop_header", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* LoopBobyBB = BasicBlock::Create(*m_local_context.LLVM_Context, "loop_body", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* AfterBB = BasicBlock::Create(*m_local_context.LLVM_Context, "afterloop", TheFunction);

		// Insert an explicit fall through from the current block to the LoopBB.
		m_local_context.LLVM_Builder->CreateBr(LoopBB);

		// Start insertion in LoopBB.
		m_local_context.LLVM_Builder->SetInsertPoint(LoopBB);

		if (!Cond->bIsNoOp())
		{
			auto condVal = Cond->codegen(m_context, m_local_context);
			if (!condVal)
				return condVal.takeError();

			//TODO: verify
			CreateExplictCast(*condVal, Cond->getType(), boolTyID, m_local_context);
			// Convert condition to a bool by comparing non-equal to 0.0.
			//condVal = m_local_context.LLVM_Builder->CreateFCmpONE(condVal, ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0)), "loopcond");

			// Insert the conditional branch into the end of LoopEndBB.
			m_local_context.LLVM_Builder->CreateCondBr(*condVal, LoopBobyBB, AfterBB);
		}

		m_local_context.LLVM_Builder->SetInsertPoint(LoopBobyBB);

		for (auto& expt : Body)
		{
			auto e_ = expt->codegen(m_context, m_local_context);
			if (!e_)
				return e_.takeError();
		}

		auto end_e_ = EndExpr->codegen(m_context, m_local_context);
		if (!end_e_)
			return end_e_.takeError();

		// Insert unconditional brnch to start
		m_local_context.LLVM_Builder->CreateBr(LoopBB);

		// Any new code will be inserted in AfterBB.
		m_local_context.LLVM_Builder->SetInsertPoint(AfterBB);
		//	LLVM_Builder.CreateFCmpONE(ConstantFP::get(LLVM_Context, APFloat(0.0)), ConstantFP::get(LLVM_Context, APFloat(0.0)), "dmm");

		// for expr always returns 0.0.
		return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
	}

	Expected<Value*> VarExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		std::vector<AllocaInst*> OldBindings;

		Function* TheFunction = m_local_context.LLVM_Builder->GetInsertBlock()->getParent();

		// Register all variables and emit their initializer.
		Value* retval = nullptr;
		for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		{
			const std::string& VarName = VarNames[i].first;
			ExprAST* Init              = VarNames[i].second.get();

			// Emit the initializer before adding the variable to scope, this prevents
			// the initializer from referencing the variable itself, and permits stuff
			// like this:
			//  g_var a = 1 in
			//    g_var a = a in ...   # refers to outer 'a'.
			Value* InitVal;
			auto InitVal_type = none;
			if (Init)
			{
				auto InitVal_ = Init->codegen(m_context, m_local_context);
				if (!InitVal_)
					return nullptr;

				InitVal = *InitVal_;

				InitVal_type = Init->getType();
			}
			else
			{ // If not specified, use 0.0.
				InitVal      = ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
				InitVal_type = doubleTyID;
			}

			CreateExplictCast(InitVal, InitVal_type, type_, m_local_context);

			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName, type_, m_context, m_local_context);
			retval             = m_local_context.LLVM_Builder->CreateStore(InitVal, Alloca);

			// Remember the old variable binding so that we can restore the binding when
			// we unrecurse.
			OldBindings.push_back(m_local_context.NamedValues[VarName]);

			// Remember this binding.
			m_local_context.NamedValues[VarName] = Alloca;
		}

		// Codegen the body, now that all vars are in scope.
		//	Value* BodyVal = Body->codegen(m_context, m_local_context);
		//	if (!BodyVal)
		//	return nullptr;

		// Pop all our variables from scope.
		//	for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		//	NamedValues[VarNames[i].first] = OldBindings[i];

		// this is assignment operation
		return retval;
	}

	// Cast to Function*
	// Cast to Function*

	Expected<Value*> PrototypeAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		const auto is_main = isMain();

		// Make the function type:  double(double,double) etc.
		std::vector<Type*> Arguments;

		if (is_main)
		{
			Arguments.reserve(1);

			std::vector<Type*> StructMembers; // layout structure
			StructMembers.reserve(m_local_context.layout.globals.size());

			// Insert variable globals
			for (auto& global : m_local_context.layout.globals)
			{
				Type* type_ = get_llvm_type(TypeID(global.type), m_local_context);
				StructMembers.emplace_back(type_);
			}

			auto loader_struct_type = StructType::create(*m_local_context.LLVM_Context, StructMembers, "layout__");
			Arguments.emplace_back(loader_struct_type->getPointerTo());

			if (Arguments.size() != 1)
			{
				return make_error<CompileError>("Expected 1 argument for 'main', instead found: "s + to_string(Arguments.size()));
			}
		}
		else
		{
			Arguments.reserve(ArgTypes.size());

			for (auto arg_type : ArgTypes)
			{
				const auto llvm_arg_type = get_llvm_type(arg_type, m_local_context);
				Arguments.emplace_back(llvm_arg_type);
			}
		}

		const auto FnRetTyID     = this->ret_type_;
		const auto llvm_ret_type = get_llvm_type(FnRetTyID, m_local_context);
		FunctionType* FT         = FunctionType::get(llvm_ret_type, Arguments, false);

		const auto linkage_type = is_main ? Function::ExternalLinkage : Function::LinkageTypes::PrivateLinkage;

		Function* F = Function::Create(FT, linkage_type, Name, m_local_context.LLVM_Module.get());

		if (!is_main) // set argument names only for non main functions
		{
			// Set names for all arguments.
			unsigned Idx = 0;
			for (auto& Arg : F->args())
				Arg.setName(Args[Idx++]);
		}
		else
		{
			auto arg = F->arg_begin();
			arg->setName("data_ptr");
		}

		return F;
	}

	TypeID PrototypeAST::getRetType() const
	{
		return this->ret_type_;
	}

	std::vector<TypeID> PrototypeAST::getArgTypes() const
	{
		return ArgTypes;
	}

	bool PrototypeAST::isMain() const
	{
		return Name == "main";
	}

	bool PrototypeAST::match(string name /*, std::vector<TypeID> ArgTypes*/) const
	{
		return name == Name /*&& ArgTypes == this->ArgTypes*/;
	}

	const std::string& PrototypeAST::getName() const
	{
		return Name;
	}

	bool PrototypeAST::isUnaryOp() const
	{
		return IsOperator && Args.size() == 1;
	}

	bool PrototypeAST::isBinaryOp() const
	{
		return IsOperator && Args.size() == 2;
	}

	char PrototypeAST::getOperatorName() const
	{
		assert(isUnaryOp() || isBinaryOp());
		return Name[Name.size() - 1];
	}

	unsigned PrototypeAST::getBinaryPrecedence() const
	{
		return Precedence;
	}

	// Cast to Function*

	Expected<Value*> FunctionAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto TheFunction = getFunction(Proto.getName(), m_context, m_local_context);
		if (!TheFunction)
			return TheFunction.takeError();

		// If this is an operator, install it.
		if (Proto.isBinaryOp())
			m_local_context.BinopPrecedence[Proto.getOperatorName()] = Proto.getBinaryPrecedence();

		const auto is_main = isMain();

		if (is_main)
		{
			auto loader_struct_type = StructType::getTypeByName(*m_local_context.LLVM_Context, "layout__");

			// Set names for all arguments.
			auto data_pointer = (*TheFunction)->arg_begin();

			// Create a new basic block to start insertion into.
			BasicBlock* BB = BasicBlock::Create(*m_local_context.LLVM_Context, "entry", *TheFunction);
			m_local_context.LLVM_Builder->SetInsertPoint(BB);

			std::vector<Value*> indices{m_local_context.LLVM_Builder->getInt32(0), m_local_context.LLVM_Builder->getInt32(0)};
			uint32_t idx = 0;
			// Load offsets into variable globals
			for (auto& global : m_local_context.layout.globals)
			{
				auto g_var = m_local_context.LLVM_Module->getGlobalVariable(global.name);
				Value* gep = m_local_context.LLVM_Builder->CreateGEP(loader_struct_type, data_pointer, indices);
				m_local_context.LLVM_Builder->CreateStore(gep, g_var);
				idx++;
				indices[1] = m_local_context.LLVM_Builder->getInt32(idx);
			}
		}
		else
		{
			// Create a new basic block to start insertion into.
			BasicBlock* BB = BasicBlock::Create(*m_local_context.LLVM_Context, "entry", *TheFunction);
			m_local_context.LLVM_Builder->SetInsertPoint(BB);

			// Record the function arguments in the NamedValues map.
			m_local_context.NamedValues.clear();
			for (auto& Arg : (*TheFunction)->args())
			{
				// Create an alloca for this variable.
				AllocaInst* Alloca = CreateEntryBlockAlloca(*TheFunction, Arg.getName(), Arg.getType(), m_context, m_local_context);

				// Store the initial value into the alloca.
				m_local_context.LLVM_Builder->CreateStore(&Arg, Alloca);

				// Add arguments to variable symbol table.
				m_local_context.NamedValues[std::string(Arg.getName())] = Alloca;
			}
		}

		for (auto& expression : Body)
		{
			auto test = expression->codegen(m_context, m_local_context);
			if (!test)
			{
				// Error reading body, remove function.
				(*TheFunction)->eraseFromParent();

				if (Proto.isBinaryOp())
					m_local_context.BinopPrecedence.erase(Proto.getOperatorName());
				return test.takeError();
			}
		}

		verifyFunction(**TheFunction);

		return *TheFunction;
		//}
	}

	const std::string& FunctionAST::getName() const
	{
		return Proto.getName();
	}

	bool FunctionAST::isMain() const
	{
		return Proto.isMain();
	}
}; // namespace slljit
