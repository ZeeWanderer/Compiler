cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)

find_package(Taskflow CONFIG)

set(CompilerCLI_SRC
		../include/pch.h
		../pch.cpp
		compiler_cli.cpp)

add_executable(CompilerCLI ${CompilerCLI_SRC})
target_include_directories(CompilerCLI PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(CompilerCLI PUBLIC "../include")
target_link_libraries(CompilerCLI PUBLIC slljit PRIVATE Taskflow::Taskflow)
add_dependencies(CompilerCLI slljit)

set(LLVM_K_TEST_SRC
		../include/pch.h
		../include/JIT.h
		llvm_k_test.cpp)

add_executable(llvm_k_test ${LLVM_K_TEST_SRC})
target_include_directories(llvm_k_test PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(llvm_k_test PUBLIC "../include")
target_link_libraries(llvm_k_test ${LLVM_TARGETS})
add_dependencies(llvm_k_test ${LLVM_TARGETS})

set(CODEGEN_TESTS_SRC
		../include/pch.h
		../include/JIT.h
		codegen_tests.cpp)

add_executable(codegen_tests ${CODEGEN_TESTS_SRC})
target_include_directories(codegen_tests PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(codegen_tests PUBLIC "../include")
target_link_libraries(codegen_tests ${LLVM_TARGETS})
add_dependencies(codegen_tests ${LLVM_TARGETS})