#include "pch.h"
#include "Parser.h"
#include "AST.h"
#include "Types.h"
#include "Layout.h"
#include "Error.h"

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

	Error Parser::parse()
	{
		return MainLoop();
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

	inline Expected<TypeID> Parser::find_var_in_scope(string name)
	{
		for (auto it = scope_list.rbegin(); it < scope_list.rend(); it++)
		{
			auto it_ = it->find(name);
			if (it_ != it->end())
			{
				return it_->second;
			}
		}
		return make_error<ParserError>("variable: " + name + " not found", m_tokenizer.get_source_location());
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

	Expected<std::unique_ptr<ExprAST>> Parser::ParseNumberExpr()
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

	Expected<std::unique_ptr<ExprAST>> Parser::ParseParenExpr()
	{
		getNextToken(); // eat (.
		auto V = ParseExpression();
		if (!V)
			return V.takeError();

		if (CurTok != ')')
			return make_error<ParserError>("expected ')'", m_tokenizer.get_source_location());

		getNextToken(); // eat ).
		return V;
	}

	/// identifierexpr
	///   ::= identifier
	///   ::= identifier '(' expression* ')'

	Expected<std::unique_ptr<ExprAST>> Parser::ParseIdentifierExpr()
	{
		auto IdName = m_tokenizer.get_identifier();

		getNextToken(); // eat identifier.

		// return expression
		if (IdName == "return")
		{
			//	getNextToken(); // eat return.
			auto expr = ParseExpression();
			if (!expr)
				return expr.takeError();

			const auto f_scope = get_current_function_scope();
			return std::make_unique<ReturnExprAST>(std::move(*expr), f_scope->getRetType());
		}

		if (CurTok != '(') // Simple variable ref.
		{
			auto type_ = find_var_in_scope(IdName);
			if (!type_)
				return type_.takeError();
			return std::make_unique<VariableExprAST>(IdName, *type_);
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
					Args.push_back(std::move(*Arg));
				}
				else
					return Arg.takeError();

				if (CurTok == ')')
					break;

				if (CurTok != ',')
					return make_error<ParserError>("expected ')' or ',' in argument list", m_tokenizer.get_source_location());
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
			return make_error<ParserError>("definition of callable not found", m_tokenizer.get_source_location());

		return std::make_unique<CallExprAST>(ret_type_, IdName, std::move(ArgTypes), std::move(Args));
	}

	/// ifexpr ::= 'if' expression 'then' expression 'else' expression

	Expected<std::unique_ptr<ExprAST>> Parser::ParseIfExpr()
	{
		push_scope();

		getNextToken(); // eat the if.

		if (CurTok != '(')
			return make_error<ParserError>("expected (", m_tokenizer.get_source_location());

		getNextToken(); // eat (

		// condition.
		auto Cond = ParseExpression();
		if (!Cond)
			return Cond.takeError();

		if (CurTok != ')')
			return make_error<ParserError>("expected )", m_tokenizer.get_source_location());

		getNextToken(); // eat )

		auto ifBody = ParseExpressionList();
		if (!ifBody)
			return ifBody.takeError();
		if (ifBody->empty())
			return make_error<ParserError>("empty if body", m_tokenizer.get_source_location());

		if (CurTok == tok_else)
		{
			getNextToken(); // eat else

			auto ElseBody = ParseExpressionList();
			if (!ElseBody)
				return ElseBody.takeError();
			if (ElseBody->empty())
				return make_error<ParserError>("empty else body", m_tokenizer.get_source_location());
			return std::make_unique<IfExprAST>(std::move(*Cond), std::move(*ifBody), std::move(*ElseBody));
		}
		else
		{
			return std::make_unique<IfExprAST>(std::move(*Cond), std::move(*ifBody), ExprList());
		}

		pop_scope();
	}

	/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression

	Expected<std::unique_ptr<ExprAST>> Parser::ParseForExpr()
	{
		push_scope();

		getNextToken(); // eat the for.

		if (CurTok != '(')
			return make_error<ParserError>("expected ( after for", m_tokenizer.get_source_location());
		getNextToken(); // eat (

		// g_var init expr.
		auto Varinit = ParseExpression();
		if (!Varinit)
			return Varinit.takeError();

		if (CurTok != ';')
			return make_error<ParserError>("expected ;", m_tokenizer.get_source_location());
		getNextToken(); // Eat ;

		// Cond.
		auto Cond = ParseExpression();
		if (!Cond)
			return Cond.takeError();

		if (CurTok != ';')
			return make_error<ParserError>("expected ;", m_tokenizer.get_source_location());
		getNextToken(); // Eat ;

		// AfterloopExpr.
		auto Afterloop = ParseExpression();
		if (!Afterloop)
			return Afterloop.takeError();

		if (CurTok != ')')
			return make_error<ParserError>("expected )", m_tokenizer.get_source_location());
		getNextToken(); // eat (

		// Body
		auto Body = ParseExpressionList();
		if (!Body)
			return Body.takeError();
		if (Body->empty())
			return make_error<ParserError>("empty loop body.", m_tokenizer.get_source_location());

		return std::make_unique<ForExprAST>(std::move(*Varinit), std::move(*Cond), std::move(*Body), std::move(*Afterloop));

		pop_scope();
	}

	/// varexpr ::= 'g_var' identifier ('=' expression)?
	//                    (',' identifier ('=' expression)?)* 'in' expression

	Expected<std::unique_ptr<ExprAST>> Parser::ParseVarExpr()
	{
		auto VarTypeStr = m_tokenizer.get_type_identifier();
		auto VarTypeID  = basic_types_id_map.at(VarTypeStr); // TODO: error check
		getNextToken();                                      // eat type.

		std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

		// At least one variable name is required.
		if (CurTok != tok_identifier)
			return make_error<ParserError>("expected identifier after type", m_tokenizer.get_source_location());

		while (true)
		{
			auto Name = m_tokenizer.get_identifier();
			getNextToken(); // eat identifier.

			// Read the optional initializer.
			if (CurTok == '=')
			{
				getNextToken(); // eat the '='.

				auto Init = ParseExpression();
				if (!Init)
					return Init.takeError();

				VarNames.push_back(std::make_pair(Name, std::move(*Init)));
			}
			else
			{
				VarNames.push_back(std::make_pair(Name, nullptr));
			}

			push_var_into_scope(Name, VarTypeID);
			

			// End of g_var list, exit loop.
			if (CurTok != ',')
				break;
			getNextToken(); // eat the ','.

			if (CurTok != tok_identifier)
				return make_error<ParserError>("expected identifier after type", m_tokenizer.get_source_location());
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

	Expected<std::unique_ptr<ExprAST>> Parser::ParsePrimary()
	{
		switch (CurTok)
		{
		default: return make_error<ParserError>("unknown token when expecting an expression", m_tokenizer.get_source_location());
		case tok_identifier:
		{
			if (auto err_ = ParseIdentifierExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		case tok_number:
		{
			if (auto err_ = ParseNumberExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		case '(':
		{
			if (auto err_ = ParseParenExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		case tok_if:
		{
			if (auto err_ = ParseIfExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		case tok_for:
		{
			if (auto err_ = ParseForExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		case tok_type:
		{
			if (auto err_ = ParseVarExpr())
				return std::move(*err_);
			else
				return err_.takeError();
		}
		}
	}

	/// unary
	///   ::= primary
	///   ::= '!' unary

	Expected<std::unique_ptr<ExprAST>> Parser::ParseUnary()
	{
		// If the current token is not an operator, it must be a primary expr.
		if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
		{
			if (auto primary = ParsePrimary())
				return std::move(*primary);
			else
				return primary.takeError();
		}

		// If this is a unary operator, read it.
		int Opc = CurTok;
		getNextToken();
		if (auto Operand = ParseUnary())
			return std::make_unique<UnaryExprAST>(Opc, std::move(*Operand));
		else
			return Operand.takeError();
	}

	/// binoprhs
	///   ::= ('+' unary)*

	Expected<std::unique_ptr<ExprAST>> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
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
				return RHS.takeError();

			// If BinOp binds less tightly with RHS than the operator after RHS, let
			// the pending operator take RHS as its LHS.
			int NextPrec = GetTokPrecedence();
			if (TokPrec < NextPrec)
			{
				RHS = ParseBinOpRHS(TokPrec + 1, std::move(*RHS));
				if (!RHS)
					return RHS.takeError();
			}

			// Merge LHS/RHS.
			LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(*RHS));
		}
	}

	/// expression
	///   ::= unary binoprhs
	///

	Expected<std::unique_ptr<ExprAST>> Parser::ParseExpression()
	{
		//	TODO: move where it belongs
		if (CurTok == ';')
			return std::make_unique<NoOpAST>();

		auto LHS = ParseUnary();
		if (!LHS)
			return LHS.takeError();

		if (auto bionphs = ParseBinOpRHS(0, std::move(*LHS)))
		{
			return std::move(*bionphs);
		}
		else
		{
			return bionphs.takeError();
		}
	}

	Expected<ExprList> Parser::ParseExpressionList()
	{
		if (CurTok != '{')
			return make_error<ParserError>("expected {", m_tokenizer.get_source_location());

		getNextToken(); // Eat {
		ExprList e_list;
		while (true)
		{
			auto retval = ParseExpression();
			if (!retval)
				return retval.takeError();

			if ((*retval)->bExpectSemicolon())
			{
				if (CurTok != ';')
					return make_error<ParserError>("expected ;", m_tokenizer.get_source_location());
				getNextToken(); // Eat ;
			}
			e_list.push_back(std::move(*retval));
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

	Expected<std::unique_ptr<PrototypeAST>> Parser::ParsePrototype()
	{
		std::string FnName;

		unsigned Kind             = 0; // 0 = identifier, 1 = unary, 2 = binary.
		unsigned BinaryPrecedence = 30;

		getNextToken(); // eat type.

		const auto FnRetTypeStr = m_tokenizer.get_type_identifier();
		const auto FnRetTypeID  = basic_types_id_map.at(FnRetTypeStr); // TODO: error check

		switch (CurTok)
		{
		default: return make_error<ParserError>("expected identifier", m_tokenizer.get_source_location());
		case tok_identifier:
			FnName = m_tokenizer.get_identifier();
			Kind   = 0;
			getNextToken();
			break;
		}

		if (CurTok != '(')
			return make_error<ParserError>("expected '(' in prototype", m_tokenizer.get_source_location());

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
				return make_error<ParserError>("expected identifier after argument type in prototype", m_tokenizer.get_source_location());

			if (getNextToken() != ',')
				break;
		}
		if (CurTok != ')')
			return make_error<ParserError>("expected ')' in prototype", m_tokenizer.get_source_location());

		// success.
		getNextToken(); // eat ')'.

		if (!Kind && FnName == "main" && ArgNames.size() != 0)
			return make_error<ParserError>("invalid number of operands for main function", m_tokenizer.get_source_location());

		// Verify right number of names for operator.
		if (Kind && ArgNames.size() != Kind)
			return make_error<ParserError>("invalid number of operands for operator", m_tokenizer.get_source_location());

		return std::make_unique<PrototypeAST>(FnRetTypeID, FnName, ArgTypes, ArgNames, Kind != 0, BinaryPrecedence);
	}

	/// definition ::= 'def' prototype expression

	Expected<std::unique_ptr<FunctionAST>> Parser::ParseTopLevelTypedExpression()
	{
		push_scope();

		if (CurTok == tok_type)
		{
			auto Proto = ParsePrototype();
			if (!Proto)
				return Proto.takeError();

			set_current_function_scope(Proto->get());
			auto E = ParseExpressionList();
			if (!E)
				return E.takeError();

			if (!E->empty())
			{
				auto& P = **Proto;

				PrototypeAST_list.push_back(std::move(*Proto));
				return std::make_unique<FunctionAST>(P, std::move(*E));
			}
			return make_error<ParserError>("empty function", m_tokenizer.get_source_location());
		}
		else
		{
			return make_error<ParserError>("expected type", m_tokenizer.get_source_location());
		}

		pop_scope();
	}

	/// external ::= 'extern' prototype

	Expected<std::unique_ptr<PrototypeAST>> Parser::ParseExtern()
	{
		getNextToken(); // eat extern.

		if (auto Prototype = ParsePrototype())
		{
			getNextToken(); // eat ;
			return std::move(*Prototype);
		}
		else
		{
			getNextToken(); // eat ;
			return Prototype.takeError();
		}
	}

	Error Parser::HandleTypedExpression()
	{
		if (auto FnAST = ParseTopLevelTypedExpression())
		{
			FunctionAST_list.push_back(std::move(*FnAST));
			//	if (auto* FnIR = FnAST->codegen(m_context, m_local_context))
			//{
			//	fprintf(stderr, "Read function definition:");
			//	FnIR->print(errs());
			//	fprintf(stderr, "\n");
			//	shllJIT->addModule(std::move(LLVM_Module));
			//	InitializeModuleAndPassManager();
			//}
			return Error::success();
		}
		else
		{
			// Skip token for error recovery.
			getNextToken();
			return FnAST.takeError();
		}
	}

	Error Parser::HandleExtern()
	{
		if (auto ProtoAST = ParseExtern())
		{
			PrototypeAST_list.emplace_back(std::move(*ProtoAST));
			//	if (auto* FnIR = ProtoAST->codegen(m_context, m_local_context))
			//{
			//	fprintf(stderr, "Read extern: ");
			//	FnIR->print(errs());
			//	fprintf(stderr, "\n");
			//	FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
			//}
			return Error::success();
		}
		else
		{
			// Skip token for error recovery.
			getNextToken();
			return ProtoAST.takeError();
		}
	}

	/// top ::= definition | external | expression | ';'

	Error Parser::MainLoop()
	{
		while (true)
		{
			//	fprintf(stderr, "ready> ");
			switch (CurTok)
			{
			case tok_eof: return Error::success();
			case ';': // ignore top-level semicolons.
				getNextToken();
				break;
			case tok_type:
			{
				if (auto err_ = HandleTypedExpression())
					return err_;
				break;
			}
			case tok_extern:
			{
				if (auto err_ = HandleExtern())
					return err_;
				break;
			}
			default:
			{
				auto str_ = (CurTok < 0) ? to_string(CurTok) : string(1, (char)CurTok);
				return make_error<ParserError>("unexpected top level token: "s + str_, m_tokenizer.get_source_location());
			}
			}
		}
		return Error::success();
	}
}; // namespace slljit
