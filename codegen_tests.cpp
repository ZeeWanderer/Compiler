// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "JIT.h"

using namespace llvm;
using namespace llvm::orc;

static std::map<char, int> BinopPrecedence;

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst*> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<shader_JIT> TheJIT;

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName)
{
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
}

Function* getFunction(std::string Name)
{
	// First, see if the function has already been added to the current module.
	if (auto* F = TheModule->getFunction(Name))
		return F;

	//// If not, check whether we can codegen the declaration from some existing
	//// prototype.
	//auto FI = FunctionProtos.find(Name);
	//if (FI != FunctionProtos.end())
	//	return FI->second->codegen();

	// If no existing prototype exists, return null.
	return nullptr;
}

static void InitializeModuleAndPassManager()
{
	// Open a new module.
	TheModule = std::make_unique<Module>("my cool jit", TheContext);
	TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());

	// Promote allocas to registers.
	TheFPM->add(createPromoteMemoryToRegisterPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->add(createInstructionCombiningPass());
	// Reassociate expressions.
	TheFPM->add(createReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->add(createCFGSimplificationPass());

	TheFPM->doInitialization();
}


//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X)
{
	fputc((char)X, stderr);
	return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X)
{
	fprintf(stderr, "%f\n", X);
	return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['='] = 2;
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40; // highest.


	TheJIT = std::make_unique<shader_JIT>();

	InitializeModuleAndPassManager();

	// CODEGEN TESTS
	std::vector<std::string> Args{"x"};
	std::string Name{"main"};

	// CODEGEN PTOTOTYPE
	std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(TheContext));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

	Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : TheFunction->args())
		Arg.setName(Args[Idx++]);

	// CODEGEN FUNCTION
	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : TheFunction->args())
	{
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

		// Store the initial value into the alloca.
		Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[Arg.getName()] = Alloca;
	}
	std::vector<std::string> VarNames{"x", "y"};
	{
		std::string VarName = VarNames[0];
		auto InitVarName    = Args[0];
		Value* InitAlloca   = NamedValues[InitVarName];
		Value* InitVal      = Builder.CreateLoad(InitAlloca, InitVarName.c_str());
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = Builder.CreateLoad(V, VarName.c_str());
		Value* RHS           = ConstantFP::get(TheContext, APFloat(5.0));
		Value* result        = Builder.CreateFAdd(LHS, RHS, "addtmp");
		Builder.CreateStore(result, Alloca);
	}

	Value* retval;
	{
		std::string VarName = VarNames[1];
		Value* InitVal      = ConstantFP::get(TheContext, APFloat(2.0));
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = Builder.CreateLoad(V, VarName.c_str());
		Value* V_rhs         = NamedValues[VarNames[0]];
		Value* RHS           = Builder.CreateLoad(V_rhs, VarNames[0].c_str());
		Value* result        = Builder.CreateFMul(LHS, RHS, "multmp");
		Builder.CreateStore(result, Alloca);
		retval = result;
	}

	//Value* RetVal = Body->codegen();

	if (retval)
	{
		// Finish off the function.
		Builder.CreateRet(retval);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		// Run the optimizer on the function.
		TheFPM->run(*TheFunction);
	}

	fprintf(stderr, "Read function definition:");
	TheFunction->print(errs());
	fprintf(stderr, "\n");
	// ADD FUNCTION MODULE AND INIT NEW MODULE
	TheJIT->addModule(std::move(TheModule));
	InitializeModuleAndPassManager();

	// CALL FUNCTIONSIN CPP
	auto ExprSymbol = TheJIT->findSymbol(Name);
	assert(ExprSymbol && "Function not found");

	// Get the symbol's address and cast it to the right type (takes no
	// arguments, returns a double) so we can call it as a native function.
	double (*FP)(double a) = (double (*)(double a))(intptr_t)cantFail(ExprSymbol.getAddress());
	fprintf(stderr, "Evaluated to %f\n", FP(2));
	auto t = FP(0);
	auto t1 = FP(5);


	// Beware: exiting in debug mode triggers assert.
	return 0;
}