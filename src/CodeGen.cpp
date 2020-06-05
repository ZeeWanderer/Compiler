#include "pch.h"
#include "CodeGen.h"
#include "Layout.h"

using namespace slljit;
using namespace llvm;
using namespace llvm::orc;
using namespace std;

namespace slljit
{
	void CodeGen::compile_layout(Context& m_context, LocalContext& m_local_context, Layout& m_layout)
	{
		std::vector<Type*> StructMembers;
		StructMembers.reserve(m_layout.globals.size());

		for (auto& global : m_layout.globals)
		{
			m_local_context.LLVM_Module->getOrInsertGlobal(global.name, Type::getDoublePtrTy(m_context.LLVM_Context));
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.name);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantPointerNull::get(Type::getDoublePtrTy(m_context.LLVM_Context)));

			//	indices.emplace_back(ConstantInt::get(m_context.LLVM_Context, APInt(32, struct_idx, true)));

			switch (global.type)
			{
			case slljit::Kdouble: StructMembers.emplace_back(Type::getDoubleTy(m_context.LLVM_Context));
			default: break;
			}
		}

		auto loader_struct_type = StructType::create(m_context.LLVM_Context, StructMembers, "layout__", false);

		// generate loader function
		std::vector<Type*> Args(1, loader_struct_type->getPointerTo());
		FunctionType* FT = FunctionType::get(Type::getVoidTy(m_context.LLVM_Context), Args, false);

		Function* TheFunction = Function::Create(FT, Function::ExternalLinkage, "__layout_loader_", m_local_context.LLVM_Module.get());

		// Set names for all arguments.
		auto arg            = TheFunction->arg_begin();
		Value* data_pointer = arg;
		arg->setName("data_ptr");

		BasicBlock* BB = BasicBlock::Create(m_context.LLVM_Context, "entry", TheFunction);
		m_context.LLVM_Builder.SetInsertPoint(BB);

		auto& g_list = m_local_context.LLVM_Module->getGlobalList();
		std::vector<Value*> indices{m_context.LLVM_Builder.getInt32(0), m_context.LLVM_Builder.getInt32(0)};
		uint32_t idx = 0;
		for (auto& g_var : g_list)
		{
			Value* gep = m_context.LLVM_Builder.CreateGEP(data_pointer, indices);
			m_context.LLVM_Builder.CreateStore(gep, &g_var);
			idx++;
			indices[1] = m_context.LLVM_Builder.getInt32(idx);
		}
		m_context.LLVM_Builder.CreateRet(nullptr);

		for (auto& global : m_layout.constant_globals)
		{
			m_local_context.LLVM_Module->getOrInsertGlobal(global.first, Type::getDoubleTy(m_context.LLVM_Context));
			GlobalVariable* gVar = m_local_context.LLVM_Module->getNamedGlobal(global.first);
			gVar->setLinkage(GlobalValue::ExternalLinkage);
			gVar->setInitializer(ConstantFP::get(m_context.LLVM_Context, APFloat(global.second.value)));
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
				fprintf(stderr, "\n");
			}
		}

		bool Cnaged_init = m_local_context.LLVM_FPM->doInitialization();
		//	m_local_context.LLVM_Module->dump();
		for (auto& it : m_local_context.LLVM_Module->functions())
		{
			m_local_context.LLVM_FPM->run(it);
		}

		m_local_context.LLVM_PM->run(*m_local_context.LLVM_Module.get());

		bool Cnaged_fin = m_local_context.LLVM_FPM->doFinalization();
		m_local_context.LLVM_Module->dump();

		auto key = m_context.shllJIT->addModule(std::move(m_local_context.LLVM_Module));
		m_local_context.set_key(key);
	}
}; // namespace slljit
