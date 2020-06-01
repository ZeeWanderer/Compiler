#include "pch.h"
#include "Context.h"

using namespace llvm;
using namespace llvm::orc;
using namespace slljit;
using namespace std;

namespace slljit
{
	void init__()
	{
		static bool b_once = true;
		if (b_once)
		{
			InitializeNativeTarget();
			InitializeNativeTargetAsmPrinter();
			InitializeNativeTargetAsmParser();
			b_once = false;
		}
	}
	Context::Context()
	    : LLVM_Builder(LLVM_Context)
	{
		init__();
		shllJIT = std::make_unique<ShaderJIT>();
	}
	LocalContext::LocalContext(Context& m_context)
	{
		BinopPrecedence = {{'=', 2}, {'<', 10}, {'>', 10}, {'+', 20}, {'-', 20}, {'*', 40}, {'/', 40}};

		// Open a new module.
		LLVM_Module = std::make_unique<Module>("my cool jit", m_context.LLVM_Context);
		LLVM_Module->setDataLayout(m_context.shllJIT->getTargetMachine().createDataLayout());

		// Create a new pass manager attached to it.
		LLVM_FPM = std::make_unique<legacy::FunctionPassManager>(LLVM_Module.get());
		LLVM_PM = std::make_unique<legacy::PassManager>();

		LLVM_FPM->add(createCFGSimplificationPass());       //	Dead code elimination
		LLVM_FPM->add(createPromoteMemoryToRegisterPass()); //	SSA conversion
		LLVM_FPM->add(createSROAPass());
		LLVM_FPM->add(createLoopSimplifyCFGPass());
		LLVM_FPM->add(createLoadStoreVectorizerPass());
		LLVM_FPM->add(createLoopVectorizePass());
		LLVM_FPM->add(createLoopUnrollPass());
		LLVM_FPM->add(createGVNPass());         //	Eliminate Common SubExpressions.
		LLVM_FPM->add(createNewGVNPass());      //	Global value numbering
		LLVM_FPM->add(createReassociatePass()); //	Reassociate expressions.
		LLVM_FPM->add(createConstantPropagationPass());
		LLVM_FPM->add(createPartiallyInlineLibCallsPass()); //	standard calls
		LLVM_FPM->add(createDeadCodeEliminationPass());
		LLVM_FPM->add(createAggressiveDCEPass());
		LLVM_FPM->add(createCFGSimplificationPass());    //	Cleanup
		LLVM_FPM->add(createInstructionCombiningPass()); //	Do simple "peephole" optimizations and bit-twiddling optzns.
		////	LLVM_FPM->add(createAggressiveInstCombinerPass());
		LLVM_FPM->add(createSLPVectorizerPass());
		LLVM_FPM->add(createFlattenCFGPass()); //	Flatten the control flow graph.

		//LLVM_FPM->add(createCFGSimplificationPass());
		//LLVM_FPM->add(createPromoteMemoryToRegisterPass());
		//LLVM_FPM->add(createEarlyCSEPass());
		//LLVM_FPM->add(createTailCallEliminationPass());
		//LLVM_FPM->add(createInstructionCombiningPass());
		//LLVM_FPM->add(createBasicAAWrapperPass());
		////LLVM_FPM->add(new MemoryDependenceAnalysis());
		//LLVM_FPM->add(createLICMPass());
		//LLVM_FPM->add(createLoopInstSimplifyPass());
		//LLVM_FPM->add(createNewGVNPass());
		//LLVM_FPM->add(createDeadStoreEliminationPass());
		//LLVM_FPM->add(createSCCPPass());
		//LLVM_FPM->add(createReassociatePass());
		//LLVM_FPM->add(createInstructionCombiningPass());
		////LLVM_FPM->add(createInstructionSimplifierPass());
		//LLVM_FPM->add(createAggressiveDCEPass());
		//LLVM_FPM->add(createCFGSimplificationPass());
		//LLVM_FPM->add(createLintPass()); // Check

		//Remove unused functions, structs, global variables, etc
		//LLVM_PM->add(createStripDeadPrototypesPass());
		LLVM_PM->add(createFunctionInliningPass());
		LLVM_PM->add(createDeadInstEliminationPass());
	}
	void LocalContext::set_key(VModuleKey module_key)
	{
		this->module_key = module_key;
	}
	auto LocalContext::get_key()
	{
		return this->module_key;
	}
}; // namespace slljit