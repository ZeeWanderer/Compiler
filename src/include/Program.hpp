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
		const string m_body;

		R(*main_func)
		(T*) = nullptr;

	public:
		/**
		 * Program constructor
		 * @param m_context global context to use 
		 * @param layout Layout to use
		 * @param body source code string
		 */
		Program(Context& m_context, Layout layout, const string body)
		    : m_context(m_context), m_local_context(m_context, layout), m_body(body)
		{
		}

		/**
		 * Compiles source to binary if not already compiled.
		 * No recompilation occurs if different parameters are passed but Program is already compiled. 
		 * Thread-safe.
		 * @param options CompileOptions to use for compilation.
		 * @param bDumpIR If ir should be dumped to console during compilation. Not threadsafe.
		 * @return Error object, requires to be checked.
		 */
		[[nodiscard]]
		Error compile(CompileOptions options = CompileOptions(), bool bDumpIR = false)
		{
			if (main_func != nullptr)
				return Error::success();

			Parser m_parser(m_local_context.BinopPrecedence);
			CodeGen m_codegen;
			m_parser.set_source(m_body);
			auto parse_err_0 = m_parser.set_variables(m_local_context.getLayout());
			if (parse_err_0)
				return move(parse_err_0);

			auto parse_err_1 = m_parser.parse();
			if (parse_err_1)
				return move(parse_err_1);

			// TMP CODEGEN KERNEL
			auto [prototypes, functions] = m_parser.take_ast();
			m_codegen.compile_layout(m_context, m_local_context);
			auto compile_err = m_codegen.compile(std::move(prototypes), std::move(functions), m_context, m_local_context, options, bDumpIR);
			if (compile_err)
				return move(compile_err);

			auto symbol = m_context.shllJIT->lookup("main", m_local_context.getJITDylib());
			if (!symbol)
				return symbol.takeError();

			main_func = (R(*)(T*))(intptr_t)symbol->getAddress();

			return Error::success();
		}

		/**
		 * Run compiled function with provided data object pointer.
		 * @param data Pointer to the data object.
		 * @return Passthrough value from compiled function.
		 */
		inline R run(T* data)
		{
			auto retval = main_func(data);
			return retval;
		}
	};
}; // namespace slljit
