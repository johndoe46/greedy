# Cross-compile a macOS AU with upstream LLVM and a macOS SDK.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

set(GREEDY_MACOS_SDK_ROOT "$ENV{GREEDY_MACOS_SDK_ROOT}" CACHE PATH "Path to a macOS SDK directory")
set(GREEDY_MACOS_LLVM_ROOT "$ENV{GREEDY_MACOS_LLVM_ROOT}" CACHE PATH "LLVM installation prefix (optional)")
if(NOT EXISTS "${GREEDY_MACOS_SDK_ROOT}/SDKSettings.json"
   OR NOT EXISTS "${GREEDY_MACOS_SDK_ROOT}/System/Library/Frameworks/AudioUnit.framework")
    message(FATAL_ERROR
        "Set GREEDY_MACOS_SDK_ROOT to a macOS SDK directory (e.g. MacOSX14.5.sdk). "
        "See README.md for macOS cross-build setup.")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GREEDY_MACOS_SDK_ROOT GREEDY_MACOS_LLVM_ROOT)

find_program(GREEDY_MACOS_CLANG NAMES clang clang-21 clang-20 clang-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(GREEDY_MACOS_CLANGXX NAMES clang++ clang++-21 clang++-20 clang++-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(CMAKE_LINKER NAMES ld64.lld ld64.lld-21 ld64.lld-20 ld64.lld-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(CMAKE_AR NAMES llvm-ar llvm-ar-21 llvm-ar-20 llvm-ar-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib llvm-ranlib-21 llvm-ranlib-20 llvm-ranlib-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(CMAKE_INSTALL_NAME_TOOL NAMES llvm-install-name-tool llvm-install-name-tool-21
    llvm-install-name-tool-20 llvm-install-name-tool-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)
find_program(GREEDY_MACOS_LIPO NAMES llvm-lipo llvm-lipo-21 llvm-lipo-20 llvm-lipo-19
    HINTS "${GREEDY_MACOS_LLVM_ROOT}/bin" REQUIRED)

# Clang invokes "lipo" for multi-architecture objects and bundles. Supply the
# LLVM implementation without adding aliases to the user's LLVM installation.
set(GREEDY_MACOS_TOOL_DIR "${CMAKE_BINARY_DIR}/llvm-tools" CACHE PATH "Darwin tool aliases")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GREEDY_MACOS_TOOL_DIR)
file(MAKE_DIRECTORY "${GREEDY_MACOS_TOOL_DIR}")
file(CREATE_LINK "${GREEDY_MACOS_LIPO}" "${GREEDY_MACOS_TOOL_DIR}/lipo" SYMBOLIC)

set(CMAKE_OSX_SYSROOT "${GREEDY_MACOS_SDK_ROOT}" CACHE PATH "macOS sysroot")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
foreach(_greedy_language C CXX OBJC OBJCXX)
    if(_greedy_language STREQUAL "C" OR _greedy_language STREQUAL "OBJC")
        set(CMAKE_${_greedy_language}_COMPILER "${GREEDY_MACOS_CLANG}")
    else()
        set(CMAKE_${_greedy_language}_COMPILER "${GREEDY_MACOS_CLANGXX}")
    endif()
    set(CMAKE_${_greedy_language}_COMPILER_TARGET "arm64-apple-macos${CMAKE_OSX_DEPLOYMENT_TARGET}")
    # The linker version tells Clang to use modern -platform_version arguments.
    set(CMAKE_${_greedy_language}_FLAGS_INIT "-B\"${GREEDY_MACOS_TOOL_DIR}\" -mlinker-version=711")
endforeach()
foreach(_greedy_kind EXE SHARED MODULE)
    set(CMAKE_${_greedy_kind}_LINKER_FLAGS_INIT "-fuse-ld=\"${CMAKE_LINKER}\"")
endforeach()

list(APPEND CMAKE_FIND_ROOT_PATH "${GREEDY_MACOS_SDK_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
