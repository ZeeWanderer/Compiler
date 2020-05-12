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

using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token:int
{
	tok_eof = -1,

	// commands
	tok_type    = -2,
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


//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

//namespace
//{

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		virtual ~ExprAST() = default;

		virtual Value* codegen() = 0;
	};

	//typedef std::list<std::unique_ptr<ExprAST>> ExprList;
	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;

	public:
		NumberExprAST(double Val)
		    : Val(Val)
		{
		}

		Value* codegen() override;
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

		Value* codegen() override;
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

		Value* codegen() override;
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

		Value* codegen() override;
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

		Value* codegen() override;
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

		Value* codegen() override;
	};

	/// IfExprAST - Expression class for if/then/else.
	class IfExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> Cond, Then, Else;

	public:
		IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
		    : Cond(std::move(Cond))
		    , Then(std::move(Then))
		    , Else(std::move(Else))
		{
		}

		Value* codegen() override;
	};

	/// ForExprAST - Expression class for for/in.
	class ForExprAST : public ExprAST
	{
		std::string VarName;
		std::unique_ptr<ExprAST> Start, End, Step, Body;

	public:
		ForExprAST(const std::string& VarName, std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
		    : VarName(VarName)
		    , Start(std::move(Start))
		    , End(std::move(End))
		    , Step(std::move(Step))
		    , Body(std::move(Body))
		{
		}

		Value* codegen() override;
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

		Value* codegen() override;
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
		Value* codegen();
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

	typedef std::list<std::unique_ptr<ExprAST>> ExprList;
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
		Value* codegen();
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
static std::map<char, int> BinopPrecedence;

class Parser
{
protected:
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

	//static std::unique_ptr<ExprAST> ParseExpression();

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
	//	static std::unique_ptr<ExprAST> ParseIfExpr()
	//{
	//	getNextToken(); // eat the if.
	//
	//	// condition.
	//	auto Cond = ParseExpression();
	//	if (!Cond)
	//		return nullptr;
	//
	//	if (CurTok != tok_then)
	//		return LogError("expected then");
	//	getNextToken(); // eat the then
	//
	//	auto Then = ParseExpression();
	//	if (!Then)
	//		return nullptr;
	//
	//	if (CurTok != tok_else)
	//		return LogError("expected else");
	//
	//	getNextToken();
	//
	//	auto Else = ParseExpression();
	//	if (!Else)
	//		return nullptr;
	//
	//	return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
	//}

	/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
	//	static std::unique_ptr<ExprAST> ParseForExpr()
	//{
	//	getNextToken(); // eat the for.
	//
	//	if (CurTok != tok_identifier)
	//		return LogError("expected identifier after for");
	//
	//	std::string IdName = IdentifierStr;
	//	getNextToken(); // eat identifier.
	//
	//	if (CurTok != '=')
	//		return LogError("expected '=' after for");
	//	getNextToken(); // eat '='.
	//
	//	auto Start = ParseExpression();
	//	if (!Start)
	//		return nullptr;
	//	if (CurTok != ',')
	//		return LogError("expected ',' after for start value");
	//	getNextToken();
	//
	//	auto End = ParseExpression();
	//	if (!End)
	//		return nullptr;
	//
	//	// The step value is optional.
	//	std::unique_ptr<ExprAST> Step;
	//	if (CurTok == ',')
	//	{
	//		getNextToken();
	//		Step = ParseExpression();
	//		if (!Step)
	//			return nullptr;
	//	}
	//
	//	if (CurTok != tok_in)
	//		return LogError("expected 'in' after for");
	//	getNextToken(); // eat 'in'.
	//
	//	auto Body = ParseExpression();
	//	if (!Body)
	//		return nullptr;
	//
	//	return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
	//}

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
		//	case tok_if: return ParseIfExpr();
		//	case tok_for: return ParseForExpr();
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
			e_list.push_back(std::move(retval));
			if (CurTok != ';')
				return LogErrorEX("Expected ;");
			getNextToken(); // Eat ;
			if (CurTok == '}')
				break;
		}

		getNextToken(); // Eat }
		return std::move(e_list);
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

		//if (!Kind && FnName == "main" && ArgNames.size() != 0)
		//	return LogErrorP("Invalid number of operands for main function");

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
			//if (auto* FnIR = FnAST->codegen())
			//{
			//	fprintf(stderr, "Read function definition:");
			//	FnIR->print(errs());
			//	fprintf(stderr, "\n");
			//	TheJIT->addModule(std::move(TheModule));
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
			//if (auto* FnIR = ProtoAST->codegen())
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
	//		if (FnAST->codegen())
	//		{
	//			// JIT the module containing the anonymous expression, keeping a handle so
	//			// we can free it later.
	//			auto H = TheJIT->addModule(std::move(TheModule));
	//			InitializeModuleAndPassManager();
	//
	//			// Search the JIT for the __anon_expr symbol.
	//			auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
	//			assert(ExprSymbol && "Function not found");
	//
	//			// Get the symbol's address and cast it to the right type (takes no
	//			// arguments, returns a double) so we can call it as a native function.
	//			double (*FP)() = (double (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
	//			fprintf(stderr, "Evaluated to %f\n", FP());
	//
	//			// Delete the anonymous expression module from the JIT.
	//			TheJIT->removeModule(H);
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
			default: break;
			}
		}
	}

}m_parser;

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst*> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<ShaderJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

