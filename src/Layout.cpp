#include "pch.h"
#include "Layout.h"

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
}; // namespace slljit
