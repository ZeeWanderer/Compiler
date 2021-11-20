cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)

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

set(slljit_INTERFACE
		include/pch.h
		include/AST.h
		include/CodeGen.h
		include/Context.h
		include/JIT.h
		include/Layout.h
		include/Parser.h
		include/Program.hpp
		include/Tokenizer.h 
		include/Types.h
		include/Instructions.h
		include/Error.h
		include/Options.h)

set(slljit_SRC
		pch.cpp
		AST.cpp
		CodeGen.cpp
		Context.cpp
		Layout.cpp
		Parser.cpp
		Tokenizer.cpp
		Types.cpp
		Instructions.cpp
		Error.cpp
		Options.cpp)

add_library(slljit STATIC ${slljit_SRC} ${slljit_INTERFACE})
target_include_directories(slljit PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(slljit PUBLIC "./include")
target_link_libraries(slljit ${LLVM_TARGETS})
add_dependencies(slljit ${LLVM_TARGETS})

set(CompilerCLI_SRC
		include/pch.h
		pch.cpp
		compiler_cli.cpp)

add_executable (CompilerCLI ${CompilerCLI_SRC})
target_include_directories(CompilerCLI PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(CompilerCLI PUBLIC "./include")
target_link_libraries(CompilerCLI slljit)
add_dependencies(CompilerCLI slljit)

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

#install(TARGETS slljit CompilerCLI llvm_k_test codegen_tests)
install(TARGETS slljit)
install(FILES ${slljit_INTERFACE} DESTINATION include)
