#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	class Layout;
	class CodeGen;

	struct CompileOptions
	{
		friend class CodeGen;

	public:
		enum OptLevel
		{
			O0,
			O1,
			O2,
			O3,
			Os,
			Oz
		};
		OptLevel opt_level = O3;

	private:
		PassBuilder::OptimizationLevel to_llvm_opt_level();
	};
}; // namespace slljit
