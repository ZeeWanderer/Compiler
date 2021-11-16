#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>

#include "llvm/Support/Error.h"

#include "Tokenizer.h"
#include "AST.h"
#include "Types.h"
#include "Layout.h"

namespace slljit
{
	using namespace std;
	//===----------------------------------------------------------------------===//
	// Parser
	//===----------------------------------------------------------------------===//

	/// BinopPrecedence - This holds the precedence for each binary operator that is
	/// defined.
	// GlobalContext
	//	static LLVMContext LLVM_Context;
	//	static IRBuilder<> LLVM_Builder(LLVM_Context);
	//	static std::unique_ptr<ShaderJIT> shllJIT;
	//
	//// ProgramLocal Context
	//	static std::unique_ptr<Module> LLVM_Module;
	//	static std::map<std::string, AllocaInst*> NamedValues;
	//	static std::unique_ptr<legacy::FunctionPassManager> LLVM_FPM;
	//	static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
	//	static std::map<char, int> BinopPrecedence = {{'=', 2}, {'<', 10}, {'>', 10}, {'+', 20}, {'-', 20}, {'*', 40}};

	class Parser
	{
	protected:
		std::map<char, int>& BinopPrecedence;
		std::list<std::unique_ptr<FunctionAST>> FunctionAST_list;
		std::list<std::unique_ptr<PrototypeAST>> PrototypeAST_list;

		std::map<std::string, PrototypeAST&> function_proto_table;

		PrototypeAST* current_function_proto = nullptr;

		vector<map<string, TypeID>> scope_list;

		Tokenizer m_tokenizer;

		/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
		/// token the parser is looking at.  getNextToken reads another token from the
		/// lexer and updates CurTok with its results.
		int CurTok;
		int getNextToken();

	public:
		Parser(std::map<char, int>& BinopPrecedence);

		void set_source(std::string_view source);

		void set_variables(Layout& context);

		std::pair<list<unique_ptr<PrototypeAST>>, list<unique_ptr<FunctionAST>>> get_ast();

		std::list<std::unique_ptr<FunctionAST>> get_function_ast();

		std::list<std::unique_ptr<PrototypeAST>> get_prototype_ast();

		Error parse();

		void pop_scope();

		void push_scope();

		void push_var_into_scope(string name, TypeID type);

		Expected<TypeID> find_var_in_scope(string name);

		void set_current_function_scope(PrototypeAST* p);

		const PrototypeAST* get_current_function_scope();

	protected:
		/// GetTokPrecedence - Get the precedence of the pending binary operator token.
		int GetTokPrecedence();

		//	static std::unique_ptr<ExprAST> ParseExpression();

		/// numberexpr ::= number
		Expected<std::unique_ptr<ExprAST>> ParseLiteralExpr();

		/// parenexpr ::= '(' expression ')'
		Expected<std::unique_ptr<ExprAST>> ParseParenExpr();

		/// identifierexpr
		///   ::= identifier
		///   ::= identifier '(' expression* ')'
		Expected<std::unique_ptr<ExprAST>> ParseIdentifierExpr();

		/// ifexpr ::= 'if' expression 'then' expression 'else' expression
		Expected<std::unique_ptr<ExprAST>> ParseIfExpr();

		/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
		Expected<std::unique_ptr<ExprAST>> ParseForExpr();

		/// varexpr ::= 'g_var' identifier ('=' expression)?
		//                    (',' identifier ('=' expression)?)* 'in' expression
		Expected<std::unique_ptr<ExprAST>> ParseVarExpr();

		/// primary
		///   ::= identifierexpr
		///   ::= numberexpr
		///   ::= parenexpr
		///   ::= ifexpr
		///   ::= forexpr
		///   ::= varexpr
		Expected<std::unique_ptr<ExprAST>> ParsePrimary();

		/// unary
		///   ::= primary
		///   ::= '!' unary
		Expected<std::unique_ptr<ExprAST>> ParseUnary();

		/// binoprhs
		///   ::= ('+' unary)*
		Expected<std::unique_ptr<ExprAST>> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);

		/// expression
		///   ::= unary binoprhs
		///
		Expected<std::unique_ptr<ExprAST>> ParseExpression();
		Expected<ExprList> ParseExpressionList();

		/// prototype
		///   ::= id '(' id* ')'
		///   ::= binary LETTER number? (id, id)
		///   ::= unary LETTER (id)
		Expected<std::unique_ptr<PrototypeAST>> ParsePrototype();

		/// definition ::= 'def' prototype expression
		Expected<std::unique_ptr<FunctionAST>> ParseTopLevelTypedExpression();

		/// toplevelexpr ::= expression
		//	static std::unique_ptr<FunctionAST> ParseTopLevelExpr()
		//{
		//	if (auto E = ParseExpression())
		//	{
		//		// Make an anonymous proto.
		//		auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
		//		return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
		//	}
		//	return nullptr;
		//}

		/// external ::= 'extern' prototype
		Expected<std::unique_ptr<PrototypeAST>> ParseExtern();

		Error HandleTypedExpression();

		Error HandleExtern();

		//	static void HandleTopLevelExpression()
		//{
		//	// Evaluate a top-level expression into an anonymous function.
		//	if (auto FnAST = ParseTopLevelExpr())
		//	{
		//		if (FnAST->codegen(m_context, m_local_context))
		//		{
		//			// JIT the module containing the anonymous expression, keeping a handle so
		//			// we can free it later.
		//			auto H = shllJIT->addModule(std::move(LLVM_Module));
		//			InitializeModuleAndPassManager();
		//
		//			// Search the JIT for the __anon_expr symbol.
		//			auto ExprSymbol = shllJIT->findSymbol("__anon_expr");
		//			assert(ExprSymbol && "Function not found");
		//
		//			// Get the symbol's address and cast it to the right type (takes no
		//			// arguments, returns a double) so we can call it as a native function.
		//			double (*FP)() = (double (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
		//			fprintf(stderr, "Evaluated to %f\n", FP());
		//
		//			// Delete the anonymous expression module from the JIT.
		//			shllJIT->removeModule(H);
		//		}
		//	}
		//	else
		//	{
		//		// Skip token for error recovery.
		//		getNextToken();
		//	}
		//}

		/// top ::= definition | external | expression | ';'
		Error MainLoop();
	};
}; // namespace slljit
