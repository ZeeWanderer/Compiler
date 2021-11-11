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
		case slljit::functionTyID:
			break;
		//case slljit::control:
		//	break;
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
			auto lookup = implict_cast_loockup.at({lhs, rhs});
			return lookup;
		}
	}
}; // namespace slljit