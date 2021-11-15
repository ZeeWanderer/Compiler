﻿#include "pch.h"
#include "Parser.h"
#include "AST.h"
#include "Types.h"
#include "Layout.h"

using namespace llvm;
using namespace llvm::orc;
using namespace slljit;
using namespace std;

namespace slljit
{
	/// GetTokPrecedence - Get the precedence of the pending binary operator token.

	int Parser::getNextToken()
	{
		return CurTok = m_tokenizer.gettok();
	}

	Parser::Parser(std::map<char, int>& BinopPrecedence)
	    : BinopPrecedence(BinopPrecedence)
	{
	}

	void Parser::set_source(std::string_view source)
	{
		m_tokenizer.set_source(source);
		getNextToken();
	}

	void Parser::set_variables(Layout& context)
	{
		push_scope();
		for (auto& gl : context.globals)
		{
			push_var_into_scope(gl.name, TypeID(gl.type));
		}

		for (auto& gl : context.constant_globals)
		{
			push_var_into_scope(gl.first, TypeID(gl.second.type));
		}
	}

	std::pair<list<unique_ptr<PrototypeAST>>, list<unique_ptr<FunctionAST>>> Parser::get_ast()
	{
		return std::pair{std::move(PrototypeAST_list), std::move(FunctionAST_list)};
	}

	std::list<std::unique_ptr<FunctionAST>> Parser::get_function_ast()
	{
		return std::move(FunctionAST_list);
	}

	std::list<std::unique_ptr<PrototypeAST>> Parser::get_prototype_ast()
	{
		return std::move(PrototypeAST_list);
	}

	void Parser::parse()
	{
		MainLoop();
	}

	inline void Parser::pop_scope()
	{
		scope_list.pop_back();
	}

	inline void Parser::push_scope()
	{
		scope_list.emplace_back(map<string, TypeID>());
	}

	inline void Parser::push_var_into_scope(string name, TypeID type)
	{
		auto& current_scope = scope_list.back();
		current_scope[name] = type;
	}

	void Parser::set_current_function_scope(PrototypeAST* p)
	{
		this->current_function_proto = p;
	}

	const PrototypeAST* Parser::get_current_function_scope()
	{
		return this->current_function_proto;
	}

	inline TypeID Parser::find_var_in_scope(string name)
	{
		for (auto it = scope_list.rbegin(); it < scope_list.rend(); it++)
		{
			auto it_ = it->find(name);
			if (it_ != it->end())
			{
				return it_->second;
			}
		}
		return none;
	}

	int Parser::GetTokPrecedence()
	{
		if (!isascii(CurTok))
			return -1;

		// Make sure it's a declared binop.
		int TokPrec = BinopPrecedence[CurTok];
		if (TokPrec <= 0)
			return -1;
		return TokPrec;
	}

	/// numberexpr ::= number

	std::unique_ptr<ExprAST> Parser::ParseNumberExpr()
	{
		const auto numStr = m_tokenizer.get_number_string();
		if (numStr.find_first_of(".", 0) != string::npos)
		{
			double value;
			std::from_chars(numStr.c_str(), numStr.c_str() + numStr.size(), value);
			auto Result = std::make_unique<NumberExprAST>(value);
			getNextToken(); // consume the number
			return std::move(Result);
		}
		else
		{
			int64_t value;
			std::from_chars(numStr.c_str(), numStr.c_str() + numStr.size(), value);
			auto Result = std::make_unique<NumberExprAST>(value);
			getNextToken(); // consume the number
			return std::move(Result);
		}
	}

	/// parenexpr ::= '(' expression ')'

	std::unique_ptr<ExprAST> Parser::ParseParenExpr()
	{
		getNextToken(); // eat (.
		auto V = ParseExpression();
		if (!V)
			return nullptr;

		if (CurTok != ')')
			return LogError("expected ')'");
		getNextToken(); // eat ).
		return V;
	}

	/// identifierexpr
	///   ::= identifier
	///   ::= identifier '(' expression* ')'

