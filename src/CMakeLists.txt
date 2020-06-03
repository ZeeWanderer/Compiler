cmake_minimum_required(VERSION 2.6)

set(COMPILERCLI_SRC
		include/pch.h
		include/AST.h
		include/CodeGen.h
		include/Context.h
		include/JIT.h
		include/Layout.h
		include/Parser.h
		include/Program.hpp
		include/Tokenizer.h
		pch.cpp
		AST.cpp
		CodeGen.cpp
		Context.cpp
		Layout.cpp
		Parser.cpp
		Tokenizer.cpp
		compiler_cli.cpp)

add_executable (CompilerCLI ${COMPILERCLI_SRC})

target_include_directories(CompilerCLI PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(CompilerCLI PUBLIC "./include")

set(LLVM_TARGETS
  LLVMAnalysis
  LLVMCore
  LLVMExecutionEngine
  LLVMInstCombine
  LLVMObject
  LLVMOrcJIT
  LLVMRuntimeDyld
  LLVMScalarOpts
  LLVMSupport
  LLVMTransformUtils
  LLVMX86CodeGen
  LLVMX86AsmParser
  LLVMX86Desc
  LLVMX86Disassembler
  LLVMX86Info
  LLVMJITLink
  LLVMPasses
  LLVMipo
  LLVMInstrumentation
  LLVMVectorize
  LLVMIRReader
  LLVMAsmParser
  LLVMLinker
  LLVMAsmPrinter
  LLVMDebugInfoDWARF
  LLVMCFGuard
  LLVMGlobalISel
  LLVMSelectionDAG
  LLVMCodeGen
  LLVMAggressiveInstCombine
  LLVMTarget
  LLVMBitWriter
  LLVMProfileData
  LLVMBitReader
  LLVMRemarks
  LLVMBitstreamReader
  LLVMTextAPI
  LLVMMCParser
  LLVMMCDisassembler
  LLVMMC
  LLVMBinaryFormat
  LLVMDebugInfoCodeView
  LLVMDebugInfoMSF
  LLVMDemangle
)

target_link_libraries(CompilerCLI ${LLVM_TARGETS})

add_dependencies(CompilerCLI ${LLVM_TARGETS})
