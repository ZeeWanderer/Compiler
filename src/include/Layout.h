#pragma once
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>

#include "Types.h"
namespace slljit
{
	using namespace std;

	template <typename T>
	concept BasicType = std::is_integral<T>::value || std::is_floating_point<T>::value;

	enum LayoutVarTypes
	{
		Kdouble = doubleTyID,
		Kint64  = int64TyID,
		Kuint64 = uint64TyID
	};

	struct GlobalDefinition
	{
		string name;
		LayoutVarTypes type;
		size_t offset;
	};

	struct ConstantGlobalDefinition
	{
		union
		{
			double valueD;
			int64_t valueSI64;
			uint64_t valueUI64;
		};

		LayoutVarTypes type;

		template <BasicType T>
		ConstantGlobalDefinition(T value, LayoutVarTypes type);

		Constant* get_init_val(LocalContext& ctx) const;
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
		void addConsatant(string name, int64_t value, LayoutVarTypes type);
	};

}; // namespace slljit
