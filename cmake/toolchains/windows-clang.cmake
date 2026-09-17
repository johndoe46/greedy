# Cross-compile for 64-bit Windows using LLVM and an xwin SDK/CRT directory.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_SYSTEM_VERSION 10.0)

set(GREEDY_WINDOWS_SDK_ROOT "$ENV{GREEDY_WINDOWS_SDK_ROOT}" CACHE PATH
    "Windows SDK/CRT directory produced by xwin splat")
if(NOT EXISTS "${GREEDY_WINDOWS_SDK_ROOT}/crt/include/vcruntime.h"
   OR NOT EXISTS "${GREEDY_WINDOWS_SDK_ROOT}/sdk/include/um/windows.h")
    message(FATAL_ERROR
        "Set GREEDY_WINDOWS_SDK_ROOT to the directory produced by xwin splat. "
        "See README.md for Windows cross-build setup.")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GREEDY_WINDOWS_SDK_ROOT)

find_program(GREEDY_CLANG_CL NAMES clang-cl clang-cl-21 clang-cl-20 clang-cl-19 REQUIRED)
find_program(CMAKE_LINKER NAMES lld-link lld-link-21 lld-link-20 lld-link-19 REQUIRED)
find_program(CMAKE_AR NAMES llvm-lib llvm-lib-21 llvm-lib-20 llvm-lib-19 REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES llvm-rc llvm-rc-21 llvm-rc-20 llvm-rc-19 REQUIRED)
find_program(CMAKE_MT NAMES llvm-mt llvm-mt-21 llvm-mt-20 llvm-mt-19 REQUIRED)
set(CMAKE_C_COMPILER "${GREEDY_CLANG_CL}")
set(CMAKE_CXX_COMPILER "${GREEDY_CLANG_CL}")
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded CACHE STRING "Static release MSVC runtime")

set(_greedy_includes "")
foreach(_greedy_include crt/include sdk/include/ucrt sdk/include/shared
                        sdk/include/um sdk/include/winrt sdk/include/cppwinrt)
    string(APPEND _greedy_includes " /imsvc\"${GREEDY_WINDOWS_SDK_ROOT}/${_greedy_include}\"")
endforeach()
set(CMAKE_C_FLAGS_INIT "${_greedy_includes}")
set(CMAKE_CXX_FLAGS_INIT "${_greedy_includes}")
set(CMAKE_RC_FLAGS_INIT
    "-I \"${GREEDY_WINDOWS_SDK_ROOT}/sdk/include/um\" -I \"${GREEDY_WINDOWS_SDK_ROOT}/sdk/include/shared\"")

set(_greedy_libraries "")
foreach(_greedy_library crt/lib/x86_64 sdk/lib/ucrt/x86_64 sdk/lib/um/x86_64)
    string(APPEND _greedy_libraries " /libpath:\"${GREEDY_WINDOWS_SDK_ROOT}/${_greedy_library}\"")
endforeach()
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_greedy_libraries}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_greedy_libraries}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_greedy_libraries}")

list(APPEND CMAKE_FIND_ROOT_PATH "${GREEDY_WINDOWS_SDK_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
