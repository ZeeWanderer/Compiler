#pragma once

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"

#include "JIT.h"
#include "AST.h"

namespace slljit
{
	using namespace llvm;
	using namespace std;

	static void init__();

	class Context
	{
	public:
		std::unique_ptr<ShaderJIT> shllJIT;

	public:
		Context();
	};

	class LocalContext
	{
	public:
		std::unique_ptr<LLVMContext> LLVM_Context;
		std::unique_ptr<IRBuilder<>> LLVM_Builder;

		std::unique_ptr<Module> LLVM_Module;

		std::map<std::string, AllocaInst*> NamedValues;

		std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
		std::map<char, int> BinopPrecedence;

		JITDylib& JD;
		//	VModuleKey module_key;

	public:
		LocalContext(Context& m_context);

		//	void set_key(VModuleKey module_key);

		//	auto get_key();
	};
}; // namespace slljit
