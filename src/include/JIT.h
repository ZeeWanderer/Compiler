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
#include "AST.h"
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

	//	ThreadSafeModule irgenAndTakeOwnership(FunctionAST& FnAST, const std::string& Suffix)
	//{
	//	if (auto* F = FnAST.codegen())
	//	{
	//		F->setName(F->getName() + Suffix);
	//		auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
	//		// Start a new module.
	//		InitializeModule();
	//		return TSM;
	//	}
	//	else
	//		report_fatal_error("Couldn't compile lazily JIT'd function");
	//}

	//	class KaleidoscopeASTMaterializationUnit : public MaterializationUnit
	//{
	//	public:
	//	KaleidoscopeASTMaterializationUnit(KaleidoscopeASTLayer& L, std::unique_ptr<FunctionAST> F);

	//	StringRef getName() const override
	//	{
	//		return "KaleidoscopeASTMaterializationUnit";
	//	}

	//	void materialize(std::unique_ptr<MaterializationResponsibility> R) override;

	//	private:
	//	void discard(const JITDylib& JD, const SymbolStringPtr& Sym) override
	//	{
	//		llvm_unreachable("Kaleidoscope functions are not overridable");
	//	}

	//	KaleidoscopeASTLayer& L;
	//	std::unique_ptr<FunctionAST> F;
	//};

	//	class KaleidoscopeASTLayer
	//{
	//	public:
	//	KaleidoscopeASTLayer(IRLayer& BaseLayer, const DataLayout& DL)
	//	    : BaseLayer(BaseLayer)
	//	    , DL(DL)
	//	{
	//	}

	//	Error add(ResourceTrackerSP RT, std::unique_ptr<FunctionAST> F)
	//	{
	//		return RT->getJITDylib().define(std::make_unique<KaleidoscopeASTMaterializationUnit>(*this, std::move(F)), RT);
	//	}

	//	void emit(std::unique_ptr<MaterializationResponsibility> MR, std::unique_ptr<FunctionAST> F)
	//	{
	//		BaseLayer.emit(std::move(MR), llvm::irgenAndTakeOwnership(*F, ""));
	//	}

	//	SymbolFlagsMap getInterface(FunctionAST& F)
	//	{
	//		MangleAndInterner Mangle(BaseLayer.getExecutionSession(), DL);
	//		SymbolFlagsMap Symbols;
	//		Symbols[Mangle(F.getName())] = JITSymbolFlags(JITSymbolFlags::Exported | JITSymbolFlags::Callable);
	//		return Symbols;
	//	}

	//	private:
	//	IRLayer& BaseLayer;
	//	const DataLayout& DL;
	//};

	//	KaleidoscopeASTMaterializationUnit::KaleidoscopeASTMaterializationUnit(KaleidoscopeASTLayer& L, std::unique_ptr<FunctionAST> F)
	//    : MaterializationUnit(L.getInterface(*F), nullptr)
	//    , L(L)
	//    , F(std::move(F))
	//{
	//}

	//	void KaleidoscopeASTMaterializationUnit::materialize(std::unique_ptr<MaterializationResponsibility> R)
	//{
	//	L.emit(std::move(R), std::move(F));
	//}

	class ShaderJIT
	{
	private:
		std::unique_ptr<ExecutionSession> ES;
		/*std::unique_ptr<EPCIndirectionUtils> EPCIU;*/

		llvm::DataLayout DL;
		MangleAndInterner Mangle;

		RTDyldObjectLinkingLayer ObjectLayer;
		IRCompileLayer CompileLayer;
		//	IRTransformLayer OptimizeLayer;
		//	KaleidoscopeASTLayer ASTLayer;

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
		ShaderJIT(std::unique_ptr<ExecutionSession> ES /*, std::unique_ptr<EPCIndirectionUtils> EPCIU*/, JITTargetMachineBuilder JTMB, DataLayout DL)
		    : ES(std::move(ES)),
		      /*, EPCIU(std::move(EPCIU)),*/
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
#endif // END                                                  \
    //, OptimizeLayer(*this->ES, CompileLayer, optimizeModule) \
    //, //ASTLayer(OptimizeLayer, this->DL)
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
			//	if (auto Err = EPCIU->cleanup())
			//	ES->reportError(std::move(Err));
		}

		static Expected<std::unique_ptr<ShaderJIT>> Create()
		{
			auto EPC = SelfExecutorProcessControl::Create();
			if (!EPC)
				return EPC.takeError();

			auto triple = (*EPC)->getTargetTriple();

			auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));

			//	auto EPCIU = EPCIndirectionUtils::Create(ES->getExecutorProcessControl());
			//	if (!EPCIU)
			//	return EPCIU.takeError();

			//(*EPCIU)->createLazyCallThroughManager(*ES, pointerToJITTargetAddress(&handleLazyCallThroughError));

			//	if (auto Err = setUpInProcessLCTMReentryViaEPCIU(**EPCIU))
			//	return std::move(Err);

			JITTargetMachineBuilder JTMB(triple);

			//	JTMB.setCodeGenOptLevel(CodeGenOpt::Aggressive);
			//	JTMB.setCPU(sys::getHostCPUName().str());

			//	SubtargetFeatures Features;
			//	auto HostFeatures = llvm::StringMap<bool>();
			//	sys::getHostCPUFeatures(HostFeatures);
			//
			//	for (auto& F : HostFeatures)
			//	Features.AddFeature(F.first(), F.second);

			//	JTMB.addFeatures(Features.getFeatures());

			auto DL = JTMB.getDefaultDataLayoutForTarget();
			if (!DL)
				return DL.takeError();

			return std::make_unique<ShaderJIT>(std::move(ES) /*, std::move(*EPCIU)*/, std::move(JTMB), std::move(*DL));
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
			//	return OptimizeLayer.add(RT, std::move(TSM));
		}

		//	Error addAST(std::unique_ptr<FunctionAST> F, ResourceTrackerSP RT = nullptr)
		//{
		//	if (!RT)
		//		RT = MainJD.getDefaultResourceTracker();
		//	return ASTLayer.add(RT, std::move(F));
		//}

		Expected<JITEvaluatedSymbol> lookup(StringRef Name)
		{
			auto test = Mangle(Name.str());
			return ES->lookup({&MainJD}, Mangle(Name.str()));
		}

	private:
		static Expected<ThreadSafeModule> optimizeModule(ThreadSafeModule TSM, const MaterializationResponsibility& R)
		{
			TSM.withModuleDo(
			    [](Module& M)
			    {
				    // Create a function pass manager.
				    //	auto FPM = std::make_unique<legacy::FunctionPassManager>(&M);

				    //// Add some optimizations.
				    //	FPM->add(createInstructionCombiningPass());
				    //	FPM->add(createReassociatePass());
				    //	FPM->add(createGVNPass());
				    //	FPM->add(createCFGSimplificationPass());
				    //	FPM->doInitialization();

				    //// Run the optimizations over all functions in the module being added to
				    //// the JIT.
				    //	for (auto& F : M)
				    // FPM->run(F);
			    });

			return std::move(TSM);
		}
	};
	//
	//
	//	class ShaderJIT_
	//	{
	//	public:
	//		using ObjLayerT     = RTDyldObjectLinkingLayer;
	//		using CompileLayerT = IRCompileLayer<ObjLayerT, SimpleCompiler>;
	//#ifdef _DEBUG
	//		DummyCache m_dummy_cache;
	//#endif // DEBUG
	//		ShaderJIT_()
	//		    : Resolver(createLookupResolver(
	//		          ES, [this](StringRef Name) { return findMangledSymbol(std::string(Name)); }, [](Error Err) { cantFail(std::move(Err), "lookupFlags failed"); }))
	//		    , TM(EngineBuilder()
	//		              .setMCPU(sys::getHostCPUName())
	//		              .setOptLevel(CodeGenOpt::Aggressive)
	//		              //.setMAttrs([]() {
	//			             // auto features = llvm::StringMap<bool>();
	//			             // sys::getHostCPUFeatures(features);
	//			             // return features;
	//		              //}())
	//		              .selectTarget())
	//		    , DL(TM->createDataLayout())
	//		    , ObjectLayer(AcknowledgeORCv1Deprecation, ES,
	//		          [this](VModuleKey) {
	//			          return ObjLayerT::Resources{std::make_shared<SectionMemoryManager>(), Resolver};
	//		          })
	//		    , CompileLayer(AcknowledgeORCv1Deprecation, ObjectLayer, SimpleCompiler(*TM))
	//		{
	//			//SubtargetFeatures Features;
	//			//auto HostFeatures = llvm::StringMap<bool>();
	//			//sys::getHostCPUFeatures(HostFeatures);
	//
	//			//for (auto& F : HostFeatures)
	//			//	Features.AddFeature(F.first(), F.second);
	//
	//			//auto f_str = Features.getString();
	//			//Features.getFeatures();
	//
	//			sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	//#ifdef _DEBUG
	//			CompileLayer.getCompiler().setObjectCache(&m_dummy_cache);
	//#endif // DEBUG
	//		}
	//
	//		TargetMachine& getTargetMachine()
	//		{
	//			return *TM;
	//		}
	//
	//		VModuleKey addModule(std::unique_ptr<Module> M)
	//		{
	//			auto K = ES.allocateVModule();
	//			cantFail(CompileLayer.addModule(K, std::move(M)));
	//			ModuleKeys.push_back(K);
	//			return K;
	//		}
	//
	//		void removeModule(VModuleKey K)
	//		{
	//			ModuleKeys.erase(find(ModuleKeys, K));
	//			cantFail(CompileLayer.removeModule(K));
	//		}
	//
	//		JITSymbol findSymbol(const std::string Name)
	//		{
	//			return findMangledSymbol(mangle(Name));
	//		}
	//
	//		JITSymbol findSymbol(const std::string Name, VModuleKey Module)
	//		{
	//			return findMangledSymbol(mangle(Name), Module);
	//		}
	//
	//	private:
	//		std::string mangle(const std::string& Name)
	//		{
	//			std::string MangledName;
	//			{
	//				raw_string_ostream MangledNameStream(MangledName);
	//				Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
	//			}
	//			return MangledName;
	//		}
	//
	//		JITSymbol findMangledSymbol(const std::string& Name, VModuleKey Module)
	//		{
	//#ifdef _WIN32
	//			// The symbol lookup of ObjectLinkingLayer uses the SymbolRef::SF_Exported
	//			// flag to decide whether a symbol will be visible or not, when we call
	//			// IRCompileLayer::findSymbolIn with ExportedSymbolsOnly set to true.
	//			//
	//			// But for Windows COFF objects, this flag is currently never set.
	//			// For a potential solution see: https://reviews.llvm.org/rL258665
	//			// For now, we allow non-exported symbols on Windows as a workaround.
	//			const bool ExportedSymbolsOnly = false;
	//#else
	//			const bool ExportedSymbolsOnly = true;
	//#endif
	//			// Search provided module
	//			if (auto Sym = CompileLayer.findSymbolIn(Module, Name, ExportedSymbolsOnly))
	//				return Sym;
	//			// If we can't find the symbol in the JIT, try looking in the host process.
	//			if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
	//				return JITSymbol(SymAddr, JITSymbolFlags::Exported);
	//
	//#ifdef _WIN32
	//			// For Windows retry without "_" at beginning, as RTDyldMemoryManager uses
	//			// GetProcAddress and standard libraries like msvcrt.dll use names
	//			// with and without "_" (for example "_itoa" but "sin").
	//			if (Name.length() > 2 && Name[0] == '_')
	//				if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name.substr(1)))
	//					return JITSymbol(SymAddr, JITSymbolFlags::Exported);
	//#endif
	//
	//			return nullptr;
	//		}
	//
	//		JITSymbol findMangledSymbol(const std::string& Name)
	//		{
	//#ifdef _WIN32
	//			// The symbol lookup of ObjectLinkingLayer uses the SymbolRef::SF_Exported
	//			// flag to decide whether a symbol will be visible or not, when we call
	//			// IRCompileLayer::findSymbolIn with ExportedSymbolsOnly set to true.
	//			//
	//			// But for Windows COFF objects, this flag is currently never set.
	//			// For a potential solution see: https://reviews.llvm.org/rL258665
	//			// For now, we allow non-exported symbols on Windows as a workaround.
	//			const bool ExportedSymbolsOnly = false;
	//#else
	//			const bool ExportedSymbolsOnly = true;
	//#endif
	//
	//			// Search modules in reverse order: from last added to first added.
	//			// This is the opposite of the usual search order for dlsym, but makes more
	//			// sense in a REPL where we want to bind to the newest available definition.
	//			for (auto H : make_range(ModuleKeys.rbegin(), ModuleKeys.rend()))
	//				if (auto Sym = CompileLayer.findSymbolIn(H, Name, ExportedSymbolsOnly))
	//					return Sym;
	//
	//			// If we can't find the symbol in the JIT, try looking in the host process.
	//			if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
	//				return JITSymbol(SymAddr, JITSymbolFlags::Exported);
	//
	//#ifdef _WIN32
	//			// For Windows retry without "_" at beginning, as RTDyldMemoryManager uses
	//			// GetProcAddress and standard libraries like msvcrt.dll use names
	//			// with and without "_" (for example "_itoa" but "sin").
	//			if (Name.length() > 2 && Name[0] == '_')
	//				if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name.substr(1)))
	//					return JITSymbol(SymAddr, JITSymbolFlags::Exported);
	//#endif
	//
	//			return nullptr;
	//		}
	//
	//		ExecutionSession ES;
	//		std::shared_ptr<SymbolResolver> Resolver;
	//		std::unique_ptr<TargetMachine> TM;
	//		const DataLayout DL;
	//		ObjLayerT ObjectLayer;
	//		CompileLayerT CompileLayer;
	//		std::vector<VModuleKey> ModuleKeys;
	//	};
}; // namespace slljit

#endif // LLVM_EXECUTIONENGINE_ORC_SHADERJIT_H
