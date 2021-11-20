#include "pch.h"
#include "Options.h"

#include "llvm/Passes/PassBuilder.h"

namespace slljit
{
	using namespace slljit;
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	PassBuilder::OptimizationLevel CompileOptions::to_llvm_opt_level()
	{
		switch (opt_level)
		{
		case slljit::CompileOptions::O0:
			return PassBuilder::OptimizationLevel::O0;
		case slljit::CompileOptions::O1:
			return PassBuilder::OptimizationLevel::O1;
		case slljit::CompileOptions::O2:
			return PassBuilder::OptimizationLevel::O2;
		case slljit::CompileOptions::O3:
			return PassBuilder::OptimizationLevel::O3;
		case slljit::CompileOptions::Os:
			return PassBuilder::OptimizationLevel::Os;
		case slljit::CompileOptions::Oz:
			return PassBuilder::OptimizationLevel::Oz;
		}
	}

}; // namespace slljit
