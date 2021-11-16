#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <charconv>
#include <list>
#include "Context.h"
#include "AST.h"
#include "Layout.h"

namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	class Layout;

	class CodeGen
	{
	public:
		void compile_layout(Context& m_context, LocalContext& m_local_context, Layout& m_layout);
		Error compile(std::list<std::unique_ptr<PrototypeAST>> prototypes, std::list<std::unique_ptr<FunctionAST>> functions, Context& m_context, LocalContext& m_local_context);
	};
}; // namespace slljit