Value* LogErrorV(const char* Str)
{
	LogError(Str);
	return nullptr;
}

Function* getFunction(std::string Name)
{
	// First, see if the function has already been added to the current module.
	if (auto* F = TheModule->getFunction(Name))
		return F;

	// If not, check whether we can codegen the declaration from some existing
	// prototype.
	auto FI = FunctionProtos.find(Name);
	if (FI != FunctionProtos.end())
		return static_cast<Function*>(FI->second->codegen());

	// If no existing prototype exists, return null.
	return nullptr;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName)
{
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
}

Value* NumberExprAST::codegen()
{
	return ConstantFP::get(TheContext, APFloat(Val));
}

Value* VariableExprAST::codegen()
{
	// Look this variable up in the function.
	Value* V = NamedValues[Name];
	if (!V)
		return LogErrorV("Unknown variable name");

	// Load the value.
	return Builder.CreateLoad(V, Name.c_str());
}

Value* UnaryExprAST::codegen()
{
	Value* OperandV = Operand->codegen();
	if (!OperandV)
		return nullptr;

	Function* F = getFunction(std::string("unary") + Opcode);
	if (!F)
		return LogErrorV("Unknown unary operator");

	return Builder.CreateCall(F, OperandV, "unop");
}

Value* BinaryExprAST::codegen()
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
		Value* Val = RHS->codegen();
		if (!Val)
			return nullptr;

		// Look up the name.
		Value* Variable = NamedValues[LHSE->getName()];
		if (!Variable)
			return LogErrorV("Unknown variable name");

		Builder.CreateStore(Val, Variable);
		return Val;
	}

	Value* L = LHS->codegen();
	Value* R = RHS->codegen();
	if (!L || !R)
		return nullptr;
	
	switch (Op)
	{
	case '+': return Builder.CreateFAdd(L, R, "addtmp");
	case '-': return Builder.CreateFSub(L, R, "subtmp");
	case '*': return Builder.CreateFMul(L, R, "multmp");
	case '<':
		L = Builder.CreateFCmpULT(L, R, "cmptmp");
		// Convert bool 0/1 to double 0.0 or 1.0
		return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
	case '>':
		L = Builder.CreateFCmpUGT(L, R, "cmptmp");
		// Convert bool 0/1 to double 0.0 or 1.0
		return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
	default: break;
	}

	// If it wasn't a builtin binary operator, it must be a user defined one. Emit
	// a call to it.
	Function* F = getFunction(std::string("binary") + Op);
	assert(F && "binary operator not found!");

	Value* Ops[] = {L, R};
	return Builder.CreateCall(F, Ops, "binop");
}

