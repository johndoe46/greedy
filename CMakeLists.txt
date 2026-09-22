cmake_minimum_required(VERSION 3.22)
project(Greedy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(GREEDY_BUILD_PLUGIN "Build the JUCE plugin" ON)
set(GREEDY_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
set(greedy_default_formats VST3)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND greedy_default_formats AU)
endif()
set(GREEDY_PLUGIN_FORMATS "${greedy_default_formats}" CACHE STRING "JUCE plugin formats to build")

add_library(greedy_engine Source/PatternEngine.cpp Source/MidiSequencer.cpp)
target_include_directories(greedy_engine PUBLIC Source)
set_target_properties(greedy_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(CTest)
if(BUILD_TESTING)
    add_executable(greedy_tests Tests/EngineTests.cpp)
    target_link_libraries(greedy_tests PRIVATE greedy_engine)
    add_test(NAME greedy_engine COMMAND greedy_tests)
endif()

if(GREEDY_BUILD_PLUGIN)
    if(GREEDY_JUCE_PATH)
        add_subdirectory("${GREEDY_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce")
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.tar.gz
            URL_HASH SHA256=0eafd736286c6e7bb6958d79fce18d42cb654b8e231890d327b7cece379bd156)
        FetchContent_MakeAvailable(JUCE)
    endif()

    # The manifest helper loads the plugin, which cannot run on the build host
    # when cross-compiling. Hosts can scan the bundle without this optional file.
    set(greedy_auto_manifest TRUE)
    if(CMAKE_CROSSCOMPILING)
        set(greedy_auto_manifest FALSE)
    endif()

    juce_add_plugin(Greedy
        COMPANY_NAME "Greedy"
        BUNDLE_ID "org.greedy.sequencer"
        PLUGIN_MANUFACTURER_CODE Grdy
        PLUGIN_CODE Grd1
        FORMATS ${GREEDY_PLUGIN_FORMATS}
        PRODUCT_NAME "Greedy"
        IS_SYNTH FALSE
        IS_MIDI_EFFECT TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        VST3_CATEGORIES Fx Generator
        AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
        VST_NUM_MIDI_INS 1
        VST_NUM_MIDI_OUTS 1
        VST3_AUTO_MANIFEST ${greedy_auto_manifest}
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(Greedy PRIVATE
        Source/PluginProcessor.cpp Source/PluginProcessor.h
        Source/PluginEditor.cpp Source/PluginEditor.h)
    target_compile_definitions(Greedy PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(Greedy PRIVATE greedy_engine juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND TARGET Greedy_VST3)
        # JUCE generates this plist, but CMake's Ninja bundle step does not copy it.
        # Hosts such as Bitwig reject a VST3 bundle without this metadata.
        add_custom_command(TARGET Greedy_VST3 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PROPERTY:Greedy_VST3,MACOSX_BUNDLE_INFO_PLIST>"
                "$<TARGET_BUNDLE_DIR:Greedy_VST3>/Contents/Info.plist"
            VERBATIM)
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_CROSSCOMPILING)
            # The generated moduleinfo and copied plist must be sealed after JUCE's
            # post-build manifest step has finished modifying the bundle.
            add_custom_command(TARGET Greedy_VST3 POST_BUILD
                COMMAND /usr/bin/codesign --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:Greedy_VST3>"
                VERBATIM)
        endif()
    endif()

    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows" AND TARGET Greedy_VST3)
        # JUCE autolinks these mixed-case filenames. xwin's libraries are
        # lowercase, and the Linux linker searches a case-sensitive filesystem.
        target_link_options(Greedy_VST3 PRIVATE
            /NODEFAULTLIB:DbgHelp.lib /NODEFAULTLIB:Dwrite.lib
            /NODEFAULTLIB:D2d1.lib /NODEFAULTLIB:DComp.lib)
        target_link_libraries(Greedy_VST3 PRIVATE dbghelp dwrite d2d1 dcomp)
    endif()

    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # Sign each Mach-O slice at link time, including the Apple Silicon slice.
        foreach(greedy_format IN LISTS GREEDY_PLUGIN_FORMATS)
            if(TARGET Greedy_${greedy_format})
                target_link_options(Greedy_${greedy_format} PRIVATE LINKER:-adhoc_codesign)
            endif()
        endforeach()
    endif()

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
        set(GREEDY_WINDOWS_SDK_ROOT "$ENV{GREEDY_WINDOWS_SDK_ROOT}" CACHE PATH
            "Windows SDK/CRT directory produced by xwin splat")
        set(GREEDY_WINDOWS_BUILD_DIR "${CMAKE_BINARY_DIR}/windows" CACHE PATH
            "Separate build directory for the Windows VST3")
        set(GREEDY_WINDOWS_BUILD_JOBS "2" CACHE STRING "Parallel jobs for the Windows build")
        add_custom_target(windows-vst3
            COMMAND "${CMAKE_COMMAND}" -S "${PROJECT_SOURCE_DIR}" -B "${GREEDY_WINDOWS_BUILD_DIR}"
                -G "${CMAKE_GENERATOR}"
                "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                "-DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/cmake/toolchains/windows-clang.cmake"
                "-DGREEDY_WINDOWS_SDK_ROOT=${GREEDY_WINDOWS_SDK_ROOT}"
                "-DGREEDY_JUCE_PATH=${JUCE_SOURCE_DIR}"
                -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DGREEDY_BUILD_PLUGIN=ON
                -DGREEDY_PLUGIN_FORMATS=VST3
            COMMAND "${CMAKE_COMMAND}" --build "${GREEDY_WINDOWS_BUILD_DIR}"
                --config Release --target Greedy_VST3 --parallel "${GREEDY_WINDOWS_BUILD_JOBS}"
            COMMAND "${CMAKE_COMMAND}" -E chdir
                "${GREEDY_WINDOWS_BUILD_DIR}/Greedy_artefacts/Release/VST3"
                "${CMAKE_COMMAND}" -E tar cf "${CMAKE_BINARY_DIR}/Greedy-Windows-x64.zip"
                --format=zip Greedy.vst3
            BYPRODUCTS "${CMAKE_BINARY_DIR}/Greedy-Windows-x64.zip"
            COMMENT "Cross-compiling the 64-bit Windows Greedy VST3"
            USES_TERMINAL VERBATIM)

        set(GREEDY_MACOS_SDK_ROOT "$ENV{GREEDY_MACOS_SDK_ROOT}" CACHE PATH "Path to a macOS SDK directory")
        set(GREEDY_MACOS_LLVM_ROOT "$ENV{GREEDY_MACOS_LLVM_ROOT}" CACHE PATH "LLVM installation prefix (optional)")
        set(GREEDY_MACOS_BUILD_DIR "${CMAKE_BINARY_DIR}/macos" CACHE PATH "Separate build directory for the macOS AU")
        set(GREEDY_MACOS_BUILD_JOBS "2" CACHE STRING "Parallel jobs for the macOS build")
        set(GREEDY_MACOS_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures to build")
        set(GREEDY_MACOS_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
        add_custom_target(macos-au
            COMMAND "${CMAKE_COMMAND}" -S "${PROJECT_SOURCE_DIR}" -B "${GREEDY_MACOS_BUILD_DIR}"
                -G "${CMAKE_GENERATOR}"
                "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                "-DCMAKE_TOOLCHAIN_FILE=${PROJECT_SOURCE_DIR}/cmake/toolchains/macos-clang.cmake"
                "-DGREEDY_MACOS_SDK_ROOT=${GREEDY_MACOS_SDK_ROOT}"
                "-DGREEDY_MACOS_LLVM_ROOT=${GREEDY_MACOS_LLVM_ROOT}"
                "-DCMAKE_OSX_ARCHITECTURES=${GREEDY_MACOS_ARCHITECTURES}"
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=${GREEDY_MACOS_DEPLOYMENT_TARGET}"
                "-DGREEDY_JUCE_PATH=${JUCE_SOURCE_DIR}"
                -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DGREEDY_BUILD_PLUGIN=ON
                -DGREEDY_PLUGIN_FORMATS=AU
            COMMAND "${CMAKE_COMMAND}" --build "${GREEDY_MACOS_BUILD_DIR}"
                --config Release --target Greedy_AU --parallel "${GREEDY_MACOS_BUILD_JOBS}"
            COMMAND "${CMAKE_COMMAND}" -E chdir
                "${GREEDY_MACOS_BUILD_DIR}/Greedy_artefacts/Release/AU"
                "${CMAKE_COMMAND}" -E tar cf "${CMAKE_BINARY_DIR}/Greedy-macOS-AU.zip"
                --format=zip Greedy.component
            BYPRODUCTS "${CMAKE_BINARY_DIR}/Greedy-macOS-AU.zip"
            COMMENT "Cross-compiling the macOS Greedy AU MIDI effect"
            USES_TERMINAL VERBATIM)
    endif()

    if(BUILD_TESTING)
        add_executable(greedy_plugin_tests Tests/PluginTests.cpp)
        target_link_libraries(greedy_plugin_tests PRIVATE Greedy)
        target_include_directories(greedy_plugin_tests PRIVATE Source
            "$<TARGET_PROPERTY:juce::juce_audio_processors,INTERFACE_INCLUDE_DIRECTORIES>")
        target_compile_definitions(greedy_plugin_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        add_test(NAME greedy_plugin COMMAND greedy_plugin_tests)
    endif()
endif()
