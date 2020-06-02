#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>

namespace slljit
{
	using namespace std;

	enum LayoutVarTypes
	{
		Kdouble = 0
	};

	struct GlobalDefinition
	{
		string name;
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
		vector<GlobalDefinition> globals;
		vector<size_t> global_offsets;
		map<string, ConstantGlobalDefinition> constant_globals;

		void addMember(string name, LayoutVarTypes type, size_t offset);

		void addConsatant(string name, double value, LayoutVarTypes type);
	};
}; // namespace slljit