Value* CallExprAST::codegen()
{
	// Look up the name in the global module table.
	Function* CalleeF = getFunction(Callee);
	if (!CalleeF)
		return LogErrorV("Unknown function referenced");

	// If argument mismatch error.
	if (CalleeF->arg_size() != Args.size())
		return LogErrorV("Incorrect # arguments passed");

	std::vector<Value*> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i)
	{
		ArgsV.push_back(Args[i]->codegen());
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Value* IfExprAST::codegen()
{
	Value* CondV = Cond->codegen();
	if (!CondV)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

	Function* TheFunction = Builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the
	// end of the function.
	BasicBlock* ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
	BasicBlock* ElseBB  = BasicBlock::Create(TheContext, "else");
	BasicBlock* MergeBB = BasicBlock::Create(TheContext, "ifcont");

	Builder.CreateCondBr(CondV, ThenBB, ElseBB);

	// Emit then value.
	Builder.SetInsertPoint(ThenBB);

	Value* ThenV = Then->codegen();
	if (!ThenV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder.GetInsertBlock();

	// Emit else block.
	TheFunction->getBasicBlockList().push_back(ElseBB);
	Builder.SetInsertPoint(ElseBB);

	Value* ElseV = Else->codegen();
	if (!ElseV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = Builder.GetInsertBlock();

	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	Builder.SetInsertPoint(MergeBB);
	PHINode* PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

	PN->addIncoming(ThenV, ThenBB);
	PN->addIncoming(ElseV, ElseBB);
	return PN;
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
Value* ForExprAST::codegen()
{
	Function* TheFunction = Builder.GetInsertBlock()->getParent();

	// Create an alloca for the variable in the entry block.
	AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

	// Emit the start code first, without 'variable' in scope.
	Value* StartVal = Start->codegen();
	if (!StartVal)
		return nullptr;

	// Store the value into the alloca.
	Builder.CreateStore(StartVal, Alloca);

	// Make the new basic block for the loop header, inserting after current
	// block.
	BasicBlock* LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB.
	Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder.SetInsertPoint(LoopBB);

	// Within the loop, the variable is defined equal to the PHI node.  If it
	// shadows an existing variable, we have to restore it, so save it now.
	AllocaInst* OldVal   = NamedValues[VarName];
	NamedValues[VarName] = Alloca;

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	if (!Body->codegen())
		return nullptr;

	// Emit the step value.
	Value* StepVal = nullptr;
	if (Step)
	{
		StepVal = Step->codegen();
		if (!StepVal)
			return nullptr;
	}
	else
	{
		// If not specified, use 1.0.
		StepVal = ConstantFP::get(TheContext, APFloat(1.0));
	}

	// Compute the end condition.
	Value* EndCond = End->codegen();
	if (!EndCond)
		return nullptr;

	// Reload, increment, and restore the alloca.  This handles the case where
	// the body of the loop mutates the variable.
	Value* CurVar  = Builder.CreateLoad(Alloca, VarName.c_str());
	Value* NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
	Builder.CreateStore(NextVar, Alloca);

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");

	// Create the "after loop" block and insert it.
	BasicBlock* AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

	// Insert the conditional branch into the end of LoopEndBB.
	Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// Any new code will be inserted in AfterBB.
	Builder.SetInsertPoint(AfterBB);

	// Restore the unshadowed variable.
	if (OldVal)
		NamedValues[VarName] = OldVal;
	else
		NamedValues.erase(VarName);

	// for expr always returns 0.0.
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}

Value* VarExprAST::codegen()
{
	std::vector<AllocaInst*> OldBindings;

	Function* TheFunction = Builder.GetInsertBlock()->getParent();

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
			InitVal = Init->codegen();
			if (!InitVal)
				return nullptr;
		}
		else
		{ // If not specified, use 0.0.
			InitVal = ConstantFP::get(TheContext, APFloat(0.0));
		}

		AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		retval = Builder.CreateStore(InitVal, Alloca);

		// Remember the old variable binding so that we can restore the binding when
		// we unrecurse.
		OldBindings.push_back(NamedValues[VarName]);

		// Remember this binding.
		NamedValues[VarName] = Alloca;
	}

	// Codegen the body, now that all vars are in scope.
	//Value* BodyVal = Body->codegen();
	//if (!BodyVal)
	//	return nullptr;

	// Pop all our variables from scope.
	//for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
	//	NamedValues[VarNames[i].first] = OldBindings[i];

	// this is assignment operation
	return retval;
}

// Cast to Function*
Value* PrototypeAST::codegen()
{
	// Make the function type:  double(double,double) etc.
	std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(TheContext));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

	Function* F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : F->args())
		Arg.setName(Args[Idx++]);

	return F;
}

