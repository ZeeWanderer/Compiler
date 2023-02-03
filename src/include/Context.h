#pragma once
#include <vector>
#include <list>
#include <map>

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
#include "Error.h"

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

		list<map<string, AllocaInst*>> scope_list;

	public:
		// TODO: make this private
		std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos; // TODO: figure aout a better way
		std::map<char, int> BinopPrecedence;

		LocalContext(Context& m_context, Layout layout);

		inline void pop_scope()
		{
			scope_list.pop_back();
		}

		inline void push_scope()
		{
			scope_list.emplace_back(map<string, AllocaInst*>());
		}

		inline void push_var_into_scope(string name, AllocaInst* alloca)
		{
			auto& current_scope = scope_list.back();
			current_scope[name] = alloca;
		}

		[[nodiscard]] inline Expected<GlobalDefinition> find_global(string name)
		{
			auto is_even = [name](GlobalDefinition i)
			{ return i.name == name; };

			const auto it = std::find_if(layout.globals.begin(), layout.globals.end(), is_even);
			if (it != layout.globals.end())
			{
				return *it;
			}

			return make_error<CompileError>("unknown global variable name: "s + name);
		}

		[[nodiscard]] inline Expected<AllocaInst*> find_var_in_scope(string name)
		{
			for (auto it = scope_list.rbegin(); it != scope_list.rend(); it++)
			{
				auto it_ = it->find(name);
				if (it_ != it->end())
				{
					return it_->second;
				}
			}
			return make_error<CompileError>("unknown variable name: "s + name);
		}

		inline bool check_curent_scope(string name)
		{
			auto it  = scope_list.rbegin();
			auto it_ = it->find(name);
			return it_ != it->end();
		}

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
