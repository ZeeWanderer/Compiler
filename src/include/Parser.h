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

	/// Used to parse source into AST.
	/**
	 * Performs Source->Token stream->AST transformation.
	 * Keeps track of variable and function scopes.
	 */
	class Parser
	{
	protected:
		std::map<char, int>& BinopPrecedence;
		std::list<std::unique_ptr<FunctionAST>> FunctionAST_list;
		std::list<std::unique_ptr<PrototypeAST>> PrototypeAST_list;

		std::map<std::string, PrototypeAST&> function_proto_table;

		PrototypeAST* current_function_proto = nullptr;

		list<map<string, TypeID>> scope_list;

		Tokenizer m_tokenizer;

		/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
		/// token the parser is looking at.  getNextToken reads another token from the
		/// lexer and updates CurTok with its results.
		int CurTok;
		int getNextToken();

	public:
		Parser(std::map<char, int>& BinopPrecedence);

		/**
		 * Sets source to process.
		 * @param source Source program string.
		 */
		void set_source(std::string_view source);

		/**
		 * Sets Layout variables to use as a reference during parse process.
		 * @param context Reference to Layout to take variables from.
		 */
		[[nodiscard]]
		Error set_variables(const Layout& context);

		/**
		 * Take posession of parsed AST.
		 * @return Moved pair of lists of prototypes and function AST expressions.
		 */
		std::pair<list<unique_ptr<PrototypeAST>>, list<unique_ptr<FunctionAST>>> take_ast();

		/**
		 * Take posession of functions AST.
		 * @return Moved list of functions AST
		 */
		std::list<std::unique_ptr<FunctionAST>> take_function_ast();

		/**
		 * Take posession of function prototypes AST.
		 * @return Moved list of function prototypes AST.
		 */
		std::list<std::unique_ptr<PrototypeAST>> take_prototype_ast();

		/**
		 * Parse the sources.
		 * @return Error object, requires to be ckecked.
		 */
		[[nodiscard]]
		Error parse();

		/**
		 * Pop current variable scope from stack.
		 */
		void pop_scope();

		/**
		 * Puch empty variable scope to stack.
		 */
		void push_scope();

		/**
		 * Puch variable into current scope.
		 */
		[[nodiscard]]
		Error push_var_into_scope(string name, TypeID type);

		/**
		 * Find variable in scopes by name.
		 * Searches scopes retroactively.
		 * @return Either error or variable type, requires to be cheked.
		 */
		[[nodiscard]]
		Expected<TypeID> find_var_in_scope(string name);

		/**
		 * Set current function scope.
		 * @param p ptotoype AST pointer.
		 */
		void set_current_function_scope(PrototypeAST* p);

		/**
		 * Get current function scope.
		 * @return prototype AST pointer of current function.
		 */
		const PrototypeAST* get_current_function_scope();

	protected:
		/// Get the precedence of the pending binary operator token.
		int GetTokPrecedence();

		/// numberexpr ::= number
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseLiteralExpr();

		/// parenexpr ::= '(' expression ')'
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseParenExpr();

		/// identifierexpr
		///   ::= identifier
		///   ::= identifier '(' expression* ')'
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseIdentifierExpr();

		/// ifexpr ::= 'if' expression 'then' expression 'else' expression
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseIfExpr();

		/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseForExpr();

		/// varexpr ::= 'g_var' identifier ('=' expression)?
		//                    (',' identifier ('=' expression)?)* 'in' expression
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseVarExpr();

		/// primary
		///   ::= identifierexpr
		///   ::= numberexpr
		///   ::= parenexpr
		///   ::= ifexpr
		///   ::= forexpr
		///   ::= varexpr
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParsePrimary();

		/// unary
		///   ::= primary
		///   ::= '!' unary
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseUnary();

		/// binoprhs
		///   ::= ('+' unary)*
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);

		/// expression
		///   ::= unary binoprhs
		///
		[[nodiscard]]
		Expected<std::unique_ptr<ExprAST>> ParseExpression();

		[[nodiscard]]
		Expected<ExprList> ParseExpressionList();

		/// prototype
		///   ::= id '(' id* ')'
		///   ::= binary LETTER number? (id, id)
		///   ::= unary LETTER (id)
		[[nodiscard]]
		Expected<std::unique_ptr<PrototypeAST>> ParsePrototype();

		/// definition ::= 'def' prototype expression
		[[nodiscard]]
		Expected<std::unique_ptr<FunctionAST>> ParseTopLevelTypedExpression();

		/// external ::= 'extern' prototype
		[[nodiscard]]
		Expected<std::unique_ptr<PrototypeAST>> ParseExtern();

		[[nodiscard]]
		Error HandleTypedExpression();

		[[nodiscard]]
		Error HandleExtern();

		/// top ::= definition | external | expression | ';'
		[[nodiscard]]
		Error MainLoop();
	};
}; // namespace slljit