	std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr()
	{
		auto IdName = m_tokenizer.get_identifier();

		getNextToken(); // eat identifier.

		// return expression
		if (IdName == "return")
		{
			//	getNextToken(); // eat return.
			auto expr          = ParseExpression();
			const auto f_scope = get_current_function_scope();
			return std::make_unique<ReturnExprAST>(std::move(expr), f_scope->getRetType());
		}

		if (CurTok != '(') // Simple variable ref.
		{
			auto type_ = find_var_in_scope(IdName);
			return std::make_unique<VariableExprAST>(IdName, type_);
		}

		// Call.
		getNextToken(); // eat (
		std::vector<std::unique_ptr<ExprAST>> Args;
		//std::vector<TypeID> ArgTypes;
		if (CurTok != ')')
		{
			while (true)
			{
				if (auto Arg = ParseExpression())
				{
					//ArgTypes.push_back(Arg->getType());
					Args.push_back(std::move(Arg));
				}
				else
					return nullptr;

				if (CurTok == ')')
					break;

				if (CurTok != ',')
					return LogError("Expected ')' or ',' in argument list");
				getNextToken();
			}
		}

		// Eat the ')'.
		getNextToken();

		TypeID ret_type_     = none;
		bool prototype_found = false;
		std::vector<TypeID> ArgTypes;
		for (auto const& proto : PrototypeAST_list)
		{
			if (proto->match(IdName /*, ArgTypes*/))
			{
				ret_type_       = proto->getRetType();
				ArgTypes        = proto->getArgTypes();
				prototype_found = true;
				break;
			}
		}
		if (!prototype_found)
			return LogError("Definition of callable not found");

		return std::make_unique<CallExprAST>(ret_type_, IdName, std::move(ArgTypes), std::move(Args));
	}

	/// ifexpr ::= 'if' expression 'then' expression 'else' expression

	std::unique_ptr<ExprAST> Parser::ParseIfExpr()
	{
		push_scope();

		getNextToken(); // eat the if.

		if (CurTok != '(')
			return LogError("expected (");

		getNextToken(); // eat (

		// condition.
		auto Cond = ParseExpression();
		if (!Cond)
			return nullptr;

		if (CurTok != ')')
			return LogError("expected )");

		getNextToken(); // eat )

		auto ifBody = ParseExpressionList();
		if (ifBody.empty())
			return nullptr;

		if (CurTok == tok_else)
		{
			getNextToken(); // eat else

			auto ElseBody = ParseExpressionList();
			if (ElseBody.empty())
				return nullptr;
			return std::make_unique<IfExprAST>(std::move(Cond), std::move(ifBody), std::move(ElseBody));
		}
		else
		{
			return std::make_unique<IfExprAST>(std::move(Cond), std::move(ifBody), ExprList());
		}

		pop_scope();
	}

	/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression

	std::unique_ptr<ExprAST> Parser::ParseForExpr()
	{
		push_scope();

		getNextToken(); // eat the for.

		if (CurTok != '(')
			return LogError("expected ( after for");
		getNextToken(); // eat (

		// g_var init expr.
		auto Varinit = ParseExpression();
		if (CurTok != ';')
			return LogError("Expected ;");
		getNextToken(); // Eat ;

		// Cond.
		auto Cond = ParseExpression();
		if (CurTok != ';')
			return LogError("Expected ;");
		getNextToken(); // Eat ;

		// AfterloopExpr.
		auto Afterloop = ParseExpression();

		if (CurTok != ')')
			return LogError("expected )");
		getNextToken(); // eat (

		// Body
		auto Body = ParseExpressionList();
		if (Body.empty())
			return LogError("Empty loop body.");

		return std::make_unique<ForExprAST>(std::move(Varinit), std::move(Cond), std::move(Body), std::move(Afterloop));

		pop_scope();
	}

	/// varexpr ::= 'g_var' identifier ('=' expression)?
	//                    (',' identifier ('=' expression)?)* 'in' expression

