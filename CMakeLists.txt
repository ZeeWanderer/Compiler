cmake_minimum_required(VERSION 3.21)

project(Compiler VERSION "0.0.0")
include(CheckCXXCompilerFlag)

if(MSVC)
    CHECK_CXX_COMPILER_FLAG("/Zc:__cplusplus" MSVC_UPDATED_CPLUSPLUS_SUPPORTED)
    if(${MSVC_UPDATED_CPLUSPLUS_SUPPORTED})
        #target_compile_options(CompilerCLI PUBLIC /Zc:__cplusplus)
        add_compile_options(/Zc:__cplusplus)
    endif()

    foreach(config DEBUG RELWITHDEBINFO RELEASE)
        foreach(lang C CXX)
            set(flags_var "CMAKE_${lang}_FLAGS_${config}")
            string(REPLACE "/Zi" "/Z7" ${flags_var} "${${flags_var}}")
       endforeach()
    endforeach()

    #message("looking for buildcache")
    #find_program(buildcache_program buildcache)
    #message("buildcache: ${buildcache_program}")
    #if(buildcache_program)
    #    message("buildcache_program found")
    #    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${buildcache_program}")
    #endif()
endif()

if(MINGW)
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) # Less useful to do it for linking, see edit2
    endif(CCACHE_FOUND)
    set(LLVM_CCACHE_BUILD ON CACHE INTERNAL "")
endif()

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
