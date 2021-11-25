#include "pch.h"
#include "Context.h"
#include "Layout.h"

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
		shllJIT = cantFail(ShaderJIT::Create());
	}

	LocalContext::LocalContext(Context& m_context, Layout layout)
	    : JD(m_context.shllJIT->create_new_JITDylib()), layout(layout)
	{
		BinopPrecedence = {{'=', 2}, {'<', 10}, {'>', 10}, {'+', 20}, {'-', 20}, {'*', 40}, {'/', 40}};

		LLVM_Context = make_unique<LLVMContext>();
		LLVM_Builder = make_unique<IRBuilder<>>(*LLVM_Context);

		// Open a new module.
		LLVM_Module = std::make_unique<Module>("shader_module", *LLVM_Context);
		LLVM_Module->setDataLayout(m_context.shllJIT->getDataLayout());
	}
}; // namespace slljit
