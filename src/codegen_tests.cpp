// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "JIT.h"

using namespace llvm;
using namespace llvm::orc;
using namespace slljit;


static std::map<char, int> BinopPrecedence;

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext LLVM_Context;
static IRBuilder<> LLVM_Builder(LLVM_Context);
static std::unique_ptr<Module> LLVM_Module;
static std::map<std::string, AllocaInst*> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> LLVM_FPM;
static std::unique_ptr<ShaderJIT> shllJIT;

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, llvm::Type* type = Type::getDoubleTy(LLVM_Context), Value* arrays = nullptr)
{
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(type, arrays, VarName);
}

Function* getFunction(std::string Name)
{
	// First, see if the function has already been added to the current module.
	if (auto* F = LLVM_Module->getFunction(Name))
		return F;

	//// If not, check whether we can codegen the declaration from some existing
	//// prototype.
	//	auto FI = FunctionProtos.find(Name);
	//	if (FI != FunctionProtos.end())
	//	return FI->second->codegen();

	// If no existing prototype exists, return null.
	return nullptr;
}

static void InitializeModuleAndPassManager()
{
	// Open a new module.
	LLVM_Module = std::make_unique<Module>("my cool jit", LLVM_Context);
	LLVM_Module->setDataLayout(shllJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	LLVM_FPM = std::make_unique<legacy::FunctionPassManager>(LLVM_Module.get());

	LLVM_FPM->add(createPromoteMemoryToRegisterPass()); //	SSA conversion
	LLVM_FPM->add(createCFGSimplificationPass());       //	Dead code elimination
	LLVM_FPM->add(createSROAPass());
	LLVM_FPM->add(createLoadStoreVectorizerPass());
	LLVM_FPM->add(createLoopSimplifyCFGPass());
	LLVM_FPM->add(createLoopVectorizePass());
	LLVM_FPM->add(createLoopUnrollPass());
	LLVM_FPM->add(createConstantPropagationPass());
	LLVM_FPM->add(createGVNPass());                     // Eliminate Common SubExpressions.
	LLVM_FPM->add(createNewGVNPass());                  //	Global value numbering
	LLVM_FPM->add(createReassociatePass());             // Reassociate expressions.
	LLVM_FPM->add(createPartiallyInlineLibCallsPass()); //	Inline standard calls
	LLVM_FPM->add(createDeadCodeEliminationPass());
	LLVM_FPM->add(createCFGSimplificationPass());    //	Cleanup
	LLVM_FPM->add(createInstructionCombiningPass()); // Do simple "peephole" optimizations and bit-twiddling optzns.
	LLVM_FPM->add(createSLPVectorizerPass());
	LLVM_FPM->add(createFlattenCFGPass()); //	Flatten the control flow graph.

	LLVM_FPM->doInitialization();
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
	std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(LLVM_Context));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(LLVM_Context), Doubles, false);

	Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, LLVM_Module.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : TheFunction->args())
		Arg.setName(Args[Idx++]);

	// CODEGEN FUNCTION
	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(LLVM_Context, "entry", TheFunction);
	LLVM_Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : TheFunction->args())
	{
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

		// Store the initial value into the alloca.
		LLVM_Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[std::string(Arg.getName())] = Alloca;
	}
	std::vector<std::string> VarNames{"x", "y"};
	{
		std::string VarName = VarNames[0];
		auto InitVarName    = Args[0];
		Value* InitAlloca   = NamedValues[InitVarName];
		Value* InitVal      = LLVM_Builder.CreateLoad(InitAlloca, InitVarName.c_str());
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		LLVM_Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = LLVM_Builder.CreateLoad(V, VarName.c_str());
		Value* RHS           = ConstantFP::get(LLVM_Context, APFloat(5.0));
		Value* result        = LLVM_Builder.CreateFAdd(LHS, RHS, "addtmp");
		LLVM_Builder.CreateStore(result, Alloca);
	}

	Value* retval;
	{
		std::string VarName = VarNames[1];
		Value* InitVal      = ConstantFP::get(LLVM_Context, APFloat(2.0));
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		LLVM_Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = LLVM_Builder.CreateLoad(V, VarName.c_str());
		Value* V_rhs         = NamedValues[VarNames[0]];
		Value* RHS           = LLVM_Builder.CreateLoad(V_rhs, VarNames[0].c_str());
		Value* result        = LLVM_Builder.CreateFMul(LHS, RHS, "multmp");
		LLVM_Builder.CreateStore(result, Alloca);
		retval = result;
	}

	//	Value* RetVal = Body->codegen();

	if (retval)
	{
		// Finish off the function.
		LLVM_Builder.CreateRet(retval);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		// Run the optimizer on the function.
		LLVM_FPM->run(*TheFunction);
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
	std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(LLVM_Context));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(LLVM_Context), Doubles, false);

	Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, LLVM_Module.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : TheFunction->args())
		Arg.setName(Args[Idx++]);

	// CODEGEN FUNCTION
	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(LLVM_Context, "entry", TheFunction);
	LLVM_Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : TheFunction->args())
	{
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

		// Store the initial value into the alloca.
		LLVM_Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[std::string(Arg.getName())] = Alloca;
	}
	std::vector<std::string> VarNames{"x", "y"};
	{
		std::string VarName = VarNames[0];
		auto InitVarName    = Args[0];
		Value* InitAlloca   = NamedValues[InitVarName];
		Value* InitVal      = LLVM_Builder.CreateLoad(InitAlloca, InitVarName.c_str());
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		LLVM_Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = LLVM_Builder.CreateLoad(V, VarName.c_str());
		Value* RHS           = ConstantFP::get(LLVM_Context, APFloat(5.0));
		Value* result        = LLVM_Builder.CreateFAdd(LHS, RHS, "addtmp");
		LLVM_Builder.CreateStore(result, Alloca);
	}

	Value* retval;
	{
		std::string VarName = VarNames[1];
		Value* InitVal      = ConstantFP::get(LLVM_Context, APFloat(2.0));
		AllocaInst* Alloca  = CreateEntryBlockAlloca(TheFunction, VarName);
		// INIT VARIBALE
		LLVM_Builder.CreateStore(InitVal, Alloca);
		// Remember this binding.
		NamedValues[VarName] = Alloca;
		Value* V             = NamedValues[VarName];
		Value* LHS           = LLVM_Builder.CreateLoad(V, VarName.c_str());
		Value* V_rhs         = NamedValues[VarNames[0]];
		Value* RHS           = LLVM_Builder.CreateLoad(V_rhs, VarNames[0].c_str());
		Value* result        = LLVM_Builder.CreateFAdd(LHS, RHS, "addtmp");
		LLVM_Builder.CreateStore(result, Alloca);
		retval = result;
	}

	//	Value* RetVal = Body->codegen();

	if (retval)
	{
		// Finish off the function.
		LLVM_Builder.CreateRet(retval);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		// Run the optimizer on the function.
		LLVM_FPM->run(*TheFunction);
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

static double test = 5;

void GenerateFunc_6()
{
	std::string Globalname = "global_x_ptr_ptr";
	LLVM_Module->getOrInsertGlobal(Globalname, Type::getDoublePtrTy(LLVM_Context));
	{

		GlobalVariable* gVar = LLVM_Module->getNamedGlobal(Globalname);
		gVar->setLinkage(GlobalValue::ExternalLinkage);
		gVar->setInitializer(ConstantPointerNull::get(Type::getDoublePtrTy(LLVM_Context)));
	}
	{
		LLVM_Module->getOrInsertGlobal("a", Type::getDoublePtrTy(LLVM_Context));
		GlobalVariable* gVar_ = LLVM_Module->getNamedGlobal("a");
		gVar_->setLinkage(GlobalValue::ExternalLinkage);
		gVar_->setInitializer(ConstantPointerNull::get(Type::getDoublePtrTy(LLVM_Context)));
	}
	{
		LLVM_Module->getOrInsertGlobal("b", Type::getDoublePtrTy(LLVM_Context));
		GlobalVariable* gVar_ = LLVM_Module->getNamedGlobal("b");
		gVar_->setLinkage(GlobalValue::ExternalLinkage);
		gVar_->setInitializer(ConstantPointerNull::get(Type::getDoublePtrTy(LLVM_Context)));
	}
	auto& g_list = LLVM_Module->getGlobalList();
	for (auto& var : g_list)
	{
		auto name = var.getName();
		name.size();
	}
	/*ConstantPointerNull::get(Type::getDoublePtrTy(LLVM_Context));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(LLVM_Context), nullptr, false);

	Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, "main_ptr_test", LLVM_Module.get());

	BasicBlock* BB = BasicBlock::Create(LLVM_Context, "entry", TheFunction);
	LLVM_Builder.SetInsertPoint(BB);
	auto ptr = LLVM_Builder.CreateLoad(gVar);
	auto constant = ConstantFP::get(LLVM_Context, APFloat(15.0));
	LLVM_Builder.CreateStore(constant, ptr);
	LLVM_Builder.CreateRet(constant);*/
}

void GenerateFunc_2()
{
	std::string Globalname = "global_x_ptr";
	LLVM_Module->getOrInsertGlobal(Globalname, Type::getInt32Ty(LLVM_Context));
	GlobalVariable* gVar = LLVM_Module->getNamedGlobal(Globalname);
	gVar->setLinkage(GlobalValue::ExternalLinkage);
	gVar->setInitializer(ConstantInt::get(LLVM_Context, APInt(32, 0)));
	//	auto const_ = gVar->isConstant();
	//	gVar->setAlignment(4);
	{
		// CODEGEN TESTS
		std::vector<std::string> Args{"base_ptr"};
		std::string Name{"loader"};

		// CODEGEN PTOTOTYPE
		std::vector<Type*> Doubles(Args.size(), Type::getInt8Ty(LLVM_Context)->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getInt32Ty(LLVM_Context), Doubles, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, LLVM_Module.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);

		// CODEGEN FUNCTION
		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(LLVM_Context, "entry", TheFunction);
		LLVM_Builder.SetInsertPoint(BB);

		// Get pointer to first argument
		Value* Base = TheFunction->arg_begin();

		//	Value* Base_int = LLVM_Builder.CreateBitCast(Base, LLVM_Builder.getInt32Ty()->getPointerTo());
		//	PointerType::get(LLVM_Builder.getInt8Ty()->getPointerTo(), 0);

		// Get __int8* pointer Offset by LLVM_Builder.getInt32(8) from Base
		Value* gep = LLVM_Builder.CreateGEP(LLVM_Builder.getInt8Ty(), Base, LLVM_Builder.getInt32(8), "a1");
		// no-op bitcast from __int8* to __int32*
		Value* gep_int = LLVM_Builder.CreateBitCast(gep, LLVM_Builder.getInt32Ty()->getPointerTo());
		//	gep_int->dump();
		//	Load __int32 from pointer
		Value* load = LLVM_Builder.CreateLoad(gep_int, "a1");
		// Store loaded value in global
		LLVM_Builder.CreateStore(load, gVar);

		Value* retval = LLVM_Builder.CreateLoad(gVar);

		//	Value* RetVal = Body->codegen();

		if (retval)
		{
			// Finish off the function.
			LLVM_Builder.CreateRet(retval);

			// Validate the generated code, checking for consistency.
			verifyFunction(*TheFunction);

			// Run the optimizer on the function.
			LLVM_FPM->run(*TheFunction);
		}
	}

	{
		// CODEGEN TESTS
		std::vector<std::string> Args{};
		std::string Name{"get_global"};

		// CODEGEN PTOTOTYPE
		std::vector<Type*> Doubles(Args.size(), Type::getInt8Ty(LLVM_Context)->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getInt32Ty(LLVM_Context), Doubles, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, Name, LLVM_Module.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);

		// CODEGEN FUNCTION
		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(LLVM_Context, "entry", TheFunction);
		LLVM_Builder.SetInsertPoint(BB);

		Value* retval = LLVM_Builder.CreateLoad(gVar);

		//	Value* RetVal = Body->codegen();

		if (retval)
		{
			// Finish off the function.
			LLVM_Builder.CreateRet(retval);

			// Validate the generated code, checking for consistency.
			verifyFunction(*TheFunction);

			// Run the optimizer on the function.
			LLVM_FPM->run(*TheFunction);
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

	shllJIT = std::make_unique<ShaderJIT>();

	InitializeModuleAndPassManager();

	GenerateFunc_0();

	shllJIT->addModule(std::move(LLVM_Module));
	InitializeModuleAndPassManager();

	// CALL FUNCTIONSIN CPP
	auto ExprSymbol = shllJIT->findSymbol("main");
	assert(ExprSymbol && "Function not found");

	// Get the symbol's address and cast it to the right type (takes no
	// arguments, returns a double) so we can call it as a native function.
	double (*FP)(double a) = (double (*)(double a))(intptr_t)cantFail(ExprSymbol.getAddress());

	InitializeModuleAndPassManager();

	GenerateFunc_1();

	shllJIT->addModule(std::move(LLVM_Module));
	InitializeModuleAndPassManager();

	// CALL FUNCTIONSIN CPP
	ExprSymbol = shllJIT->findSymbol("main");
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
	LLVM_Module->dump();
	shllJIT->addModule(std::move(LLVM_Module));
	InitializeModuleAndPassManager();

	auto ExprSymbol_gptr = shllJIT->findSymbol("global_x_ptr");
	assert(ExprSymbol_gptr && "Function not found");

	auto ExprSymbol_ptr = shllJIT->findSymbol("loader");
	assert(ExprSymbol_ptr && "Function not found");

	data_test d{1, 2};
	//	auto* tmp                = &d;
	//	auto tmp__               = reinterpret_cast<double*>(tmp);
	__int32 (*_FP__ptr)(__int8* base_ptr) = (__int32 (*)(__int8* base_ptr))(intptr_t)cantFail(ExprSymbol_ptr.getAddress());
	auto t_fp_ptr                         = _FP__ptr(reinterpret_cast<__int8*>(&d));

	auto ExprSymbol_get_global_ptr = shllJIT->findSymbol("get_global");
	assert(ExprSymbol_get_global_ptr && "Function not found");

	__int32 (*_FP__get_global)() = (__int32 (*)())(intptr_t)cantFail(ExprSymbol_get_global_ptr.getAddress());
	auto t_fp_global_ptr         = _FP__get_global();

	auto global_x_ptr = (_int32*)cantFail(ExprSymbol_gptr.getAddress());

	*global_x_ptr = 4;

	t_fp_global_ptr = _FP__get_global();

	InitializeModuleAndPassManager();

	GenerateFunc_6();
	LLVM_Module->dump();
	shllJIT->addModule(std::move(LLVM_Module));
	InitializeModuleAndPassManager();
	//auto ExprSymbol_global_x_ptr_ptr = shllJIT->findSymbol("global_x_ptr_ptr");
	//double (*_FPglobal_x_ptr_ptr)()  = (double (*)())(intptr_t)cantFail(ExprSymbol_get_global_ptr.getAddress());
	//_FPglobal_x_ptr_ptr();

	// Beware: exiting in debug mode triggers assert.
	return 0;
}