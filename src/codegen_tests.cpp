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
static std::unique_ptr<ShaderJIT> TheJIT;

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, llvm::Type* type = Type::getDoubleTy(TheContext))
{
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(type, nullptr, VarName);
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

	TheFPM->add(createSLPVectorizerPass());

	TheFPM->add(createLoadStoreVectorizerPass());

	TheFPM->add(createLoopVectorizePass());

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

void GenerateFunc_0()
{
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
		NamedValues[std::string(Arg.getName())] = Alloca;
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

	//	Value* RetVal = Body->codegen();

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
}

void GenerateFunc_1()
{
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
		NamedValues[std::string(Arg.getName())] = Alloca;
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
		Value* result        = Builder.CreateFAdd(LHS, RHS, "addtmp");
		Builder.CreateStore(result, Alloca);
		retval = result;
	}

	//	Value* RetVal = Body->codegen();

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
}

struct data_test
{
	double b;
	__int32 a;
	
};

void GenerateFunc_2()
{
	std::string Globalname = "global_x_ptr";
	TheModule->getOrInsertGlobal(Globalname, Type::getInt32Ty(TheContext));
	GlobalVariable* gVar = TheModule->getNamedGlobal(Globalname);
	gVar->setLinkage(GlobalValue::ExternalLinkage);
	gVar->setInitializer(ConstantInt::get(TheContext, APInt(32, 0)));
	//auto const_ = gVar->isConstant();
	//gVar->setAlignment(4);

	{
		// CODEGEN TESTS
		std::vector<std::string> Args{"base_ptr"};
		std::string Name{"loader"};

		// CODEGEN PTOTOTYPE
		std::vector<Type*> Doubles(Args.size(), Type::getInt8Ty(TheContext)->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getInt32Ty(TheContext), Doubles, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);

		// CODEGEN FUNCTION
		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(TheContext, "entry", TheFunction);
		Builder.SetInsertPoint(BB);

		// Get pointer to first argument
		Value* Base = TheFunction->arg_begin();

		//Value* Base_int = Builder.CreateBitCast(Base, Builder.getInt32Ty()->getPointerTo());
		//PointerType::get(Builder.getInt8Ty()->getPointerTo(), 0);

		// Get __int8* pointer Offset by Builder.getInt32(8) from Base
		Value* gep = Builder.CreateGEP(Builder.getInt8Ty(), Base, Builder.getInt32(8), "a1");
		// no-op bitcast from __int8* to __int32*
		Value* gep_int = Builder.CreateBitCast(gep, Builder.getInt32Ty()->getPointerTo());
		//gep_int->dump();
		//Load __int32 from pointer
		Value* load = Builder.CreateLoad(gep_int, "a1");
		// Store loaded value in global
		Builder.CreateStore(load, gVar);

		Value* retval = Builder.CreateLoad(gVar);

		//	Value* RetVal = Body->codegen();

		if (retval)
		{
			// Finish off the function.
			Builder.CreateRet(retval);

			// Validate the generated code, checking for consistency.
			verifyFunction(*TheFunction);

			// Run the optimizer on the function.
			TheFPM->run(*TheFunction);
		}

	}

	{
		// CODEGEN TESTS
		std::vector<std::string> Args{};
		std::string Name{"get_global"};

		// CODEGEN PTOTOTYPE
		std::vector<Type*> Doubles(Args.size(), Type::getInt8Ty(TheContext)->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getInt32Ty(TheContext), Doubles, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);

		// CODEGEN FUNCTION
		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(TheContext, "entry", TheFunction);
		Builder.SetInsertPoint(BB);

		Value* retval = Builder.CreateLoad(gVar);

		//	Value* RetVal = Body->codegen();

		if (retval)
		{
			// Finish off the function.
			Builder.CreateRet(retval);

			// Validate the generated code, checking for consistency.
			verifyFunction(*TheFunction);

			// Run the optimizer on the function.
			TheFPM->run(*TheFunction);
		}
	}


	return;
}

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


	TheJIT = std::make_unique<ShaderJIT>();

	InitializeModuleAndPassManager();

	GenerateFunc_0();

	TheJIT->addModule(std::move(TheModule));
	InitializeModuleAndPassManager();

	// CALL FUNCTIONSIN CPP
	auto ExprSymbol = TheJIT->findSymbol("main");
	assert(ExprSymbol && "Function not found");

	// Get the symbol's address and cast it to the right type (takes no
	// arguments, returns a double) so we can call it as a native function.
	double (*FP)(double a) = (double (*)(double a))(intptr_t)cantFail(ExprSymbol.getAddress());


	InitializeModuleAndPassManager();

	GenerateFunc_1();

	TheJIT->addModule(std::move(TheModule));
	InitializeModuleAndPassManager();

	// CALL FUNCTIONSIN CPP
	ExprSymbol = TheJIT->findSymbol("main");
	assert(ExprSymbol && "Function not found");

	double (*FP_)(double a) = (double (*)(double a))(intptr_t)cantFail(ExprSymbol.getAddress());
	fprintf(stderr, "Evaluated to %f\n", FP(2));
	auto t  = FP(0);
	auto t1 = FP(5);
	fprintf(stderr, "Evaluated to %f\n", FP_(2));
	t  = FP_(0);
	t1 = FP_(5);

	InitializeModuleAndPassManager();

	GenerateFunc_2();
	TheModule->dump();
	TheJIT->addModule(std::move(TheModule));
	InitializeModuleAndPassManager();

	auto ExprSymbol_gptr = TheJIT->findSymbol("global_x_ptr");
	assert(ExprSymbol_gptr && "Function not found");

	auto ExprSymbol_ptr = TheJIT->findSymbol("loader");
	assert(ExprSymbol_ptr && "Function not found");

	data_test d{1, 2};
	//auto* tmp                = &d; 
	//auto tmp__               = reinterpret_cast<double*>(tmp);
	__int32 (*_FP__ptr)(__int8* base_ptr) = (__int32 (*)(__int8* base_ptr))(intptr_t)cantFail(ExprSymbol_ptr.getAddress());
	auto t_fp_ptr                         = _FP__ptr(reinterpret_cast<__int8*>(&d));

	auto ExprSymbol_get_global_ptr = TheJIT->findSymbol("get_global");
	assert(ExprSymbol_get_global_ptr && "Function not found");

	__int32 (*_FP__get_global)() = (__int32 (*)())(intptr_t)cantFail(ExprSymbol_get_global_ptr.getAddress());
	auto t_fp_global_ptr                = _FP__get_global();

	auto global_x_ptr = (_int32*)cantFail(ExprSymbol_gptr.getAddress());

	*global_x_ptr = 4;

	t_fp_global_ptr = _FP__get_global();

	// Beware: exiting in debug mode triggers assert.
	return 0;
}