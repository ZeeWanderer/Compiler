#pragma once

#ifndef LLVM_EXECUTIONENGINE_ORC_SHADERJIT_H
#define LLVM_EXECUTIONENGINE_ORC_SHADERJIT_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/EPCIndirectionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <fstream>

namespace slljit
{
	using namespace llvm;
	using namespace orc;
	using namespace object;
	using namespace std;

	class DummyCache : public ObjectCache
	{
	public:
		virtual void notifyObjectCompiled(const Module* M, MemoryBufferRef Obj) override
		{
			ofstream ibject_f("./tmp.obj", ios::out | ios::trunc | ios::binary);
			auto test = ibject_f.is_open();
			ibject_f.write(Obj.getBufferStart(), Obj.getBufferSize());
		}

		virtual std::unique_ptr<MemoryBuffer> getObject(const Module* M) override
		{
			return nullptr;
		}
	};

	class ShaderJIT
	{
	private:
		std::unique_ptr<ExecutionSession> ES;

		llvm::DataLayout DL;
		MangleAndInterner Mangle;

		RTDyldObjectLinkingLayer ObjectLayer;
		IRCompileLayer CompileLayer;

		JITDylib& MainJD;

#ifdef _DEBUG
		DummyCache m_dummy_cache;
#endif // DEBUG

		static void handleLazyCallThroughError()
		{
			errs() << "LazyCallThrough error: Could not find function body";
			exit(1);
		}

	public:
		ShaderJIT(std::unique_ptr<ExecutionSession> ES, JITTargetMachineBuilder JTMB, DataLayout DL)
		    : ES(std::move(ES)),
		      DL(std::move(DL)),
		      Mangle(*this->ES, this->DL),
		      ObjectLayer(*this->ES, []()
		          { return std::make_unique<SectionMemoryManager>(); })
#ifdef _DEBUG
		      ,
		      CompileLayer(*this->ES, ObjectLayer, std::make_unique<ConcurrentIRCompiler>(std::move(JTMB), &m_dummy_cache))
#else  // RELEASE
		      ,
		      CompileLayer(*this->ES, ObjectLayer, std::make_unique<ConcurrentIRCompiler>(std::move(JTMB)))
#endif // END
		      ,
		      MainJD(this->ES->createBareJITDylib("main_JITDylib"))
		{
			MainJD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));

			if (JTMB.getTargetTriple().isOSBinFormatCOFF())
			{
				ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
				ObjectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
			}
		}

		~ShaderJIT()
		{
			if (auto Err = ES->endSession())
				ES->reportError(std::move(Err));
		}

		static Expected<std::unique_ptr<ShaderJIT>> Create()
		{
			auto EPC = SelfExecutorProcessControl::Create();
			if (!EPC)
				return EPC.takeError();

			auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));

			JITTargetMachineBuilder JTMB(
			    ES->getExecutorProcessControl().getTargetTriple());

			JTMB.setCodeGenOptLevel(CodeGenOpt::Aggressive);
			JTMB.setCPU(sys::getHostCPUName().str());

			SubtargetFeatures Features;
			auto HostFeatures = llvm::StringMap<bool>();
			sys::getHostCPUFeatures(HostFeatures);

			for (auto& F : HostFeatures)
				Features.AddFeature(F.first(), F.second);

			JTMB.addFeatures(Features.getFeatures());

			auto DL = JTMB.getDefaultDataLayoutForTarget();
			if (!DL)
				return DL.takeError();

			return std::make_unique<ShaderJIT>(std::move(ES), std::move(JTMB),
			    std::move(*DL));
		}

		const DataLayout& getDataLayout() const
		{
			return DL;
		}

		JITDylib& getMainJITDylib()
		{
			return MainJD;
		}

		Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr)
		{
			if (!RT)
				RT = MainJD.getDefaultResourceTracker();
			return CompileLayer.add(RT, std::move(TSM));
		}

		Expected<JITEvaluatedSymbol> lookup(StringRef Name)
		{
			return ES->lookup({&MainJD}, Mangle(Name.str()));
		}
	};
}; // namespace slljit

#endif // LLVM_EXECUTIONENGINE_ORC_SHADERJIT_H