	std::unique_ptr<ExprAST> Parser::ParseVarExpr()
	{
		auto VarTypeStr = m_tokenizer.get_type_identifier();
		auto VarTypeID  = basic_types_id_map.at(VarTypeStr); // TODO: error check
		getNextToken();                                      // eat type.

		std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

		// At least one variable name is required.
		if (CurTok != tok_identifier)
			return LogError("expected identifier after var");

		while (true)
		{
			auto Name = m_tokenizer.get_identifier();
			getNextToken(); // eat identifier.

			// Read the optional initializer.
			std::unique_ptr<ExprAST> Init = nullptr;
			if (CurTok == '=')
			{
				getNextToken(); // eat the '='.

				Init = ParseExpression();
				if (!Init)
					return nullptr;
			}

			push_var_into_scope(Name, VarTypeID);
			VarNames.push_back(std::make_pair(Name, std::move(Init)));

			// End of g_var list, exit loop.
			if (CurTok != ',')
				break;
			getNextToken(); // eat the ','.

			if (CurTok != tok_identifier)
				return LogError("expected identifier list after var");
		}

		return std::make_unique<VarExprAST>(std::move(VarTypeID), std::move(VarNames));
	}

	/// primary
	///   ::= identifierexpr
	///   ::= numberexpr
	///   ::= parenexpr
	///   ::= ifexpr
	///   ::= forexpr
	///   ::= varexpr

	std::unique_ptr<ExprAST> Parser::ParsePrimary()
	{
		switch (CurTok)
		{
		default: return LogError("unknown token when expecting an expression");
		case tok_identifier: return ParseIdentifierExpr();
		case tok_number: return ParseNumberExpr();
		case '(': return ParseParenExpr();
		case tok_if: return ParseIfExpr();
		case tok_for: return ParseForExpr();
		case tok_type: return ParseVarExpr();
		}
	}

	/// unary
	///   ::= primary
	///   ::= '!' unary

	std::unique_ptr<ExprAST> Parser::ParseUnary()
	{
		// If the current token is not an operator, it must be a primary expr.
		if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
			return ParsePrimary();

		// If this is a unary operator, read it.
		int Opc = CurTok;
		getNextToken();
		if (auto Operand = ParseUnary())
			return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
		return nullptr;
	}

	/// binoprhs
	///   ::= ('+' unary)*

	std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
	{
		// If this is a binop, find its precedence.
		while (true)
		{
			int TokPrec = GetTokPrecedence();

			// If this is a binop that binds at least as tightly as the current binop,
			// consume it, otherwise we are done.
			if (TokPrec < ExprPrec)
				return LHS;

			// Okay, we know this is a binop.
			int BinOp = CurTok;
			getNextToken(); // eat binop

			// Parse the unary expression after the binary operator.
			auto RHS = ParseUnary();
			if (!RHS)
				return nullptr;

			// If BinOp binds less tightly with RHS than the operator after RHS, let
			// the pending operator take RHS as its LHS.
			int NextPrec = GetTokPrecedence();
			if (TokPrec < NextPrec)
			{
				RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
				if (!RHS)
					return nullptr;
			}

			// Merge LHS/RHS.
			LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
		}
	}

	/// expression
	///   ::= unary binoprhs
	///

	std::unique_ptr<ExprAST> Parser::ParseExpression()
	{
		//	TODO: move where it belongs
		if (CurTok == ';')
			return std::make_unique<NoOpAST>();

		auto LHS = ParseUnary();
		if (!LHS)
			return nullptr;

		return ParseBinOpRHS(0, std::move(LHS));
	}

	ExprList Parser::ParseExpressionList()
	{
		if (CurTok != '{')
			return LogErrorEX("Expected {");

		getNextToken(); // Eat {
		ExprList e_list;
		while (true)
		{
			auto retval = ParseExpression();
			if (retval->bExpectSemicolon())
			{
				if (CurTok != ';')
					return LogErrorEX("Expected ;");
				getNextToken(); // Eat ;
			}
			e_list.push_back(std::move(retval));
			if (CurTok == '}')
				break;
		}

		getNextToken(); // Eat }
		return e_list;
	}

	/// prototype
	///   ::= id '(' id* ')'
	///   ::= binary LETTER number? (id, id)
	///   ::= unary LETTER (id)

