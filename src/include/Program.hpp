#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>

#include "Tokenizer.h"
#include "Parser.h"
#include "CodeGen.h"
#include "Context.h"
#include "Layout.h"

namespace slljit
{
	using namespace std;

	template <class T>
	class Program
	{
	public:
		Context& m_context;
		LocalContext m_local_context;
		Layout m_layout;
		//	vector<pair<intptr_t, GlobalDefinition>> runtime_globals;
		double (*main_func)() = nullptr;
		void (*loader__)(T*)  = nullptr;

	public:
		Program(Context& m_context)
		    : m_context(m_context), m_local_context(m_context)
		{
		}
		void compile(string body, Layout layout)
		{
			m_layout = layout;
			Parser m_parser(m_local_context.BinopPrecedence);
			CodeGen m_codegen;
			m_parser.set_source(body);
			m_parser.set_variables(m_layout);

			m_parser.parse();

			// TMP CODEGEN KERNEL
			auto [prototypes, functions] = m_parser.get_ast();
			m_codegen.compile_layout(m_context, m_local_context, m_layout);
			m_codegen.compile(std::move(prototypes), std::move(functions), m_context, m_local_context);

			//	auto loader_symbol = m_context.shllJIT->findSymbol("__layout_loader_", m_local_context.module_key);
			//	auto symbol        = m_context.shllJIT->findSymbol("main", m_local_context.module_key);
			auto symbol        = ExitOnError()(m_context.shllJIT->lookup("main", m_local_context.JD));
			auto loader_symbol = ExitOnError()(m_context.shllJIT->lookup("__layout_loader_", m_local_context.JD));
			main_func          = (double (*)())(intptr_t)symbol.getAddress();
			loader__           = (void (*)(T*))(intptr_t)loader_symbol.getAddress();
		}
		double run(T* data)
		{
			loader__(data);
			auto retval = main_func();
			return retval;
		}
	};
}; // namespace slljit
