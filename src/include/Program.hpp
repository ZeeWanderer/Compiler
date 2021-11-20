#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>

#include "Error.h"
#include "Tokenizer.h"
#include "Parser.h"
#include "CodeGen.h"
#include "Context.h"
#include "Layout.h"
#include "Options.h"

namespace slljit
{
	using namespace std;

	/// Used to compile, hold and run a shader program.
	/** 
	 * Thread-safe for compilation.
	 * @tparam T data structure type used to pass data to compiled function
	 * @tparam R compiled function return type
	 */
	template <class T, typename R>
	class Program
	{
	private:
		Context& m_context;
		LocalContext m_local_context;
		Layout m_layout;

		R(*main_func)
		(void)               = nullptr;
		void (*loader__)(T*) = nullptr;

	public:
		/**
		 * Program constructor
		 * @param m_context global context to use 
		 */
		Program(Context& m_context)
		    : m_context(m_context), m_local_context(m_context)
		{
		}

		/**
		 * Compiles source to binary if not already compiled.
		 * No recompilation occurs if different parameters are passed but Program is already compiled. 
		 * Thread-safe.
		 * @param body Source code to compile
		 * @param layout Layout object to use for defining input data layout
		 * @param options CompileOptions to use for compilation.
		 * @return Error object, requires to be checked.
		 */
		[[nodiscard]] Error compile(string body, Layout layout, CompileOptions options = CompileOptions())
		{
			if (main_func != nullptr && loader__ != nullptr)
				return Error::success();

			m_layout = layout;
			Parser m_parser(m_local_context.BinopPrecedence);
			CodeGen m_codegen;
			m_parser.set_source(body);
			m_parser.set_variables(m_layout);

			auto err_ = m_parser.parse();
			if (err_)
				return err_;

			// TMP CODEGEN KERNEL
			auto [prototypes, functions] = m_parser.get_ast();
			m_codegen.compile_layout(m_context, m_local_context, m_layout);
			auto compile_err = m_codegen.compile(std::move(prototypes), std::move(functions), m_context, m_local_context, options);
			if (compile_err)
				return compile_err;

			auto symbol = m_context.shllJIT->lookup("main", m_local_context.JD);
			if (!symbol)
				return symbol.takeError();

			auto loader_symbol = m_context.shllJIT->lookup("__layout_loader_", m_local_context.JD);
			if (!loader_symbol)
				return loader_symbol.takeError();

			main_func = (R(*)())(intptr_t)symbol->getAddress();
			loader__  = (void (*)(T*))(intptr_t)loader_symbol->getAddress();

			return Error::success();
		}

		/**
		 * Run compiled function with provided data object pointer.
		 * @param data Pointer to the data object.
		 * @return Passthrough value from compiled function.
		 */
		R run(T* data)
		{
			loader__(data);
			auto retval = main_func();
			return retval;
		}
	};
}; // namespace slljit
