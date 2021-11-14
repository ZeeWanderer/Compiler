#include "pch.h"
#include "Instructions.h"
#include "Types.h"
#include "Context.h"

namespace slljit
{
	void CreateExplictCast(Value*& val, TypeID fromTy, TypeID toTy, LocalContext& ctx)
	{
		if (fromTy != toTy)
		{
			const auto llvm_type = get_llvm_type(toTy, ctx);
			if (isIntegerTy(fromTy) && isIntegerTy(toTy))
			{
				val = ctx.LLVM_Builder->CreateIntCast(val, llvm_type, isSigned(toTy));
			}
			else if (isFloatingPointTy(fromTy) && isFloatingPointTy(toTy))
			{
				val = ctx.LLVM_Builder->CreateFPCast(val, llvm_type);
			}
			else
			{
				auto castOP = fp_int_cast_op_lookup.at({fromTy, toTy});
				val         = ctx.LLVM_Builder->CreateCast(castOP, val, llvm_type);
			}
		}
	}

	TypeID CreateImplictCast(Value*& lhs, Value*& rhs, TypeID lhsTy, TypeID rhsTy, LocalContext& ctx)
	{
		if (lhsTy != rhsTy)
		{
			auto implictTy = implict_cast_lookup.at({lhsTy, rhsTy});
			CreateExplictCast(lhs, lhsTy, implictTy, ctx);
			CreateExplictCast(rhs, rhsTy, implictTy, ctx);

			return implictTy;
		}
		return lhsTy;
	}

	Value* CtreateCMP(CmpPredicate pred, Value* lhs, Value* rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name)
	{
		const auto llvm_pred = pred_to_llvm_pred.at(type_).at(pred);
		return ctx.LLVM_Builder->CreateCmp(llvm_pred, lhs, rhs, Name);
	}

	Value* CreateAdd(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name)
	{
		if (isFloatingPointTy(type_))
		{
			return ctx.LLVM_Builder->CreateFAdd(lhs, rhs, Name);
		}
		else if (isIntegerTy(type_))
		{
			return ctx.LLVM_Builder->CreateAdd(lhs, rhs, Name);
		}
		else
		{
			return nullptr;
		}
	}

	Value* CreateSub(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name)
	{
		if (isFloatingPointTy(type_))
		{
			return ctx.LLVM_Builder->CreateFSub(lhs, rhs, Name);
		}
		else if (isIntegerTy(type_))
		{
			return ctx.LLVM_Builder->CreateSub(lhs, rhs, Name);
		}
		else
		{
			return nullptr;
		}
	}

	Value* CreateMul(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name)
	{
		if (isFloatingPointTy(type_))
		{
			return ctx.LLVM_Builder->CreateFMul(lhs, rhs, Name);
		}
		else if (isIntegerTy(type_))
		{
			return ctx.LLVM_Builder->CreateMul(lhs, rhs, Name);
		}
		else
		{
			return nullptr;
		}
	}

	Value* CreateDiv(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name)
	{
		if (isFloatingPointTy(type_))
		{
			return ctx.LLVM_Builder->CreateFDiv(lhs, rhs, Name);
		}
		else if (isIntegerTy(type_))
		{
			if (isSigned(type_))
			{
				return ctx.LLVM_Builder->CreateSDiv(lhs, rhs, Name);
			}
			else
			{
				return ctx.LLVM_Builder->CreateUDiv(lhs, rhs, Name);
			}
		}
		else
		{
			return nullptr;
		}
	}
} // namespace slljit