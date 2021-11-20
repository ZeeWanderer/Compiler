#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace slljit
{
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	class Layout;
	class CodeGen;

	/// Used to pass parameters to code generation stage
	/**
	 * Holds data about various compile parameters like optimization level.
	 */
	struct CompileOptions
	{
		friend class CodeGen;

	public:
		//! OptLevel enum.
		/*! Defines compile optimization */
		enum OptLevel
		{
			O0, /*!< No ptimization */
			O1, /*!< msvc O1 equivalent */
			O2, /*!< msvc O2 equivalent */
			O3, /*!< msvc O3 equivalent */
			Os, /*!< O2 with attempts to optimyze for code size */
			Oz  /*!< Special mode. Optimize for code size at all costs */
		};

		/**
		 * Optimization level. 
		 * Default level is O3.
		 */
		OptLevel opt_level = O3;

	private:
		PassBuilder::OptimizationLevel to_llvm_opt_level();
	};
}; // namespace slljit
