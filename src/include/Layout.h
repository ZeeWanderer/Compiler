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
		Kint64 = int64TyID
	};

	struct GlobalDefinition
	{
		string name;
		LayoutVarTypes type;
		size_t offset;
	};

	struct ConstantGlobalDefinition
	{
		template <BasicType T>
		ConstantGlobalDefinition(T value, LayoutVarTypes type)
		{
			switch (type)
			{
			case slljit::Kdouble:
				this->valueD = (double)value;
				break;
			case slljit::Kint64:
				this->valueI64 = (int64_t)value;
				break;
			default: break;
			}

			this->type = type;
		}

		union
		{
			double valueD;
			int64_t valueI64;
		};
		
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
		void addConsatant(string name, int64_t value, LayoutVarTypes type);
	};
}; // namespace slljit
