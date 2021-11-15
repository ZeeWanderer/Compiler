#include "pch.h"
#include "Context.h"

namespace slljit
{
	using namespace llvm;
	using namespace orc;
	using namespace slljit;
	using namespace std;

	void init__()
	{
		static bool b_once                             = true;
		static constexpr const char* llvm_cl_options[] = {"./slljit", "-extra-vectorizer-passes", "-openmp-opt-disable", "-enable-unroll-and-jam", "-enable-simple-loop-unswitch"};
		static constexpr auto llvm_cl_options_c        = sizeof(llvm_cl_options) / sizeof(*llvm_cl_options);

		if (b_once)
		{
			const auto test = cl::ParseCommandLineOptions(llvm_cl_options_c, llvm_cl_options);
			assert(test);

			InitializeNativeTarget();
			InitializeNativeTargetAsmPrinter();
			InitializeNativeTargetAsmParser();

			b_once = false;
		}
	}
	Context::Context()
	{
		init__();
		shllJIT = ExitOnError()(ShaderJIT::Create());
		//	auto& TM = shllJIT->getTargetMachine();
		//	TM.setOptLevel(CodeGenOpt::Level::Aggressive);
	}
	LocalContext::LocalContext(Context& m_context)
	    : JD(m_context.shllJIT->create_new_JITDylib())
	{
		BinopPrecedence = {{'=', 2}, {'<', 10}, {'>', 10}, {'+', 20}, {'-', 20}, {'*', 40}, {'/', 40}};

		LLVM_Context = make_unique<LLVMContext>();
		LLVM_Builder = make_unique<IRBuilder<>>(*LLVM_Context);

		// Open a new module.
		LLVM_Module = std::make_unique<Module>("shader_module", *LLVM_Context);
		LLVM_Module->setDataLayout(m_context.shllJIT->getDataLayout());

		// Create a new pass manager attached to it.
		LLVM_FPM = std::make_unique<legacy::FunctionPassManager>(LLVM_Module.get());
		LLVM_PM  = std::make_unique<legacy::PassManager>();

		auto builder         = PassManagerBuilder();
		builder.OptLevel     = CodeGenOpt::Level::Aggressive;
		builder.SLPVectorize = true;
		builder.NewGVN       = true;

		builder.populateFunctionPassManager(*LLVM_FPM);
		builder.populateModulePassManager(*LLVM_PM);

		//	LLVM_FPM->add(createCFGSimplificationPass());       //	Dead code elimination
		//	LLVM_FPM->add(createPromoteMemoryToRegisterPass()); //	SSA conversion
		//	LLVM_FPM->add(createSROAPass());
		//	LLVM_FPM->add(createLoopSimplifyCFGPass());
		//	LLVM_FPM->add(createLoadStoreVectorizerPass());
		//	LLVM_FPM->add(createLoopVectorizePass());
		//	LLVM_FPM->add(createLoopUnrollPass());
		//	LLVM_FPM->add(createGVNPass());         //	Eliminate Common SubExpressions.
		//	LLVM_FPM->add(createNewGVNPass());      //	Global value numbering
		//	LLVM_FPM->add(createReassociatePass()); //	Reassociate expressions.
		//	LLVM_FPM->add(createConstantPropagationPass());
		//	LLVM_FPM->add(createPartiallyInlineLibCallsPass()); //	standard calls
		//	LLVM_FPM->add(createDeadCodeEliminationPass());
		//	LLVM_FPM->add(createAggressiveDCEPass());
		//	LLVM_FPM->add(createCFGSimplificationPass());    //	Cleanup
		//	LLVM_FPM->add(createInstructionCombiningPass()); //	Do simple "peephole" optimizations and bit-twiddling optzns.
		//////	LLVM_FPM->add(createAggressiveInstCombinerPass());
		//	LLVM_FPM->add(createSLPVectorizerPass());
		//	LLVM_FPM->add(createFlattenCFGPass()); //	Flatten the control flow graph.

		//	LLVM_FPM->add(createCFGSimplificationPass());
		//	LLVM_FPM->add(createPromoteMemoryToRegisterPass());
		//	LLVM_FPM->add(createEarlyCSEPass());
		//	LLVM_FPM->add(createTailCallEliminationPass());
		//	LLVM_FPM->add(createInstructionCombiningPass());
		//	LLVM_FPM->add(createBasicAAWrapperPass());
		////LLVM_FPM->add(new MemoryDependenceAnalysis());
		//	LLVM_FPM->add(createLICMPass());
		//	LLVM_FPM->add(createLoopInstSimplifyPass());
		//	LLVM_FPM->add(createNewGVNPass());
		//	LLVM_FPM->add(createDeadStoreEliminationPass());
		//	LLVM_FPM->add(createSCCPPass());
		//	LLVM_FPM->add(createReassociatePass());
		//	LLVM_FPM->add(createInstructionCombiningPass());
		////LLVM_FPM->add(createInstructionSimplifierPass());
		//	LLVM_FPM->add(createAggressiveDCEPass());
		//	LLVM_FPM->add(createCFGSimplificationPass());
		//	LLVM_FPM->add(createLintPass()); // Check

		//	Remove unused functions, structs, global variables, etc
		//	LLVM_PM->add(createStripDeadPrototypesPass());
		//	LLVM_PM->add(createFunctionInliningPass());
		//	LLVM_PM->add(createDeadInstEliminationPass());
	}
	//	void LocalContext::set_key(VModuleKey module_key)
	//{
	//	this->module_key = module_key;
	//}
	//	auto LocalContext::get_key()
	//{
	//	return this->module_key;
	//}
}; // namespace slljit
