cmake_minimum_required(VERSION 2.6)

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
  LLVMFrontendOpenMP
)

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
target_link_libraries(CompilerCLI ${LLVM_TARGETS})
add_dependencies(CompilerCLI ${LLVM_TARGETS})

set(LLVM_K_TEST_SRC
		include/pch.h
		include/JIT.h
		llvm_k_test.cpp)

add_executable (llvm_k_test ${LLVM_K_TEST_SRC})
target_include_directories(llvm_k_test PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(llvm_k_test PUBLIC "./include")
target_link_libraries(llvm_k_test ${LLVM_TARGETS})
add_dependencies(llvm_k_test ${LLVM_TARGETS})

set(CODEGEN_TESTS_SRC
		include/pch.h
		include/JIT.h
		codegen_tests.cpp)

add_executable (codegen_tests ${CODEGEN_TESTS_SRC})
target_include_directories(codegen_tests PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(codegen_tests PUBLIC "./include")
target_link_libraries(codegen_tests ${LLVM_TARGETS})
add_dependencies(codegen_tests ${LLVM_TARGETS})

install(TARGETS CompilerCLI llvm_k_test codegen_tests)
