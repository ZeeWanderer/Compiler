#pragma once
// TODO: remove pch,h
#include "pch.h"
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
		LLVMContext LLVM_Context;
		IRBuilder<> LLVM_Builder;
		std::unique_ptr<ShaderJIT> shllJIT;

	public:
		Context();
	};

	class LocalContext
	{
	public:
		std::unique_ptr<Module> LLVM_Module;
		std::map<std::string, AllocaInst*> NamedValues;
		std::unique_ptr<legacy::FunctionPassManager> LLVM_FPM;
		std::unique_ptr<legacy::PassManager> LLVM_PM;
		std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
		std::map<char, int> BinopPrecedence;
		VModuleKey module_key;

	public:
		LocalContext(Context& m_context);

		void set_key(VModuleKey module_key);

		auto get_key();
	};
}; // namespace slljit