Value* ReturnExprAST::codegen()
{
	if (Value* RetVal = Operand->codegen())
	{
		// Finish off the function.
		Builder.CreateRet(RetVal);

		return RetVal;
	}
	return nullptr;
}

// Cast to Function*
Value* FunctionAST::codegen()
{
	Function* TheFunction            = getFunction(Proto.getName());
	if (!TheFunction)
		return nullptr;

	// If this is an operator, install it.
	if (Proto.isBinaryOp())
		BinopPrecedence[Proto.getOperatorName()] = Proto.getBinaryPrecedence();

	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : TheFunction->args())
	{
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

		// Store the initial value into the alloca.
		Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[Arg.getName()] = Alloca;
	}

	for (auto& expression : Body)
	{
		auto test = expression->codegen();
		if (! test)
		{
			// Error reading body, remove function.
			TheFunction->eraseFromParent();

			if (Proto.isBinaryOp())
				BinopPrecedence.erase(Proto.getOperatorName());
			return nullptr;
		}
	}

	//if (Value* RetVal = Body->codegen())
	//{
	//	// Finish off the function.
	//	Builder.CreateRet(RetVal);

	// Validate the generated code, checking for consistency.
	verifyFunction(*TheFunction);

	// Run the optimizer on the function.
	TheFPM->run(*TheFunction);

	return TheFunction;
	//}

	
	
}

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModuleAndPassManager()
{
	// Open a new module.
	TheModule = std::make_unique<Module>("my cool jit", TheContext);
	TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());

	// Promote allocas to registers.
	TheFPM->add(createPromoteMemoryToRegisterPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->add(createInstructionCombiningPass());
	// Reassociate expressions.
	TheFPM->add(createReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->add(createCFGSimplificationPass());

	// TEST PASSES

	TheFPM->add(createLoopVectorizePass());

	TheFPM->add(createSLPVectorizerPass());

	TheFPM->add(createLoadStoreVectorizerPass());

	TheFPM->doInitialization();
}



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

int main()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['='] = 2;
	BinopPrecedence['<'] = 10;
	BinopPrecedence['>'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40; // highest.

	TheJIT = std::make_unique<ShaderJIT>();

	InitializeModuleAndPassManager();

	std::string source_code = R"(
extern putchard(double x);
extern printd(double x);
double main(double a1, double a2, double b1, double b2,)
{
	double a = a1*(a1 + b1);
	double b = a2*(a2 + b2);
	double c = a1*(a1 + b1);
	double d = a2*(a2 + b2);
	return a+b+c+d;
}
)";
	m_parser.set_source(source_code);

	m_parser.parse();

	// TMP CODEGEN KERNEL
	auto [prototypes,functions] = m_parser.get_ast();

	for (auto it = prototypes.begin(); it != prototypes.end(); ++it)
	{
		if (auto* FnIR = static_cast<Function*>((*it)->codegen()))
		{
			fprintf(stderr, "Read prototype: ");
			FnIR->print(errs());
			fprintf(stderr, "\n");
			FunctionProtos[(*it)->getName()] = std::move((*it));
		}
	}

	for (auto it = functions.begin(); it != functions.end(); ++it)
	{
		if (auto* FnIR = static_cast<Function*>((*it)->codegen()))
		{
			fprintf(stderr, "Read function definition:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
			TheJIT->addModule(std::move(TheModule));
			InitializeModuleAndPassManager();
		}
	}
	
	auto ExprSymbol = TheJIT->findSymbol("main");
	assert(ExprSymbol && "Function not found");

	double (*FP)(double, double, double, double) = (double (*)(double, double, double, double))(intptr_t)cantFail(ExprSymbol.getAddress());
	fprintf(stderr, "Evaluated to %f\n", FP(1,2,3,4));

	return 0;
}