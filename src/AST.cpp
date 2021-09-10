#include "pch.h"
#include "AST.h"

#include "Context.h"

using namespace llvm;
using namespace llvm::orc;
using namespace slljit;
using namespace std;

namespace slljit
{
	Value* BinaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
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
				return LogErrorV("destination of '=' must be a variable");
			// Codegen the RHS.
			Value* Val = RHS->codegen(m_context, m_local_context);
			if (!Val)
				return nullptr;

			// Look up global
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(LHSE->getName());
			if (gVar)
			{
				if (gVar->isConstant())
					return LogErrorV("Trying to store in constant global");

				auto ptr = m_local_context.LLVM_Builder->CreateLoad(gVar->getValueType(), gVar, LHSE->getName());
				//	Value* gep = m_local_context.LLVM_Builder->CreateGEP(Type::getDoubleTy(*m_local_context.LLVM_Context), ptr, m_local_context.LLVM_Builder->getInt32(0), "ptr_");
				m_local_context.LLVM_Builder->CreateStore(Val, ptr);
				return Val;
			}

			// Look up variable.
			Value* Variable = m_local_context.NamedValues[LHSE->getName()];
			if (!Variable)
				return LogErrorV("Unknown variable name");

			m_local_context.LLVM_Builder->CreateStore(Val, Variable);
			return Val;
		}

		Value* L = LHS->codegen(m_context, m_local_context);
		Value* R = RHS->codegen(m_context, m_local_context);
		if (!L || !R)
			return nullptr;

		switch (Op)
		{
		case '+': return m_local_context.LLVM_Builder->CreateFAdd(L, R, "addtmp");
		case '-': return m_local_context.LLVM_Builder->CreateFSub(L, R, "subtmp");
		case '*': return m_local_context.LLVM_Builder->CreateFMul(L, R, "multmp");
		case '/': return m_local_context.LLVM_Builder->CreateFDiv(L, R, "divtmp");
		case '<':
			L = m_local_context.LLVM_Builder->CreateFCmpULT(L, R, "cmptmp");
			// Convert bool 0/1 to double 0.0 or 1.0
			return m_local_context.LLVM_Builder->CreateUIToFP(L, Type::getDoubleTy(*m_local_context.LLVM_Context), "booltmp");
		case '>':
			L = m_local_context.LLVM_Builder->CreateFCmpUGT(L, R, "cmptmp");
			// Convert bool 0/1 to double 0.0 or 1.0
			return m_local_context.LLVM_Builder->CreateUIToFP(L, Type::getDoubleTy(*m_local_context.LLVM_Context), "booltmp");
		default: break;
		}

		// If it wasn't a builtin binary operator, it must be a user defined one. Emit
		// a call to it.
		Function* F = getFunction(std::string("binary") + Op, m_context, m_local_context);
		assert(F && "binary operator not found!");

		// TODO: verify this is safe
		ArrayRef<Value*> Ops = {L, R}; // Should be safe in this case
		return m_local_context.LLVM_Builder->CreateCall(F, Ops, "binop");
	}

	/// LogError* - These are little helper functions for error handling.
	std::unique_ptr<ExprAST> LogError(const char* Str)
	{
		fprintf(stderr, "Error: %s\n", Str);
		return nullptr;
	}

	std::unique_ptr<PrototypeAST> LogErrorP(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	std::unique_ptr<FunctionAST> LogErrorF(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	ExprList LogErrorEX(const char* Str)
	{
		LogError(Str);
		return {};
	}

	Value* LogErrorV(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	Function* getFunction(std::string Name, Context& m_context, LocalContext& m_local_context)
	{
		// First, see if the function has already been added to the current module.
		if (auto* F = m_local_context.LLVM_Module->getFunction(Name))
			return F;

		// If not, check whether we can codegen the declaration from some existing
		// prototype.
		auto FI = m_local_context.FunctionProtos.find(Name);
		if (FI != m_local_context.FunctionProtos.end())
			return static_cast<Function*>(FI->second->codegen(m_context, m_local_context));

		// If no existing prototype exists, return null.
		return nullptr;
	}

	/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	/// the function.  This is used for mutable variables etc.
	AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, Context& m_context, LocalContext& m_local_context)
	{
		IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(Type::getDoubleTy(*m_local_context.LLVM_Context), nullptr, VarName);
	}

	Value* NoOpAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
	}

	Value* NumberExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(Val));
	}

	Value* VariableExprAST::codegen(Context& m_context, LocalContext& m_local_context)
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
			return LogErrorV("Unknown variable name");

		// Load the value.
		return m_local_context.LLVM_Builder->CreateLoad(V->getAllocatedType(), V, Name.c_str());
	}

	Value* UnaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Value* OperandV = Operand->codegen(m_context, m_local_context);
		if (!OperandV)
			return nullptr;

		Function* F = getFunction(std::string("unary") + Opcode, m_context, m_local_context);
		if (!F)
			return LogErrorV("Unknown unary operator");

		return m_local_context.LLVM_Builder->CreateCall(F, OperandV, "unop");
	}

	Value* ReturnExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		if (Value* RetVal = Operand->codegen(m_context, m_local_context))
		{
			// Finish off the function.
			m_local_context.LLVM_Builder->CreateRet(RetVal);

			return RetVal;
		}
		return nullptr;
	}

	Value* CallExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Look up the name in the global module table.
		Function* CalleeF = getFunction(Callee, m_context, m_local_context);
		if (!CalleeF)
			return LogErrorV("Unknown function referenced");

		// If argument mismatch error.
		if (CalleeF->arg_size() != Args.size())
			return LogErrorV("Incorrect # arguments passed");

		std::vector<Value*> ArgsV;
		for (unsigned i = 0, e = Args.size(); i != e; ++i)
		{
			ArgsV.push_back(Args[i]->codegen(m_context, m_local_context));
			if (!ArgsV.back())
				return nullptr;
		}

		return m_local_context.LLVM_Builder->CreateCall(CalleeF, ArgsV, "calltmp");
	}

	Value* IfExprAST::codegen(Context& m_context, LocalContext& m_local_context)
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

		Value* CondV = Cond->codegen(m_context, m_local_context);
		if (!CondV)
			return nullptr;

		// Convert condition to a bool by comparing non-equal to 0.0.
		CondV = m_local_context.LLVM_Builder->CreateFCmpONE(CondV, ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0)), "ifcond");

		m_local_context.LLVM_Builder->CreateCondBr(CondV, IfBB, ElseBB);

		// Emit then value.
		m_local_context.LLVM_Builder->SetInsertPoint(IfBB);

		{
			bool isTerminated = false;
			for (auto& then_expr : Then)
			{
				then_expr->codegen(m_context, m_local_context);
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
				else_expr->codegen(m_context, m_local_context);
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
		return CondV;
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

	Value* ForExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = m_local_context.LLVM_Builder->GetInsertBlock()->getParent();

		// generate init variable
		if (VarInit)
			VarInit->codegen(m_context, m_local_context);

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
			Value* condVal = Cond->codegen(m_context, m_local_context);
			// Convert condition to a bool by comparing non-equal to 0.0.
			condVal = m_local_context.LLVM_Builder->CreateFCmpONE(condVal, ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0)), "loopcond");

			// Insert the conditional branch into the end of LoopEndBB.
			m_local_context.LLVM_Builder->CreateCondBr(condVal, LoopBobyBB, AfterBB);
		}

		m_local_context.LLVM_Builder->SetInsertPoint(LoopBobyBB);

		for (auto& expt : Body)
		{
			expt->codegen(m_context, m_local_context);
		}

		EndExpr->codegen(m_context, m_local_context);

		// Insert unconditional brnch to start
		m_local_context.LLVM_Builder->CreateBr(LoopBB);

		// Any new code will be inserted in AfterBB.
		m_local_context.LLVM_Builder->SetInsertPoint(AfterBB);
		//	LLVM_Builder.CreateFCmpONE(ConstantFP::get(LLVM_Context, APFloat(0.0)), ConstantFP::get(LLVM_Context, APFloat(0.0)), "dmm");

		// for expr always returns 0.0.
		return ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
	}

	Value* VarExprAST::codegen(Context& m_context, LocalContext& m_local_context)
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
			if (Init)
			{
				InitVal = Init->codegen(m_context, m_local_context);
				if (!InitVal)
					return nullptr;
			}
			else
			{ // If not specified, use 0.0.
				InitVal = ConstantFP::get(*m_local_context.LLVM_Context, APFloat(0.0));
			}

			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName, m_context, m_local_context);
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

	Value* PrototypeAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Make the function type:  double(double,double) etc.
		std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*m_local_context.LLVM_Context));
		FunctionType* FT = FunctionType::get(Type::getDoubleTy(*m_local_context.LLVM_Context), Doubles, false);

		Function* F = Function::Create(FT, Function::ExternalLinkage, Name, m_local_context.LLVM_Module.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : F->args())
			Arg.setName(Args[Idx++]);

		return F;
	}

	// Cast to Function*
	// Cast to Function*

	Value* FunctionAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = getFunction(Proto.getName(), m_context, m_local_context);
		if (!TheFunction)
			return nullptr;

		// If this is an operator, install it.
		if (Proto.isBinaryOp())
			m_local_context.BinopPrecedence[Proto.getOperatorName()] = Proto.getBinaryPrecedence();

		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(*m_local_context.LLVM_Context, "entry", TheFunction);
		m_local_context.LLVM_Builder->SetInsertPoint(BB);

		// Record the function arguments in the NamedValues map.
		m_local_context.NamedValues.clear();
		for (auto& Arg : TheFunction->args())
		{
			// Create an alloca for this variable.
			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), m_context, m_local_context);

			// Store the initial value into the alloca.
			m_local_context.LLVM_Builder->CreateStore(&Arg, Alloca);

			// Add arguments to variable symbol table.
			m_local_context.NamedValues[std::string(Arg.getName())] = Alloca;
		}

		for (auto& expression : Body)
		{
			auto test = expression->codegen(m_context, m_local_context);
			if (!test)
			{
				// Error reading body, remove function.
				TheFunction->eraseFromParent();

				if (Proto.isBinaryOp())
					m_local_context.BinopPrecedence.erase(Proto.getOperatorName());
				return nullptr;
			}
		}

		verifyFunction(*TheFunction);

		return TheFunction;
		//}
	}

	const std::string& FunctionAST::getName() const
	{
		return Proto.getName();
	}
}; // namespace slljit
