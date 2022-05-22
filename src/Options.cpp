#include "pch.h"
#include "Options.h"

#include "llvm/Passes/PassBuilder.h"

namespace slljit
{
	using namespace slljit;
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	llvm::OptimizationLevel CompileOptions::to_llvm_opt_level()
	{
		switch (opt_level)
		{
		case slljit::CompileOptions::O0:
			return llvm::OptimizationLevel::O0;
		case slljit::CompileOptions::O1:
			return llvm::OptimizationLevel::O1;
		case slljit::CompileOptions::O2:
			return llvm::OptimizationLevel::O2;
		case slljit::CompileOptions::O3:
			return llvm::OptimizationLevel::O3;
		case slljit::CompileOptions::Os:
			return llvm::OptimizationLevel::Os;
		case slljit::CompileOptions::Oz:
			return llvm::OptimizationLevel::Oz;
		}
	}

}; // namespace slljit
