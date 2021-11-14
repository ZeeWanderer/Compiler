#include "pch.h"
#include "Types.h"

#include "Context.h"

namespace slljit
{

	llvm::Type* get_llvm_type(TypeID TyID, LocalContext& m_local_context)
	{
		switch (TyID)
		{
		case slljit::doubleTyID:
			return m_local_context.LLVM_Builder->getDoubleTy();
		case slljit::int64TyID:
			return m_local_context.LLVM_Builder->getInt64Ty();
		case slljit::uint64TyID:
			return m_local_context.LLVM_Builder->getInt64Ty();
		case slljit::boolTyID:
			return m_local_context.LLVM_Builder->getInt1Ty();
		case slljit::functionTyID:
			break;
		case slljit::none:
			break;
		default:
			break;
		}

		return nullptr;
	}

	TypeID operator+(TypeID lhs, TypeID rhs)
	{
		if (lhs == rhs)
			return lhs;
		else
		{
			auto lookup = implict_cast_lookup.at({lhs, rhs});
			return lookup;
		}
	}
	bool isFloatingPointTy(TypeID type_)
	{
		switch (type_)
		{
		case slljit::doubleTyID:
			return true;
		default:
			return false;
		}
	}
	bool isIntegerTy(TypeID type_)
	{
		switch (type_)
		{
		case slljit::int64TyID:
		case slljit::uint64TyID:
		case slljit::boolTyID:
			return true;
		default:
			return false;
		}
	}

	bool isSigned(TypeID type_)
	{
		switch (type_)
		{
		case slljit::int64TyID:
			return true;
		default:
			return false;
		}
	}
}; // namespace slljit