#include "pch.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "CodeGen.h"
#include "Layout.h"
#include "Types.h"
#include "Options.h"

namespace slljit
{
	using namespace slljit;
	using namespace llvm;
	using namespace llvm::orc;
	using namespace std;

	void CodeGen::compile_layout(Context& m_context, LocalContext& m_local_context)
	{
		auto& context      = m_local_context.getContext();
		auto& builder      = m_local_context.getBuilder();
		auto& module       = m_local_context.getModule();
		const auto& layout = m_local_context.getLayout();

		// Insert variable globals
		// Globals contain pointers to layout members, init with nullptr
		for (auto& global : layout.globals)
		{
			Type* type_ = get_llvm_type(TypeID(global.type), m_local_context);
			module.getOrInsertGlobal(global.name, type_->getPointerTo());
			GlobalVariable* gVar = module.getNamedGlobal(global.name);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantPointerNull::get(type_->getPointerTo()));
		}

		// Insert constant globals
		for (auto& global : layout.constant_globals)
		{
			Type* type_      = get_llvm_type(TypeID(global.second.type), m_local_context);
			Constant* init_c = global.second.get_init_val(m_local_context);

			module.getOrInsertGlobal(global.first, type_);
			GlobalVariable* gVar = module.getNamedGlobal(global.first);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(init_c);
			gVar->setConstant(true);
		}
	}

	Error CodeGen::compile(std::list<std::unique_ptr<PrototypeAST>> prototypes, std::list<std::unique_ptr<FunctionAST>> functions, Context& m_context, LocalContext& m_local_context, CompileOptions& options, bool bDumpIR)
	{
		for (auto it = prototypes.begin(); it != prototypes.end(); ++it)
		{
			if (auto FnIR = (*it)->codegen(m_context, m_local_context))
			{
				m_local_context.FunctionProtos[(*it)->getName()] = std::move((*it));
			}
			else
				return FnIR.takeError();
		}

		for (auto it = functions.begin(); it != functions.end(); ++it)
		{
			auto FnIR = (*it)->codegen(m_context, m_local_context);
			if (!FnIR)
			{
				return FnIR.takeError();
			}
		}

		if (bDumpIR)
		{
			fprintf(stderr, "----IR_START----\n");
			fprintf(stderr, "; PreOptimization:\n");
			m_local_context.getModule().dump();
		}

		if (options.opt_level != CompileOptions::O0)
		{
			const auto llvm_opt_level = options.to_llvm_opt_level();
			PassBuilder PB;

			auto LLVM_LAM  = LoopAnalysisManager();
			auto LLVM_FAM  = FunctionAnalysisManager();
			auto LLVM_CGAM = CGSCCAnalysisManager();
			auto LLVM_MAM  = ModuleAnalysisManager();

			PB.registerModuleAnalyses(LLVM_MAM);
			PB.registerCGSCCAnalyses(LLVM_CGAM);
			PB.registerFunctionAnalyses(LLVM_FAM);
			PB.registerLoopAnalyses(LLVM_LAM);
			PB.crossRegisterProxies(LLVM_LAM, LLVM_FAM, LLVM_CGAM, LLVM_MAM);

			auto LLVM_FPM = FunctionPassManager(PB.buildFunctionSimplificationPipeline(llvm_opt_level, ThinOrFullLTOPhase::None));
			auto LLVM_MPM = ModulePassManager(PB.buildPerModuleDefaultPipeline(llvm_opt_level, true));

			LLVM_MPM.run(m_local_context.getModule(), LLVM_MAM);
		}

		if (bDumpIR)
		{
			fprintf(stderr, "\n; PostOptimization:\n");
			m_local_context.getModule().dump();
		}

		// ASSEMBLY
		//std::string outStr;
		//{
		//	/*	auto& TM = m_context.shllJIT->;
		//	 llvm::legacy::PassManager pm;

		//	 llvm::raw_string_ostream stream(outStr);

		//	 llvm::buffer_ostream pstream(stream);

		//	 TM.addPassesToEmitFile(pm, pstream, nullptr,

		//	     llvm::CodeGenFileType::CGFT_AssemblyFile);

		//	 pm.run(*m_local_context.LLVM_Module.get());*/
		//}

		////	fprintf(stderr, "; Assembly:\n%s", outStr.c_str());

		auto RT = m_local_context.getJITDylib().getDefaultResourceTracker();

		auto TSM = ThreadSafeModule(std::move(m_local_context.takeModule()), std::move(m_local_context.takeContext()));

		Error err = m_context.shllJIT->addModule(std::move(TSM), RT);
		if (err)
		{
			return err;
		}

		if (bDumpIR)
		{
			fprintf(stderr, "\n; JITDylib:\n");
			m_local_context.getJITDylib().dump(dbgs());
			fprintf(stderr, "\n-----IR_END-----\n");
		}

		return Error::success();
	}
}; // namespace slljit
