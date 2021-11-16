#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

#include "Types.h"

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
		TypeID type_ = none;

		ExprAST() = default;

		ExprAST(TypeID type_)
		    : type_(type_){};

		virtual TypeID getType() const
		{
			return type_;
		}

		virtual TypeID getRetType() const
		{
			return none;
		}

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
		    : ExprAST(none)
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
		union
		{
			bool ValB;
			double ValD;
			int64_t ValSI64;
		};

	public:
		NumberExprAST(bool Val)
		    : ExprAST(boolTyID), ValB(Val)
		{
		}

		NumberExprAST(double Val)
		    : ExprAST(doubleTyID), ValD(Val)
		{
		}

		NumberExprAST(int64_t Val)
		    : ExprAST(int64TyID), ValSI64(Val)
		{
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST
	{
		std::string Name;

	public:
		VariableExprAST(const std::string& Name, TypeID type_)
		    : Name(Name), ExprAST(type_)
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
			type_ = this->Operand->getType();
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};
	/// UnaryExprAST - Expression class for a unary operator.
	class ReturnExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> Operand;

	public:
		ReturnExprAST(std::unique_ptr<ExprAST> Operand, TypeID type_)
		    : ExprAST(type_), Operand(std::move(Operand))
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
			auto LHSTy = this->LHS->getType();
			auto RHSTy = this->RHS->getType();
			type_      = LHSTy + RHSTy;
		}

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;
		std::vector<TypeID> ArgTypes; // defined in prototype

	public:
		CallExprAST(TypeID ret_type_, const std::string& Callee, std::vector<TypeID> ArgTypes, std::vector<std::unique_ptr<ExprAST>> Args)
		    : ExprAST(ret_type_), Callee(Callee), ArgTypes(std::move(ArgTypes)), Args(std::move(Args))
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
		VarExprAST(TypeID VarType, std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames)
		    : ExprAST(std::move(VarType)), VarNames(std::move(VarNames)){};

		Value* codegen(Context& m_context, LocalContext& m_local_context) override;
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes), as well as if it is an operator.
	class PrototypeAST : public ExprAST
	{
		const std::string Name;
		const std::vector<std::string> Args;
		const std::vector<TypeID> ArgTypes;
		const TypeID ret_type_;
		const bool IsOperator;
		const unsigned Precedence; // Precedence if a binary op.

	public:
		PrototypeAST(TypeID ret_type_, const std::string& Name, std::vector<TypeID> ArgTypes, std::vector<std::string> Args, bool IsOperator = false, unsigned Prec = 0)
		    : ExprAST(functionTyID), ret_type_(ret_type_), ArgTypes(std::move(ArgTypes)), Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec)
		{
		}

		// Cast to Function*
		Value* codegen(Context& m_context, LocalContext& m_local_context) override;

		TypeID getRetType() const override;

		std::vector<TypeID> getArgTypes() const;

		bool isMain() const;

		bool match(string name /*, std::vector<TypeID> ArgTypes*/) const;

		const std::string& getName() const;

		bool isUnaryOp() const;

		bool isBinaryOp() const;

		char getOperatorName() const;

		unsigned getBinaryPrecedence() const;
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST
	{
		PrototypeAST& Proto;
		ExprList Body;

	public:
		FunctionAST(PrototypeAST& Proto, ExprList Body)
		    : ExprAST(functionTyID), Proto(Proto), Body(std::move(Body))
		{
		}

		// Cast to Function*
		Value* codegen(Context& m_context, LocalContext& m_local_context) override;

		TypeID getRetType() const override
		{
			return Proto.getRetType();
		}

		const std::string& getName() const;

		bool isMain() const;
	};

	Value* LogErrorV(const char* Str);

	Function* getFunction(std::string Name, Context& m_context, LocalContext& m_local_context);
	/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	/// the function.  This is used for mutable variables etc.
	static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const StringRef VarName, TypeID VarType, Context& m_context, LocalContext& m_local_context);
}; // namespace slljit
