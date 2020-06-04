cmake_minimum_required(VERSION 3.17)

project(Compiler VERSION "0.0.0")
include(CheckCXXCompilerFlag)

option(SLLJIT_OPT_AVX "Build with AVX instructions set." OFF)
option(SLLJIT_OPT_AVX2 "Build with AVX2 instructions set." OFF)
option(SLLJIT_OPT_AVX512 "Build with AVX512 instructions set." OFF)

if(MSVC)
    set(AVX_FLAG "/arch:AVX")
    set(AVX2_FLAG "/arch:AVX2")
    set(AVX512_FLAG "/arch:AVX512")
else()
    set(AVX_FLAG "-mavx")
    set(AVX2_FLAG "-mavx2")
    set(AVX512_FLAG "-mavx512")
endif()

set(OPT_LEVELS
        ${SLLJIT_OPT_AVX512}
        ${SLLJIT_OPT_AVX2}
        ${SLLJIT_OPT_AVX})

set(OPT_FLAGS 
        ${AVX512_FLAG}
        ${AVX2_FLAG}
        ${AVX_FLAG})

set(OPT_FLAG_NAMES 
        AVX512
        AVX2
        AVX)

CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_ARCH_NATIVE_SUPPORTED)

if(${COMPILER_OPT_ARCH_NATIVE_SUPPORTED})
    #target_compile_options(CompilerCLI PUBLIC -mavx2)
    add_compile_options(-march=native)
endif()

foreach(b_use f_name flag IN ZIP_LISTS OPT_LEVELS OPT_FLAG_NAMES OPT_FLAGS)
    if(${b_use})
        set(CHEK_VAR_NAME "COMPILER_OPT_ARCH_${f_name}_SUPPORTED")
        CHECK_CXX_COMPILER_FLAG(${flag} ${CHEK_VAR_NAME})
        if(${${CHEK_VAR_NAME}})
            add_compile_options(${flag})
            break()
        endif()
    endif()
endforeach()

if(MSVC)
    CHECK_CXX_COMPILER_FLAG("/Zc:__cplusplus" MSVC_UPDATED_CPLUSPLUS_SUPPORTED)
    if(${MSVC_UPDATED_CPLUSPLUS_SUPPORTED})
        #target_compile_options(CompilerCLI PUBLIC /Zc:__cplusplus)
        add_compile_options(/Zc:__cplusplus)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "")

#set(COMPILE_DEFINITIONS "_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING=1;_HAS_EXCEPTIONS=0")
add_definitions(-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING=1 -D_HAS_EXCEPTIONS=0)

set(LLVM_TARGETS_TO_BUILD "X86" CACHE INTERNAL "")

set(LLVM_ENABLE_BINDINGS OFF CACHE INTERNAL "")
set(LLVM_BUILD_RUNTIME OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_RUNTIMES OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_DOCS OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_EXAMPLES OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_TESTS OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_TOOLS OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_UTILS OFF CACHE INTERNAL "")
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE INTERNAL "")
set(LLVM_ENABLE_DUMP ON CACHE INTERNAL "")
set(LLVM_OPTIMIZED_TABLEGEN ON CACHE INTERNAL "")

add_subdirectory(llvm-project/llvm EXCLUDE_FROM_ALL)

set(LLVM_DIR "${CMAKE_CURRENT_BINARY_DIR}/llvm-project/llvm/lib/cmake/llvm")

find_package(LLVM CONFIG)

add_subdirectory(src)
