#include "pch.h"
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
	// const map<std::pair<llvm::Type::TypeID, llvm::Type::TypeID>, llvm::Instruction::CastOps> llvm_types_to_cast_op = {{{Type::DoubleTyID, Type::IntegerTyID}, Instruction::FPToSI}, {{Type::IntegerTyID, Type::DoubleTyID}, Instruction::SIToFP}};

	Expected<Value*> BinaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& context = m_local_context.getContext();
		auto& builder = m_local_context.getBuilder();
		auto& module  = m_local_context.getModule();

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
			GlobalVariable* gVar = module.getNamedGlobal(LHSE->getName());
			if (gVar)
			{
				if (gVar->isConstant())
					return make_error<CompileError>("Trying to store in constant global");

				auto ptr = builder.CreateLoad(gVar->getValueType(), gVar, LHSE->getName());
				//	Value* gep = m_local_context.LLVM_Builder->CreateGEP(Type::getDoubleTy(*m_local_context.LLVM_Context), ptr, m_local_context.LLVM_Builder->getInt32(0), "ptr_");
				builder.CreateStore(Val, ptr);
				return Val;
			}

			// Look up variable.

			auto Variable = m_local_context.find_var_in_scope(LHSE->getName());
			if (!Variable)
				return Variable.takeError();
			auto VariableTy = LHSE->getType();

			// if types differ cast val to variable type
			CreateExplictCast(Val, ValTy, VariableTy, m_local_context);

			builder.CreateStore(Val, *Variable);
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
		return builder.CreateCall(*F, Ops, "binop");
	}

	Expected<Function*> getFunction(std::string Name, Context& m_context, LocalContext& m_local_context)
	{
		// First, see if the function has already been added to the current module.
		if (auto* F = m_local_context.getModule().getFunction(Name))
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
		return ConstantFP::get(m_local_context.getContext(), APFloat(0.0));
	}

	Expected<Value*> NumberExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		const auto llvm_type = get_llvm_type(type_, m_local_context);

		switch (type_)
		{
		case slljit::doubleTyID:
			return ConstantFP::get(m_local_context.getContext(), APFloat(ValD));
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
		auto& builder = m_local_context.getBuilder();
		auto& module  = m_local_context.getModule();
		auto& layout  = m_local_context.getLayout();

		// Look this variable up in the function.
		GlobalVariable* gVar = module.getNamedGlobal(Name);
		if (gVar)
		{
			const auto gVarValueType = gVar->getValueType();

			if (gVar->isConstant())
				return builder.CreateLoad(gVarValueType, gVar, Name);
			else
			{
				auto g_result = m_local_context.find_global(gVar->getName().str());
				[[likely]] if (g_result)
				{
					// TODO: load a single time
					auto ptr     = builder.CreateLoad(gVarValueType, gVar, Name);
					auto valType = get_llvm_type(TypeID(g_result->type), m_local_context);
					// auto valType_ = static_cast<PointerType*>(gVar->getValueType())->getElementType();
					return builder.CreateLoad(valType, ptr, Name);
				}
				else
				{
					consumeError(g_result.takeError());

					return builder.CreateLoad(gVarValueType, gVar, Name);
				}
			}
		}

		auto V = m_local_context.find_var_in_scope(Name);
		if (!V)
			return V.takeError();

		// Load the value.
		return builder.CreateLoad((*V)->getAllocatedType(), *V, Name.c_str());
	}

	Expected<Value*> UnaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& builder = m_local_context.getBuilder();

		auto OperandV = Operand->codegen(m_context, m_local_context);
		if (!OperandV)
			return OperandV.takeError();

		auto F = getFunction(std::string("unary") + Opcode, m_context, m_local_context);
		if (!F)
			return F.takeError();

		return builder.CreateCall(*F, *OperandV, "unop");
	}

	Expected<Value*> ReturnExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& builder = m_local_context.getBuilder();

		if (auto RetVal = Operand->codegen(m_context, m_local_context))
		{
			const auto OperandTy = Operand->getType();
			CreateExplictCast(*RetVal, OperandTy, this->getType(), m_local_context);
			// Finish off the function.
			builder.CreateRet(*RetVal);

			return *RetVal;
		}
		else
			return RetVal.takeError();
	}

	Expected<Value*> CallExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& builder = m_local_context.getBuilder();

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

		return builder.CreateCall(*CalleeF, ArgsV, "calltmp");
	}

	Expected<Value*> IfExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& context = m_local_context.getContext();
		auto& builder = m_local_context.getBuilder();

		Function* TheFunction = builder.GetInsertBlock()->getParent();
		m_local_context.push_scope();

		// Create blocks for the then and else cases.  Insert the 'then' block at the
		// end of the function.
		BasicBlock* IfBB    = BasicBlock::Create(context, "if", TheFunction);
		BasicBlock* ElseBB  = BasicBlock::Create(context, "else", TheFunction);
		BasicBlock* MergeBB = BasicBlock::Create(context, "after", TheFunction);

		auto CondV = Cond->codegen(m_context, m_local_context);
		if (!CondV)
			return CondV.takeError();

		CreateExplictCast(*CondV, Cond->getType(), boolTyID, m_local_context);

		builder.CreateCondBr(*CondV, IfBB, ElseBB);

		// Emit then value.
		builder.SetInsertPoint(IfBB);

		{
			m_local_context.push_scope();
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
				builder.CreateBr(MergeBB);
			m_local_context.pop_scope();
		}

		// Emit else block.
		builder.SetInsertPoint(ElseBB);

		{
			m_local_context.push_scope();
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

			if (!isTerminated)
				builder.CreateBr(MergeBB);
			m_local_context.pop_scope();
		}

		builder.SetInsertPoint(MergeBB);

		m_local_context.pop_scope();
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
		auto& context = m_local_context.getContext();
		auto& builder = m_local_context.getBuilder();

		Function* TheFunction = builder.GetInsertBlock()->getParent();
		m_local_context.push_scope();

		// generate init variable
		if (VarInit)
		{
			auto init_ = VarInit->codegen(m_context, m_local_context);
			if (!init_)
				return init_.takeError();
		}

		// Make the new basic block for the loop header, inserting after current
		// block.
		BasicBlock* LoopBB = BasicBlock::Create(context, "loop_header", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* LoopBobyBB = BasicBlock::Create(context, "loop_body", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* AfterBB = BasicBlock::Create(context, "afterloop", TheFunction);

		// Insert an explicit fall through from the current block to the LoopBB.
		builder.CreateBr(LoopBB);

		// Start insertion in LoopBB.
		builder.SetInsertPoint(LoopBB);

		if (!Cond->bIsNoOp())
		{
			auto condVal = Cond->codegen(m_context, m_local_context);
			if (!condVal)
				return condVal.takeError();

			// TODO: verify
			CreateExplictCast(*condVal, Cond->getType(), boolTyID, m_local_context);
			// Convert condition to a bool by comparing non-equal to 0.0.
			// condVal = m_local_context.LLVM_Builder->CreateFCmpONE(condVal, ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0)), "loopcond");

			// Insert the conditional branch into the end of LoopEndBB.
			builder.CreateCondBr(*condVal, LoopBobyBB, AfterBB);
		}

		builder.SetInsertPoint(LoopBobyBB);

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
		builder.CreateBr(LoopBB);

		// Any new code will be inserted in AfterBB.
		builder.SetInsertPoint(AfterBB);
		//	LLVM_Builder.CreateFCmpONE(ConstantFP::get(LLVM_Context, APFloat(0.0)), ConstantFP::get(LLVM_Context, APFloat(0.0)), "dmm");

		m_local_context.pop_scope();
		// for expr always returns 0.0.
		return ConstantFP::get(context, APFloat(0.0));
	}

	Expected<Value*> VarExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& context = m_local_context.getContext();
		auto& builder = m_local_context.getBuilder();

		Function* TheFunction = builder.GetInsertBlock()->getParent();

		// Register all variables and emit their initializer.
		Value* retval = nullptr;
		for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		{
			const std::string& VarName = VarNames[i].first;
			ExprAST* Init              = VarNames[i].second.get();

			if (m_local_context.check_curent_scope(VarName))
			{
				return make_error<CompileError>("variable `"s + VarName + "` redefinition");
			}

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
				InitVal      = ConstantFP::get(context, APFloat(0.0));
				InitVal_type = doubleTyID;
			}

			CreateExplictCast(InitVal, InitVal_type, type_, m_local_context);

			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName, type_, m_context, m_local_context);
			retval             = builder.CreateStore(InitVal, Alloca);

			m_local_context.push_var_into_scope(VarName, Alloca);
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

	Expected<Value*> PrototypeAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		auto& context = m_local_context.getContext();
		auto& module  = m_local_context.getModule();
		auto& layout  = m_local_context.getLayout();

		const auto is_main = isMain();

		// Make the function type:  double(double,double) etc.
		std::vector<Type*> Arguments;

		if (is_main)
		{
			Arguments.reserve(1);

			std::vector<Type*> StructMembers; // layout structure
			StructMembers.reserve(layout.globals.size());

			// Insert variable globals
			for (auto& global : layout.globals)
			{
				Type* type_ = get_llvm_type(TypeID(global.type), m_local_context);
				StructMembers.emplace_back(type_);
			}

			auto loader_struct_type = StructType::create(context, StructMembers, "layout__");
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

		Function* F = Function::Create(FT, linkage_type, Name, module);

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
		m_local_context.push_scope();

		auto& context = m_local_context.getContext();
		auto& builder = m_local_context.getBuilder();
		auto& module  = m_local_context.getModule();
		auto& layout  = m_local_context.getLayout();

		auto TheFunction = getFunction(Proto.getName(), m_context, m_local_context);
		if (!TheFunction)
			return TheFunction.takeError();

		// If this is an operator, install it.
		if (Proto.isBinaryOp())
			m_local_context.BinopPrecedence[Proto.getOperatorName()] = Proto.getBinaryPrecedence();

		const auto is_main = isMain();

		if (is_main)
		{
			auto loader_struct_type = StructType::getTypeByName(context, "layout__");

			// Set names for all arguments.
			auto data_pointer = (*TheFunction)->arg_begin();

			// Create a new basic block to start insertion into.
			BasicBlock* BB = BasicBlock::Create(context, "entry", *TheFunction);
			builder.SetInsertPoint(BB);

			std::vector<Value*> indices{builder.getInt32(0), builder.getInt32(0)};
			uint32_t idx = 0;
			// Load offsets into variable globals
			for (auto& global : layout.globals)
			{
				auto g_var = module.getGlobalVariable(global.name);
				Value* gep = builder.CreateGEP(loader_struct_type, data_pointer, indices);
				builder.CreateStore(gep, g_var);
				idx++;
				indices[1] = builder.getInt32(idx);
			}
		}
		else
		{
			// Create a new basic block to start insertion into.
			BasicBlock* BB = BasicBlock::Create(context, "entry", *TheFunction);
			builder.SetInsertPoint(BB);

			// Record the function arguments in the NamedValues map.
			for (auto& Arg : (*TheFunction)->args())
			{
				// Create an alloca for this variable.
				AllocaInst* Alloca = CreateEntryBlockAlloca(*TheFunction, Arg.getName(), Arg.getType(), m_context, m_local_context);

				// Store the initial value into the alloca.
				builder.CreateStore(&Arg, Alloca);

				// Add arguments to variable symbol table.
				m_local_context.push_var_into_scope(Arg.getName().str(), Alloca);
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

		m_local_context.pop_scope();
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
