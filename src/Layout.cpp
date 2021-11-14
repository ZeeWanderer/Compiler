#include "pch.h"
#include "Layout.h"
#include "Context.h"

using namespace slljit;
using namespace llvm;
using namespace llvm::orc;
using namespace std;

namespace slljit
{
	void Layout::addMember(string name, LayoutVarTypes type, size_t offset)
	{
		globals.emplace_back(GlobalDefinition{name, type, offset});
		names.insert(name);
		global_offsets.emplace_back(offset);
	}

	void Layout::addConsatant(string name, double value, LayoutVarTypes type)
	{
		constant_globals.emplace(pair{name, ConstantGlobalDefinition{value, type}});
		names.insert(name);
	}

	void Layout::addConsatant(string name, int64_t value, LayoutVarTypes type)
	{
		constant_globals.emplace(pair{name, ConstantGlobalDefinition{value, type}});
		names.insert(name);
	}

	template <BasicType T>
	inline ConstantGlobalDefinition::ConstantGlobalDefinition(T value, LayoutVarTypes type)
	{
		switch (type)
		{
		case slljit::Kdouble:
			this->valueD = (double)value;
			break;
		case slljit::Kint64:
			this->valueSI64 = (int64_t)value;
			break;
		case slljit::Kuint64:
			this->valueUI64 = (uint64_t)value;
			break;
		default: break;
		}

		this->type = type;
	}

	Constant* ConstantGlobalDefinition::get_init_val(LocalContext& ctx)
	{
		switch (type)
		{
		case slljit::Kdouble:
			return ConstantFP::get(*ctx.LLVM_Context, APFloat(valueD));
		case slljit::Kint64:
			return ConstantInt::get(*ctx.LLVM_Context, APInt(64, valueSI64, true));
		case slljit::Kuint64:
			return ConstantInt::get(*ctx.LLVM_Context, APInt(64, valueUI64, false));
		default: break;
		}
	}

}; // namespace slljit
