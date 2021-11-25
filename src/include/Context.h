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
#include "Layout.h"

namespace slljit
{
	using namespace llvm;
	using namespace std;

	static void init__();

	/// Used to hold global compilation and execution context.
	/**
	 * Holds jit compilation engine.
	 */
	class Context
	{
	public:
		std::unique_ptr<ShaderJIT> shllJIT;

	public:
		Context();
	};

	/// Used to define context for a single Program.
	/**
	 * Holds Program-local context variables like binary module the Program is compiled into.
	 */
	class LocalContext
	{
	private:
		std::unique_ptr<LLVMContext> LLVM_Context;
		std::unique_ptr<IRBuilder<>> LLVM_Builder;

		std::unique_ptr<Module> LLVM_Module;

		JITDylib& JD;

		Layout layout;

	public:
		// TODO: make this private
		std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos; // TODO: figure aout a better way
		std::map<char, int> BinopPrecedence;
		std::map<std::string, AllocaInst*> NamedValues; // TODO: make this scoped like in Parser

		LocalContext(Context& m_context, Layout layout);

		inline LLVMContext& getContext()
		{
			return *LLVM_Context;
		}

		inline IRBuilder<>& getBuilder()
		{
			return *LLVM_Builder;
		}

		inline Module& getModule()
		{
			return *LLVM_Module;
		}

		inline const Layout& getLayout()
		{
			return layout;
		}

		inline JITDylib& getJITDylib()
		{
			return JD;
		}

		inline std::unique_ptr<Module> takeModule()
		{
			return std::move(LLVM_Module);
		}

		inline std::unique_ptr<LLVMContext> takeContext()
		{
			return std::move(LLVM_Context);
		}
	};
}; // namespace slljit
