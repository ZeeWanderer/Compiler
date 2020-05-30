// Compiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// TODO: nothrow error handling
// TODO: parse funnction arg types
// TODO: More operators
// TODO: 1+ char length operators
// TODO: Branching: if-else,for loop;
// TODO: 1+ char length operators
// TODO: Actully use types(Cast, conversion etc.)
// TODO: convert to OOP in mpsl image.
// TODO: layout as in mpsl
// TODO: remove delayed jit

#include "pch.h"
#include "JIT.h"

using namespace slljit;
namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;
	//===----------------------------------------------------------------------===//
	// Lexer
	//===----------------------------------------------------------------------===//

	// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
	// of these for known things.
	enum Token : int
	{
		tok_eof = -1,

		// commands
		tok_type   = -2,
		tok_extern = -3,

		// primary
		tok_identifier = -4,
		tok_number     = -5,

		// control
		tok_if   = -6,
		tok_else = -8,
		tok_for  = -9,
	};

	class Tokenizer
	{
	protected:
		std::string IdentifierStr;     // Filled in if tok_identifier
		std::string TypeIdentifierStr; // Filled in if tok_type
		double NumVal;                 // Filled in if tok_number
		std::string_view source_code;

		int _getchar()
		{
			static size_t source_idx = 0;
			if (source_idx < source_code.size())
			{
				return source_code[source_idx++];
			}
			else
			{
				return EOF;
			}
		}

		int LastChar = ' ';
		/// gettok - Return the next token from standard input.
	public:
		void set_source(std::string_view source)
		{
			this->source_code = source;
		}

		int gettok()
		{

			// Skip any whitespace.
			while (isspace(LastChar))
				LastChar = _getchar();

			if (isalpha(LastChar))
			{ // identifier: [a-zA-Z][a-zA-Z0-9]*
				IdentifierStr = LastChar;
				while (isalnum((LastChar = _getchar())))
					IdentifierStr += LastChar;

				// BASIC TYPES
				if (IdentifierStr == "double")
				{
					TypeIdentifierStr = IdentifierStr;
					return tok_type;
				}

				// COMMANDS
				if (IdentifierStr == "extern")
					return tok_extern;
				if (IdentifierStr == "if")
					return tok_if;
				if (IdentifierStr == "else")
					return tok_else;
				if (IdentifierStr == "for")
					return tok_for;
				return tok_identifier;
			}

			if (isdigit(LastChar) || LastChar == '.')
			{ // Number: [0-9.]+
				std::string NumStr;
				do
				{
					NumStr += LastChar;
					LastChar = _getchar();
				} while (isdigit(LastChar) || LastChar == '.');

				//	NumVal = strtod(NumStr.c_str(), nullptr);
				std::from_chars(NumStr.c_str(), NumStr.c_str() + NumStr.size(), NumVal);
				return tok_number;
			}

			if (LastChar == '#')
			{
				// Comment until end of line.
				do
					LastChar = _getchar();
				while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

				if (LastChar != EOF)
					return gettok();
			}

			// Check for end of file.  Don't eat the EOF.
			if (LastChar == EOF)
				return tok_eof;

			// Otherwise, just return the character as its ascii value.
			int ThisChar = LastChar;
			LastChar     = _getchar();
			return ThisChar;
		}

		std::string get_identifier()
		{
			return IdentifierStr;
		}

		std::string get_type_identifier()
		{
			return TypeIdentifierStr;
		}

		double get_double_val()
		{
			return NumVal;
		}
	};

	class Context;
	class LocalContext;

	//===----------------------------------------------------------------------===//
	// Abstract Syntax Tree (aka Parse Tree)
	//===----------------------------------------------------------------------===//

	// namespace
	//{

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		virtual ~ExprAST() = default;

		virtual bool bExpectSemicolon()
		{
			return true;
		}

		virtual bool bIsNoOp()
		{
			return false;
		}

		virtual Value* codegen(Context& m_context, LocalContext& m_local_context) = 0;
	};

	typedef std::list<std::unique_ptr<ExprAST>> ExprList;

	class NoOpAST : public ExprAST
	{

	public:
		NoOpAST()
		{
		}

		virtual bool bIsNoOp() override;

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	// typedef std::list<std::unique_ptr<ExprAST>> ExprList;
	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;

	public:
		NumberExprAST(double Val)
		    : Val(Val)
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST
	{
		std::string Name;

	public:
		VariableExprAST(const std::string& Name)
		    : Name(Name)
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
		const std::string& getName() const
		{
			return Name;
		}
	};

	/// UnaryExprAST - Expression class for a unary operator.
	class UnaryExprAST : public ExprAST
	{
		char Opcode;
		std::unique_ptr<ExprAST> Operand;

	public:
		UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
		    : Opcode(Opcode)
		    , Operand(std::move(Operand))
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};
	/// UnaryExprAST - Expression class for a unary operator.
	class ReturnExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> Operand;

	public:
		ReturnExprAST(std::unique_ptr<ExprAST> Operand)
		    : Operand(std::move(Operand))
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST
	{
		char Op;
		std::unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
		    : Op(Op)
		    , LHS(std::move(LHS))
		    , RHS(std::move(RHS))
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
		    : Callee(Callee)
		    , Args(std::move(Args))
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// IfExprAST - Expression class for if/else.
	class IfExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> Cond;
		ExprList Then;
		ExprList Else;

	public:
		IfExprAST(std::unique_ptr<ExprAST> Cond, ExprList Then, ExprList Else)
		    : Cond(std::move(Cond))
		    , Then(std::move(Then))
		    , Else(std::move(Else))
		{
		}

		bool bExpectSemicolon() override;

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// ForExprAST - Expression class for for/in.
	class ForExprAST : public ExprAST
	{
		//	std::string VarName;
		std::unique_ptr<ExprAST> VarInit, Cond;
		ExprList Body;
		std::unique_ptr<ExprAST> EndExpr;

	public:
		ForExprAST(std::unique_ptr<ExprAST> VarInit, std::unique_ptr<ExprAST> Cond, ExprList Body, std::unique_ptr<ExprAST> EndExpr)
		    : VarInit(std::move(VarInit))
		    , Cond(std::move(Cond))
		    , Body(std::move(Body))
		    , EndExpr(std::move(EndExpr))
		{
		}

		bool bExpectSemicolon() override;

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// VarExprAST - Expression class for var/in
	class VarExprAST : public ExprAST
	{
		std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

	public:
		VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames)
		    : VarNames(std::move(VarNames))
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes), as well as if it is an operator.
	class PrototypeAST : public ExprAST
	{
		std::string Name;
		std::vector<std::string> Args;
		bool IsOperator;
		unsigned Precedence; // Precedence if a binary op.

	public:
		PrototypeAST(const std::string& Name, std::vector<std::string> Args, bool IsOperator = false, unsigned Prec = 0)
		    : Name(Name)
		    , Args(std::move(Args))
		    , IsOperator(IsOperator)
		    , Precedence(Prec)
		{
		}

		// Cast to Function*
		Value* codegen(Context& m_context, LocalContext& m_local_context);
		const std::string& getName() const
		{
			return Name;
		}

		bool isUnaryOp() const
		{
			return IsOperator && Args.size() == 1;
		}
		bool isBinaryOp() const
		{
			return IsOperator && Args.size() == 2;
		}

		char getOperatorName() const
		{
			assert(isUnaryOp() || isBinaryOp());
			return Name[Name.size() - 1];
		}

		unsigned getBinaryPrecedence() const
		{
			return Precedence;
		}
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST
	{
		PrototypeAST& Proto;
		ExprList Body;

	public:
		FunctionAST(PrototypeAST& Proto, ExprList Body)
		    : Proto(Proto)
		    , Body(std::move(Body))
		{
		}

		// Cast to Function*
		Value* codegen(Context& m_context, LocalContext& m_local_context);
	};

	//} // end anonymous namespace

	//===----------------------------------------------------------------------===//
	// Parser
	//===----------------------------------------------------------------------===//

	/// LogError* - These are little helper functions for error handling.
	std::unique_ptr<ExprAST> LogError(const char* Str)
	{
		fprintf(stderr, "Error: %s\n", Str);
		return nullptr;
	}

	std::unique_ptr<PrototypeAST> LogErrorP(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	std::unique_ptr<FunctionAST> LogErrorF(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	ExprList LogErrorEX(const char* Str)
	{
		LogError(Str);
		return {};
	}

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

		Tokenizer m_tokenizer;

		/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
		/// token the parser is looking at.  getNextToken reads another token from the
		/// lexer and updates CurTok with its results.
		int CurTok;
		int getNextToken()
		{
			return CurTok = m_tokenizer.gettok();
		}

	public:
		Parser(std::map<char, int>& BinopPrecedence)
		    : BinopPrecedence(BinopPrecedence)
		{
		}

		void set_source(std::string_view source)
		{
			m_tokenizer.set_source(source);
			getNextToken();
		}

		auto get_ast()
		{
			return std::pair{std::move(PrototypeAST_list), std::move(FunctionAST_list)};
		}

		std::list<std::unique_ptr<FunctionAST>> get_function_ast()
		{
			return std::move(FunctionAST_list);
		}

		std::list<std::unique_ptr<PrototypeAST>> get_prototype_ast()
		{
			return std::move(PrototypeAST_list);
		}

		void parse()
		{
			MainLoop();
		}

	protected:
		/// GetTokPrecedence - Get the precedence of the pending binary operator token.
		int GetTokPrecedence()
		{
			if (!isascii(CurTok))
				return -1;

			// Make sure it's a declared binop.
			int TokPrec = BinopPrecedence[CurTok];
			if (TokPrec <= 0)
				return -1;
			return TokPrec;
		}

		//	static std::unique_ptr<ExprAST> ParseExpression();

		/// numberexpr ::= number
		std::unique_ptr<ExprAST> ParseNumberExpr()
		{
			auto Result = std::make_unique<NumberExprAST>(m_tokenizer.get_double_val());
			getNextToken(); // consume the number
			return std::move(Result);
		}

		/// parenexpr ::= '(' expression ')'
		std::unique_ptr<ExprAST> ParseParenExpr()
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
		std::unique_ptr<ExprAST> ParseIdentifierExpr()
		{
			auto IdName = m_tokenizer.get_identifier();

			getNextToken(); // eat identifier.

			// return expression
			if (IdName == "return")
			{
				auto expr = ParseExpression();
				return std::make_unique<ReturnExprAST>(std::move(expr));
			}

			if (CurTok != '(') // Simple variable ref.
				return std::make_unique<VariableExprAST>(IdName);

			// Call.
			getNextToken(); // eat (
			std::vector<std::unique_ptr<ExprAST>> Args;
			if (CurTok != ')')
			{
				while (true)
				{
					if (auto Arg = ParseExpression())
						Args.push_back(std::move(Arg));
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

			return std::make_unique<CallExprAST>(IdName, std::move(Args));
		}

		/// ifexpr ::= 'if' expression 'then' expression 'else' expression
		std::unique_ptr<ExprAST> ParseIfExpr()
		{
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
		}

		/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
		std::unique_ptr<ExprAST> ParseForExpr()
		{
			getNextToken(); // eat the for.

			if (CurTok != '(')
				return LogError("expected ( after for");
			getNextToken(); // eat (

			// var init expr.
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
		}

		/// varexpr ::= 'var' identifier ('=' expression)?
		//                    (',' identifier ('=' expression)?)* 'in' expression
		std::unique_ptr<ExprAST> ParseVarExpr()
		{
			getNextToken(); // eat type.

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

				VarNames.push_back(std::make_pair(Name, std::move(Init)));

				// End of var list, exit loop.
				if (CurTok != ',')
					break;
				getNextToken(); // eat the ','.

				if (CurTok != tok_identifier)
					return LogError("expected identifier list after var");
			}

			return std::make_unique<VarExprAST>(std::move(VarNames));
		}

		/// primary
		///   ::= identifierexpr
		///   ::= numberexpr
		///   ::= parenexpr
		///   ::= ifexpr
		///   ::= forexpr
		///   ::= varexpr
		std::unique_ptr<ExprAST> ParsePrimary()
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
		std::unique_ptr<ExprAST> ParseUnary()
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
		std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
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
		std::unique_ptr<ExprAST> ParseExpression()
		{
			//	TODO: move where it belongs
			if (CurTok == ';')
				return std::make_unique<NoOpAST>();

			auto LHS = ParseUnary();
			if (!LHS)
				return nullptr;

			return ParseBinOpRHS(0, std::move(LHS));
		}
		ExprList ParseExpressionList()
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
		std::unique_ptr<PrototypeAST> ParsePrototype()
		{
			std::string FnName;

			unsigned Kind             = 0; // 0 = identifier, 1 = unary, 2 = binary.
			unsigned BinaryPrecedence = 30;

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
			while (getNextToken() == tok_type)
			{
				if (getNextToken() == tok_identifier)
					ArgNames.push_back(m_tokenizer.get_identifier());
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

			return std::make_unique<PrototypeAST>(FnName, ArgNames, Kind != 0, BinaryPrecedence);
		}

		/// definition ::= 'def' prototype expression
		std::unique_ptr<FunctionAST> ParseTopLevelTypedExpression()
		{
			getNextToken(); // eat type.

			if (CurTok == tok_identifier)
			{
				auto Proto = ParsePrototype();
				if (!Proto)
					return nullptr;

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
		}

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
		std::unique_ptr<PrototypeAST> ParseExtern()
		{
			getNextToken(); // eat extern.
			return ParsePrototype();
		}

		void HandleTypedExpression()
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

		void HandleExtern()
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
		void MainLoop()
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
	};

	//===----------------------------------------------------------------------===//
	// Code Generation
	//===----------------------------------------------------------------------===//

	class Context
	{
	public:
		LLVMContext LLVM_Context;
		IRBuilder<> LLVM_Builder;
		std::unique_ptr<ShaderJIT> shllJIT;

	public:
		Context()
		    : LLVM_Builder(LLVM_Context)
		{
			shllJIT = std::make_unique<ShaderJIT>();
		}
	};

	class LocalContext
	{
	public:
		std::unique_ptr<Module> LLVM_Module;
		std::map<std::string, AllocaInst*> NamedValues;
		std::unique_ptr<legacy::FunctionPassManager> LLVM_FPM;
		std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
		std::map<char, int> BinopPrecedence;
		VModuleKey module_key;

	public:
		LocalContext(Context& m_context)
		{
			BinopPrecedence = {{'=', 2}, {'<', 10}, {'>', 10}, {'+', 20}, {'-', 20}, {'*', 40}};

			// Open a new module.
			LLVM_Module = std::make_unique<Module>("my cool jit", m_context.LLVM_Context);
			LLVM_Module->setDataLayout(m_context.shllJIT->getTargetMachine().createDataLayout());

			// Create a new pass manager attached to it.
			LLVM_FPM = std::make_unique<legacy::FunctionPassManager>(LLVM_Module.get());

			LLVM_FPM->add(createPromoteMemoryToRegisterPass()); //	SSA conversion
			LLVM_FPM->add(createCFGSimplificationPass());       //	Dead code elimination
			LLVM_FPM->add(createSROAPass());
			LLVM_FPM->add(createLoadStoreVectorizerPass());
			LLVM_FPM->add(createLoopSimplifyCFGPass());
			LLVM_FPM->add(createLoopVectorizePass());
			LLVM_FPM->add(createLoopUnrollPass());
			LLVM_FPM->add(createConstantPropagationPass());
			LLVM_FPM->add(createGVNPass());                     // Eliminate Common SubExpressions.
			LLVM_FPM->add(createNewGVNPass());                  //	Global value numbering
			LLVM_FPM->add(createReassociatePass());             // Reassociate expressions.
			LLVM_FPM->add(createPartiallyInlineLibCallsPass()); //	Inline standard calls
			LLVM_FPM->add(createDeadCodeEliminationPass());
			LLVM_FPM->add(createCFGSimplificationPass());    //	Cleanup
			LLVM_FPM->add(createInstructionCombiningPass()); // Do simple "peephole" optimizations and bit-twiddling optzns.
			LLVM_FPM->add(createSLPVectorizerPass());
			LLVM_FPM->add(createFlattenCFGPass()); //	Flatten the control flow graph.

			LLVM_FPM->doInitialization();
		}

		void set_key(VModuleKey module_key)
		{
			this->module_key = module_key;
		}

		auto get_key()
		{
			return this->module_key;
		}
	};

	Value* LogErrorV(const char* Str)
	{
		LogError(Str);
		return nullptr;
	}

	Function* getFunction(std::string Name, Context& m_context, LocalContext& m_local_context)
	{
		// First, see if the function has already been added to the current module.
		if (auto* F = m_local_context.LLVM_Module->getFunction(Name))
			return F;

		// If not, check whether we can codegen the declaration from some existing
		// prototype.
		auto FI = m_local_context.FunctionProtos.find(Name);
		if (FI != m_local_context.FunctionProtos.end())
			return static_cast<Function*>(FI->second->codegen(m_context, m_local_context));

		// If no existing prototype exists, return null.
		return nullptr;
	}

	/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	/// the function.  This is used for mutable variables etc.
	static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, Context& m_context, LocalContext& m_local_context)
	{
		IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(Type::getDoubleTy(m_context.LLVM_Context), nullptr, VarName);
	}

	bool NoOpAST::bIsNoOp()
	{
		return true;
	}

	Value* NoOpAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		return ConstantFP::get(m_context.LLVM_Context, APFloat(0.0));
	}

	Value* NumberExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		return ConstantFP::get(m_context.LLVM_Context, APFloat(Val));
	}

	Value* VariableExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Look this variable up in the function.
		GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(Name);
		if (gVar)
		{
			return m_context.LLVM_Builder.CreateLoad(gVar, Name);
		}

		Value* V = m_local_context.NamedValues[Name];
		if (!V)
			return LogErrorV("Unknown variable name");

		// Load the value.
		return m_context.LLVM_Builder.CreateLoad(V, Name.c_str());
	}

	Value* UnaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Value* OperandV = Operand->codegen(m_context, m_local_context);
		if (!OperandV)
			return nullptr;

		Function* F = getFunction(std::string("unary") + Opcode, m_context, m_local_context);
		if (!F)
			return LogErrorV("Unknown unary operator");

		return m_context.LLVM_Builder.CreateCall(F, OperandV, "unop");
	}

	Value* BinaryExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Special case '=' because we don't want to emit the LHS as an expression.
		if (Op == '=')
		{
			// Assignment requires the LHS to be an identifier.
			// This assume we're building without RTTI because LLVM builds that way by
			// default.  If you build LLVM with RTTI this can be changed to a
			// dynamic_cast for automatic error checking.
			VariableExprAST* LHSE = static_cast<VariableExprAST*>(LHS.get());
			if (!LHSE)
				return LogErrorV("destination of '=' must be a variable");
			// Codegen the RHS.
			Value* Val = RHS->codegen(m_context, m_local_context);
			if (!Val)
				return nullptr;

			// Look up global
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(LHSE->getName());
			if (gVar)
			{
				if (gVar->isConstant())
					return LogErrorV("Trying to store in constant global");

				m_context.LLVM_Builder.CreateStore(Val, gVar);
				return Val;
			}

			// Look up variable.
			Value* Variable = m_local_context.NamedValues[LHSE->getName()];
			if (!Variable)
				return LogErrorV("Unknown variable name");

			m_context.LLVM_Builder.CreateStore(Val, Variable);
			return Val;
		}

		Value* L = LHS->codegen(m_context, m_local_context);
		Value* R = RHS->codegen(m_context, m_local_context);
		if (!L || !R)
			return nullptr;

		switch (Op)
		{
		case '+': return m_context.LLVM_Builder.CreateFAdd(L, R, "addtmp");
		case '-': return m_context.LLVM_Builder.CreateFSub(L, R, "subtmp");
		case '*': return m_context.LLVM_Builder.CreateFMul(L, R, "multmp");
		case '<':
			L = m_context.LLVM_Builder.CreateFCmpULT(L, R, "cmptmp");
			// Convert bool 0/1 to double 0.0 or 1.0
			return m_context.LLVM_Builder.CreateUIToFP(L, Type::getDoubleTy(m_context.LLVM_Context), "booltmp");
		case '>':
			L = m_context.LLVM_Builder.CreateFCmpUGT(L, R, "cmptmp");
			// Convert bool 0/1 to double 0.0 or 1.0
			return m_context.LLVM_Builder.CreateUIToFP(L, Type::getDoubleTy(m_context.LLVM_Context), "booltmp");
		default: break;
		}

		// If it wasn't a builtin binary operator, it must be a user defined one. Emit
		// a call to it.
		Function* F = getFunction(std::string("binary") + Op, m_context, m_local_context);
		assert(F && "binary operator not found!");

		Value* Ops[] = {L, R};
		return m_context.LLVM_Builder.CreateCall(F, Ops, "binop");
	}

	Value* CallExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Look up the name in the global module table.
		Function* CalleeF = getFunction(Callee, m_context, m_local_context);
		if (!CalleeF)
			return LogErrorV("Unknown function referenced");

		// If argument mismatch error.
		if (CalleeF->arg_size() != Args.size())
			return LogErrorV("Incorrect # arguments passed");

		std::vector<Value*> ArgsV;
		for (unsigned i = 0, e = Args.size(); i != e; ++i)
		{
			ArgsV.push_back(Args[i]->codegen(m_context, m_local_context));
			if (!ArgsV.back())
				return nullptr;
		}

		return m_context.LLVM_Builder.CreateCall(CalleeF, ArgsV, "calltmp");
	}

	bool IfExprAST::bExpectSemicolon()
	{
		return false;
	}

	Value* IfExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Value* CondV = Cond->codegen(m_context, m_local_context);
		if (!CondV)
			return nullptr;

		// Convert condition to a bool by comparing non-equal to 0.0.
		CondV = m_context.LLVM_Builder.CreateFCmpONE(CondV, ConstantFP::get(m_context.LLVM_Context, APFloat(0.0)), "ifcond");

		Function* TheFunction = m_context.LLVM_Builder.GetInsertBlock()->getParent();

		// Create blocks for the then and else cases.  Insert the 'then' block at the
		// end of the function.
		BasicBlock* IfBB    = BasicBlock::Create(m_context.LLVM_Context, "if", TheFunction);
		BasicBlock* ElseBB  = BasicBlock::Create(m_context.LLVM_Context, "else");
		BasicBlock* MergeBB = BasicBlock::Create(m_context.LLVM_Context, "ifcont");

		m_context.LLVM_Builder.CreateCondBr(CondV, IfBB, ElseBB);

		// Emit then value.
		m_context.LLVM_Builder.SetInsertPoint(IfBB);

		for (auto& then_expr : Then)
		{
			then_expr->codegen(m_context, m_local_context);
		}

		//	Value* ThenV = Then->codegen(m_context, m_local_context);
		//	if (!ThenV)
		//	return nullptr;

		m_context.LLVM_Builder.CreateBr(MergeBB);
		// Codegen of 'Then' can change the current block, update IfBB for the PHI.
		IfBB = m_context.LLVM_Builder.GetInsertBlock();

		// Emit else block.
		TheFunction->getBasicBlockList().push_back(ElseBB);
		m_context.LLVM_Builder.SetInsertPoint(ElseBB);

		for (auto& else_expr : Else)
		{
			else_expr->codegen(m_context, m_local_context);
		}

		//	Value* ElseV = Else->codegen(m_context, m_local_context);
		//	if (!ElseV)
		//	return nullptr;

		m_context.LLVM_Builder.CreateBr(MergeBB);
		// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
		ElseBB = m_context.LLVM_Builder.GetInsertBlock();

		// Emit merge block.
		TheFunction->getBasicBlockList().push_back(MergeBB);
		m_context.LLVM_Builder.SetInsertPoint(MergeBB);
		//	PHINode* PN = LLVM_Builder.CreatePHI(Type::getDoubleTy(LLVM_Context), 2, "iftmp");

		//	PN->addIncoming(ThenV, IfBB);
		//	PN->addIncoming(ElseV, ElseBB);
		return CondV;
	}

	bool ForExprAST::bExpectSemicolon()
	{
		return false;
	}

	// Output for-loop as:
	//   var = alloca double
	//   ...
	//   start = startexpr
	//   store start -> var
	//   goto loop
	// loop:
	//   ...
	//   bodyexpr
	//   ...
	// loopend:
	//   step = stepexpr
	//   endcond = endexpr
	//
	//   curvar = load var
	//   nextvar = curvar + step
	//   store nextvar -> var
	//   br endcond, loop, endloop
	// outloop:
	Value* ForExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = m_context.LLVM_Builder.GetInsertBlock()->getParent();

		// generate init variable
		if (VarInit)
			VarInit->codegen(m_context, m_local_context);

		// Make the new basic block for the loop header, inserting after current
		// block.
		BasicBlock* LoopBB = BasicBlock::Create(m_context.LLVM_Context, "loop", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* LoopBobyBB = BasicBlock::Create(m_context.LLVM_Context, "loop_body", TheFunction);
		// Create the "after loop" block and insert it.
		BasicBlock* AfterBB = BasicBlock::Create(m_context.LLVM_Context, "afterloop", TheFunction);

		// Insert an explicit fall through from the current block to the LoopBB.
		m_context.LLVM_Builder.CreateBr(LoopBB);

		// Start insertion in LoopBB.
		m_context.LLVM_Builder.SetInsertPoint(LoopBB);

		if (!Cond->bIsNoOp())
		{
			Value* condVal = Cond->codegen(m_context, m_local_context);
			// Convert condition to a bool by comparing non-equal to 0.0.
			condVal = m_context.LLVM_Builder.CreateFCmpONE(condVal, ConstantFP::get(m_context.LLVM_Context, APFloat(0.0)), "loopcond");

			// Insert the conditional branch into the end of LoopEndBB.
			m_context.LLVM_Builder.CreateCondBr(condVal, LoopBobyBB, AfterBB);
		}

		m_context.LLVM_Builder.SetInsertPoint(LoopBobyBB);

		for (auto& expt : Body)
		{
			expt->codegen(m_context, m_local_context);
		}

		EndExpr->codegen(m_context, m_local_context);

		// Insert unconditional brnch to start
		m_context.LLVM_Builder.CreateBr(LoopBB);

		// Any new code will be inserted in AfterBB.
		m_context.LLVM_Builder.SetInsertPoint(AfterBB);
		//	LLVM_Builder.CreateFCmpONE(ConstantFP::get(LLVM_Context, APFloat(0.0)), ConstantFP::get(LLVM_Context, APFloat(0.0)), "dmm");

		// for expr always returns 0.0.
		return ConstantFP::get(m_context.LLVM_Context, APFloat(0.0));
	}

	Value* VarExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		std::vector<AllocaInst*> OldBindings;

		Function* TheFunction = m_context.LLVM_Builder.GetInsertBlock()->getParent();

		// Register all variables and emit their initializer.
		Value* retval = nullptr;
		for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		{
			const std::string& VarName = VarNames[i].first;
			ExprAST* Init              = VarNames[i].second.get();

			// Emit the initializer before adding the variable to scope, this prevents
			// the initializer from referencing the variable itself, and permits stuff
			// like this:
			//  var a = 1 in
			//    var a = a in ...   # refers to outer 'a'.
			Value* InitVal;
			if (Init)
			{
				InitVal = Init->codegen(m_context, m_local_context);
				if (!InitVal)
					return nullptr;
			}
			else
			{ // If not specified, use 0.0.
				InitVal = ConstantFP::get(m_context.LLVM_Context, APFloat(0.0));
			}

			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName, m_context, m_local_context);
			retval             = m_context.LLVM_Builder.CreateStore(InitVal, Alloca);

			// Remember the old variable binding so that we can restore the binding when
			// we unrecurse.
			OldBindings.push_back(m_local_context.NamedValues[VarName]);

			// Remember this binding.
			m_local_context.NamedValues[VarName] = Alloca;
		}

		// Codegen the body, now that all vars are in scope.
		//	Value* BodyVal = Body->codegen(m_context, m_local_context);
		//	if (!BodyVal)
		//	return nullptr;

		// Pop all our variables from scope.
		//	for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		//	NamedValues[VarNames[i].first] = OldBindings[i];

		// this is assignment operation
		return retval;
	}

	// Cast to Function*
	Value* PrototypeAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		// Make the function type:  double(double,double) etc.
		std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(m_context.LLVM_Context));
		FunctionType* FT = FunctionType::get(Type::getDoubleTy(m_context.LLVM_Context), Doubles, false);

		Function* F = Function::Create(FT, Function::ExternalLinkage, Name, m_local_context.LLVM_Module.get());

		// Set names for all arguments.
		unsigned Idx = 0;
		for (auto& Arg : F->args())
			Arg.setName(Args[Idx++]);

		return F;
	}

	Value* ReturnExprAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		if (Value* RetVal = Operand->codegen(m_context, m_local_context))
		{
			// Finish off the function.
			m_context.LLVM_Builder.CreateRet(RetVal);

			return RetVal;
		}
		return nullptr;
	}

	// Cast to Function*
	Value* FunctionAST::codegen(Context& m_context, LocalContext& m_local_context)
	{
		Function* TheFunction = getFunction(Proto.getName(), m_context, m_local_context);
		if (!TheFunction)
			return nullptr;

		// If this is an operator, install it.
		if (Proto.isBinaryOp())
			m_local_context.BinopPrecedence[Proto.getOperatorName()] = Proto.getBinaryPrecedence();

		// Create a new basic block to start insertion into.
		BasicBlock* BB = BasicBlock::Create(m_context.LLVM_Context, "entry", TheFunction);
		m_context.LLVM_Builder.SetInsertPoint(BB);

		// Record the function arguments in the NamedValues map.
		m_local_context.NamedValues.clear();
		for (auto& Arg : TheFunction->args())
		{
			// Create an alloca for this variable.
			AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), m_context, m_local_context);

			// Store the initial value into the alloca.
			m_context.LLVM_Builder.CreateStore(&Arg, Alloca);

			// Add arguments to variable symbol table.
			m_local_context.NamedValues[std::string(Arg.getName())] = Alloca;
		}

		for (auto& expression : Body)
		{
			auto test = expression->codegen(m_context, m_local_context);
			if (!test)
			{
				// Error reading body, remove function.
				TheFunction->eraseFromParent();

				if (Proto.isBinaryOp())
					m_local_context.BinopPrecedence.erase(Proto.getOperatorName());
				return nullptr;
			}
		}

		//	if (Value* RetVal = Body->codegen(m_context, m_local_context))
		//{
		//	// Finish off the function.
		//	LLVM_Builder.CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		// Run the optimizer on the function.
		m_local_context.LLVM_FPM->run(*TheFunction);

		return TheFunction;
		//}
	}

	class Layout;

	class CodeGen
	{
	public:
		void compile_layout(Context& m_context, LocalContext& m_local_context, Layout& m_layout);
		void compile(std::list<std::unique_ptr<PrototypeAST>> prototypes, std::list<std::unique_ptr<FunctionAST>> functions, Context& m_context, LocalContext& m_local_context);
	};

	enum LayoutVarTypes
	{
		Kdouble = 0
	};

	struct GlobalDefinition
	{
		LayoutVarTypes type;
		size_t offset;
	};

	struct ConstantGlobalDefinition
	{
		double value;
		LayoutVarTypes type;
	};

	class Layout
	{
	public:
		set<string> names;
		map<string, GlobalDefinition> globals;
		map<string, struct ConstantGlobalDefinition> constant_globals;
		void addMember(string name, LayoutVarTypes type, size_t offset)
		{
			globals.emplace(pair{name, GlobalDefinition{type, offset}});
			names.insert(name);
		}

		void addConsatantMember(string name, double value, LayoutVarTypes type)
		{
			constant_globals.emplace(pair{name, ConstantGlobalDefinition{value, type}});
			names.insert(name);
		}
	};

	class Program
	{
	private:
		Context& m_context;
		LocalContext m_local_context;
		Layout m_layout;
		vector<pair<intptr_t, GlobalDefinition>> runtime_globals;
		double (*main_func)() = nullptr;

	public:
		Program(Context& m_context)
		    : m_context(m_context)
		    , m_local_context(m_context)
		{
		}
		void compile(string body, Layout layout);
		double run(intptr_t data);
	};

	void Program::compile(string body, Layout layout)
	{
		m_layout = layout;
		Parser m_parser(m_local_context.BinopPrecedence);
		CodeGen m_codegen;
		m_parser.set_source(body);

		m_parser.parse();

		// TMP CODEGEN KERNEL
		auto [prototypes, functions] = m_parser.get_ast();
		m_codegen.compile_layout(m_context, m_local_context, m_layout);
		m_codegen.compile(std::move(prototypes), std::move(functions), m_context, m_local_context);

		runtime_globals.reserve(m_layout.globals.size());
		for (auto& global : m_layout.globals)
		{
			auto symbol = m_context.shllJIT->findSymbol(global.first, m_local_context.module_key);
			auto ptr    = (intptr_t)cantFail(symbol.getAddress());
			runtime_globals.emplace_back(pair{ptr, global.second});
		}
		auto symbol = m_context.shllJIT->findSymbol("main", m_local_context.module_key);
		main_func   = (double (*)())(intptr_t)cantFail(symbol.getAddress());
	}

	double Program::run(intptr_t data)
	{
		// TODO: generate loader in llvm as offsets are known at compile time
		for (auto& global : runtime_globals)
		{
			auto value_ptr  = reinterpret_cast<double*>(data + global.second.offset);
			auto value      = *value_ptr;
			auto global_ptr = reinterpret_cast<double*>(global.first);
			*global_ptr     = value;
		}
		auto retval = main_func();

		// TODO: mutate globals directly from llvm instead of this copyback.
		//	To do that make global a pointer and store there a pointer to struct.
		for (auto& global : runtime_globals)
		{
			auto value_ptr  = reinterpret_cast<double*>(data + global.second.offset);
			auto global_ptr = reinterpret_cast<double*>(global.first);
			*value_ptr      = *global_ptr;
		}
		return retval;
	}

	void CodeGen::compile_layout(Context& m_context, LocalContext& m_local_context, Layout& m_layout)
	{
		for (auto& global : m_layout.constant_globals)
		{
			m_local_context.LLVM_Module->getOrInsertGlobal(global.first, Type::getDoubleTy(m_context.LLVM_Context));
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.first);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantFP::get(m_context.LLVM_Context, APFloat(global.second.value)));
			gVar->setConstant(true);
		}
		for (auto& global : m_layout.globals)
		{
			m_local_context.LLVM_Module->getOrInsertGlobal(global.first, Type::getDoubleTy(m_context.LLVM_Context));
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.first);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantFP::get(m_context.LLVM_Context, APFloat(0.0)));
		}
	}

	void CodeGen::compile(std::list<std::unique_ptr<PrototypeAST>> prototypes, std::list<std::unique_ptr<FunctionAST>> functions, Context& m_context, LocalContext& m_local_context)
	{
		for (auto it = prototypes.begin(); it != prototypes.end(); ++it)
		{
			if (auto* FnIR = static_cast<Function*>((*it)->codegen(m_context, m_local_context)))
			{
				fprintf(stderr, "Read prototype: ");
				FnIR->print(errs());
				fprintf(stderr, "\n");
				m_local_context.FunctionProtos[(*it)->getName()] = std::move((*it));
			}
		}

		for (auto it = functions.begin(); it != functions.end(); ++it)
		{
			if (auto* FnIR = static_cast<Function*>((*it)->codegen(m_context, m_local_context)))
			{
				fprintf(stderr, "Read function definition:");
				FnIR->print(errs());
				fprintf(stderr, "\n");
			}
		}

		m_local_context.LLVM_Module->dump();
		auto key = m_context.shllJIT->addModule(std::move(m_local_context.LLVM_Module));
		m_local_context.set_key(key);
	}

} // namespace slljit

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X)
{
	fputc((char)X, stderr);
	return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X)
{
	fprintf(stderr, "%f\n", X);
	return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
#include <sstream>
#include <iomanip>
#include <iostream>

std::string hexStr(unsigned char* data, int len)
{
	std::stringstream ss;
	ss << std::hex;

	for (int i(0); i < len; ++i)
		ss << std::setw(2) << std::setfill('0') << (int)data[i];

	return ss.str();
}

struct Data
{
	double x;
};

int main()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	Context m_context;
	Program m_program(m_context);
	Layout m_layout;
	m_layout.addMember("x", ::Kdouble, offsetof(Data, x));
	m_layout.addConsatantMember("v", 5, ::Kdouble);
	//	InitializeModuleAndPassManager();

	std::string source_code = R"(
extern putchard(double x);
extern printd(double x);
double test(double a1, double a2, double b1, double b2,  double c1, double c2,  double d1, double d2)
{
	double c = a1+a2+b1+b2+c1+c2+d1+d2;
	return c;
}
double main()
{
	double a = x+v*2;
	x = 4;
	return a;
}
)";
	m_program.compile(source_code, m_layout);

	Data data{3.0};

	auto retval = m_program.run((intptr_t)&data);

	return 0;
}
