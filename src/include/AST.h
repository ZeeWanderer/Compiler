#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

namespace slljit
{
	using namespace llvm;
	using namespace std;

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

		virtual bool bIsTerminator()
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

		virtual bool bIsNoOp() override
		{
			return true;
		}

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
		    : Opcode(Opcode), Operand(std::move(Operand))
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

		virtual bool bIsTerminator() override
		{
			return true;
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
		    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS))
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
		    : Callee(Callee), Args(std::move(Args))
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
		    : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else))
		{
		}

		bool bExpectSemicolon() override
		{
			return false;
		}

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
		    : VarInit(std::move(VarInit)), Cond(std::move(Cond)), Body(std::move(Body)), EndExpr(std::move(EndExpr))
		{
		}

		bool bExpectSemicolon() override
		{
			return false;
		}

		// Output for-loop as:
		//   g_var = alloca double
		//   ...
		//   start = startexpr
		//   store start -> g_var
		//   goto loop
		// loop:
		//   ...
		//   bodyexpr
		//   ...
		// loopend:
		//   step = stepexpr
		//   endcond = endexpr
		//
		//   curvar = load g_var
		//   nextvar = curvar + step
		//   store nextvar -> g_var
		//   br endcond, loop, endloop
		// outloop:
		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// VarExprAST - Expression class for g_var/in
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
		    : Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec)
		{
		}

		// Cast to Function*
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
		    : Proto(Proto), Body(std::move(Body))
		{
		}

		// Cast to Function*
		// Cast to Function*
		Value* codegen(Context& m_context, LocalContext& m_local_context);

		const std::string& getName() const;
	};

	/// LogError* - These are little helper functions for error handling.
	std::unique_ptr<ExprAST> LogError(const char* Str);

	std::unique_ptr<PrototypeAST> LogErrorP(const char* Str);

	std::unique_ptr<FunctionAST> LogErrorF(const char* Str);

	ExprList LogErrorEX(const char* Str);

	Value* LogErrorV(const char* Str);

	Function* getFunction(std::string Name, Context& m_context, LocalContext& m_local_context);
	/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	/// the function.  This is used for mutable variables etc.
	static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, Context& m_context, LocalContext& m_local_context);
}; // namespace slljit