	std::unique_ptr<PrototypeAST> Parser::ParsePrototype()
	{
		std::string FnName;

		unsigned Kind             = 0; // 0 = identifier, 1 = unary, 2 = binary.
		unsigned BinaryPrecedence = 30;

		const auto FnRetTypeStr = m_tokenizer.get_type_identifier();
		const auto FnRetTypeID  = basic_types_id_map.at(FnRetTypeStr); // TODO: error check

		switch (CurTok)
		{
		default: return LogErrorP("Expected function name in prototype");
		case tok_identifier:
			FnName = m_tokenizer.get_identifier();
			Kind   = 0;
			getNextToken();
			break;
		}

		if (CurTok != '(')
			return LogErrorP("Expected '(' in prototype");

		std::vector<std::string> ArgNames;
		std::vector<TypeID> ArgTypes;
		while (getNextToken() == tok_type)
		{
			const auto ArgTypeStr = m_tokenizer.get_type_identifier();
			const auto ArgTypeID  = basic_types_id_map.at(ArgTypeStr);
			ArgTypes.emplace_back(ArgTypeID);

			if (getNextToken() == tok_identifier)
			{
				const auto arg_name = m_tokenizer.get_identifier();
				push_var_into_scope(arg_name, ArgTypeID);
				ArgNames.push_back(arg_name);
			}
			else
				return LogErrorP("Expected identifier after type in prototype");

			if (getNextToken() != ',')
				break;

		}
		if (CurTok != ')')
			return LogErrorP("Expected ')' in prototype");

		// success.
		getNextToken(); // eat ')'.

		if (!Kind && FnName == "main" && ArgNames.size() != 0)
			return LogErrorP("Invalid number of operands for main function");

		// Verify right number of names for operator.
		if (Kind && ArgNames.size() != Kind)
			return LogErrorP("Invalid number of operands for operator");

		return std::make_unique<PrototypeAST>(FnRetTypeID, FnName, ArgTypes, ArgNames, Kind != 0, BinaryPrecedence);
	}

	/// definition ::= 'def' prototype expression

	std::unique_ptr<FunctionAST> Parser::ParseTopLevelTypedExpression()
	{
		push_scope();

		const auto TypeStr = m_tokenizer.get_type_identifier();
		const auto TypeID  = basic_types_id_map.at(TypeStr); // TODO: error check

		this->function_ret_in_scope = TypeID;

		getNextToken(); // eat type.

		if (CurTok == tok_identifier)
		{
			auto Proto = ParsePrototype();
			if (!Proto)
				return nullptr;

			set_current_function_scope(Proto.get());
			auto E = ParseExpressionList();
			if (!E.empty())
			{
				auto& P = *Proto;

				PrototypeAST_list.emplace_back(std::move(Proto));
				return std::make_unique<FunctionAST>(P, std::move(E));
			}
			return LogErrorF("Empty function");
		}
		else
		{
			return LogErrorF("Expected identifier after type");
		}

		pop_scope();
	}

	/// external ::= 'extern' prototype

	std::unique_ptr<PrototypeAST> Parser::ParseExtern()
	{
		getNextToken(); // eat extern.
		return ParsePrototype();
	}

	void Parser::HandleTypedExpression()
	{
		if (auto FnAST = ParseTopLevelTypedExpression())
		{
			FunctionAST_list.emplace_back(std::move(FnAST));
			//	if (auto* FnIR = FnAST->codegen(m_context, m_local_context))
			//{
			//	fprintf(stderr, "Read function definition:");
			//	FnIR->print(errs());
			//	fprintf(stderr, "\n");
			//	shllJIT->addModule(std::move(LLVM_Module));
			//	InitializeModuleAndPassManager();
			//}
		}
		else
		{
			// Skip token for error recovery.
			getNextToken();
		}
	}

	void Parser::HandleExtern()
	{
		if (auto ProtoAST = ParseExtern())
		{
			PrototypeAST_list.emplace_back(std::move(ProtoAST));
			//	if (auto* FnIR = ProtoAST->codegen(m_context, m_local_context))
			//{
			//	fprintf(stderr, "Read extern: ");
			//	FnIR->print(errs());
			//	fprintf(stderr, "\n");
			//	FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
			//}
		}
		else
		{
			// Skip token for error recovery.
			getNextToken();
		}
	}

	/// top ::= definition | external | expression | ';'

	void Parser::MainLoop()
	{
		while (true)
		{
			//	fprintf(stderr, "ready> ");
			switch (CurTok)
			{
			case tok_eof: return;
			case ';': // ignore top-level semicolons.
				getNextToken();
				break;
			case tok_type: HandleTypedExpression(); break;
			case tok_extern: HandleExtern(); break;
			default: auto tmp = 2 + 3; break;
			}
		}
	}
}; // namespace slljit
