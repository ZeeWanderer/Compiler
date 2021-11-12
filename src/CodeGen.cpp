#include "pch.h"
#include "CodeGen.h"
#include "Layout.h"

#include "llvm/Support/raw_ostream.h"

using namespace slljit;
using namespace llvm;
using namespace llvm::orc;
using namespace std;

namespace slljit
{
	void CodeGen::compile_layout(Context& m_context, LocalContext& m_local_context, Layout& m_layout)
	{
		std::vector<Type*> StructMembers; // layout structure
		StructMembers.reserve(m_layout.globals.size());

		// Insert variable globals
		// Globals contain pointers to layout members, init with nullptr
		for (auto& global : m_layout.globals) 
		{
			Type* type_;
			switch (global.type)
			{
			case slljit::Kdouble:
				type_ = Type::getDoubleTy(*m_local_context.LLVM_Context);
				break;
			case slljit::Kint64:
				type_ = Type::getInt64Ty(*m_local_context.LLVM_Context);
				break;
			default: break;
			}
			m_local_context.LLVM_Module->getOrInsertGlobal(global.name, type_->getPointerTo());
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.name);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantPointerNull::get(type_->getPointerTo()));

			StructMembers.emplace_back(type_);
		}

		auto loader_struct_type = StructType::create(*m_local_context.LLVM_Context, StructMembers, "layout__", false);

		// generate loader function
		std::vector<Type*> Args(1, loader_struct_type->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getVoidTy(*m_local_context.LLVM_Context), Args, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, "__layout_loader_", m_local_context.LLVM_Module.get());

		// Set names for all arguments.
		auto arg            = TheFunction->arg_begin();
		Value* data_pointer = arg;
		arg->setName("data_ptr");

		BasicBlock* BB = BasicBlock::Create(*m_local_context.LLVM_Context, "entry", TheFunction);
		m_local_context.LLVM_Builder->SetInsertPoint(BB);

		auto& g_list = m_local_context.LLVM_Module->getGlobalList();
		std::vector<Value*> indices{m_local_context.LLVM_Builder->getInt32(0), m_local_context.LLVM_Builder->getInt32(0)};
		uint32_t idx = 0;
		// Load offsets into variable globals
		for (auto& g_var : g_list)
		{
			Value* gep = m_local_context.LLVM_Builder->CreateGEP(loader_struct_type, data_pointer, indices);
			m_local_context.LLVM_Builder->CreateStore(gep, &g_var);
			idx++;
			indices[1] = m_local_context.LLVM_Builder->getInt32(idx);
		}
		m_local_context.LLVM_Builder->CreateRet(nullptr);

		// Insert constant globals
		for (auto& global : m_layout.constant_globals)
		{
			Type* type_;
			Constant* init_c;
			switch (global.second.type)
			{
			case slljit::Kdouble:
				type_ = Type::getDoubleTy(*m_local_context.LLVM_Context);
				init_c = ConstantFP::get(*m_local_context.LLVM_Context, APFloat(global.second.valueD));
				break;
			case slljit::Kint64:
				type_ = Type::getInt64Ty(*m_local_context.LLVM_Context);
				init_c = ConstantInt::get(*m_local_context.LLVM_Context, APInt(64, global.second.valueI64, true));
				break;
			default: break;
			}

			m_local_context.LLVM_Module->getOrInsertGlobal(global.first, type_);
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.first);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(init_c);
			gVar->setConstant(true);
		}
	}
	void CodeGen::compile(std::list<std::unique_ptr<PrototypeAST>> prototypes, std::list<std::unique_ptr<FunctionAST>> functions, Context& m_context, LocalContext& m_local_context)
	{
		for (auto it = prototypes.begin(); it != prototypes.end(); ++it)
		{
			if (auto* FnIR = static_cast<Function*>((*it)->codegen(m_context, m_local_context)))
			{
				m_local_context.FunctionProtos[(*it)->getName()] = std::move((*it));
			}
		}

		for (auto it = functions.begin(); it != functions.end(); ++it)
		{
			if (auto* FnIR = static_cast<Function*>((*it)->codegen(m_context, m_local_context)))
			{
				// FnIR->print(errs());
				//	fprintf(stderr, "\n");
			}
		}
#if _DEBUG
		fprintf(stderr, "; PreOptimization:\n");
		m_local_context.LLVM_Module->dump();
#endif

		bool Cnaged_init = m_local_context.LLVM_FPM->doInitialization();
		assert(Cnaged_init == false);
		//	m_local_context.LLVM_Module->dump();
		for (auto& it : m_local_context.LLVM_Module->functions())
		{
			m_local_context.LLVM_FPM->run(it);
		}

		m_local_context.LLVM_PM->run(*m_local_context.LLVM_Module);

		bool Cnaged_fin = m_local_context.LLVM_FPM->doFinalization();
		assert(Cnaged_fin == false);

#if _DEBUG
		fprintf(stderr, "; PostOptimization:\n");
		m_local_context.LLVM_Module->dump();

		// ASSEMBLY
		std::string outStr;
		{
			/*	auto& TM = m_context.shllJIT->;
			 llvm::legacy::PassManager pm;

			 llvm::raw_string_ostream stream(outStr);

			 llvm::buffer_ostream pstream(stream);

			 TM.addPassesToEmitFile(pm, pstream, nullptr,

			     llvm::CodeGenFileType::CGFT_AssemblyFile);

			 pm.run(*m_local_context.LLVM_Module.get());*/
		}

		//	fprintf(stderr, "; Assembly:\n%s", outStr.c_str());
#endif
		auto RT = m_local_context.JD.getDefaultResourceTracker();

		auto TSM = ThreadSafeModule(std::move(m_local_context.LLVM_Module), std::move(m_local_context.LLVM_Context));

	
		Error err = m_context.shllJIT->addModule(std::move(TSM), RT);

		if (err)
		{
			// TODO: return compile error
		}

#if _DEBUG
		if (err)
			fprintf(stderr, "; error\n");

		fprintf(stderr, "; JDlib:\n");
		m_local_context.JD.dump(dbgs());
#endif
	}
}; // namespace slljit
