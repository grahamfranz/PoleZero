cmake_minimum_required(VERSION 3.22)
if(APPLE)
    project(PoleZero VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(PoleZero VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------------------------------------------
# JUCE
#
# Point CMake at a JUCE source tree (the one you cloned from the JUCE repo).
# Pass it on the command line with -DJUCE_PATH=/path/to/JUCE, or set the
# JUCE_PATH environment variable.
# ----------------------------------------------------------------------------
if(NOT DEFINED JUCE_PATH AND DEFINED ENV{JUCE_PATH})
    set(JUCE_PATH "$ENV{JUCE_PATH}")
endif()

if(NOT JUCE_PATH)
    message(FATAL_ERROR
        "JUCE_PATH is not set. Pass -DJUCE_PATH=/path/to/JUCE on the cmake "
        "command line, or set the JUCE_PATH environment variable to a JUCE "
        "source checkout (the directory containing JUCE's top-level "
        "CMakeLists.txt).")
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE_PATH='${JUCE_PATH}' does not contain a CMakeLists.txt. Point it "
        "at the root of a JUCE source checkout.")
endif()

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# ----------------------------------------------------------------------------
# Plugin target
# ----------------------------------------------------------------------------
juce_add_plugin(PoleZero
    COMPANY_NAME            "GrahamFranz"
    BUNDLE_ID               "com.grahamfranz.polezero"
    PLUGIN_MANUFACTURER_CODE Gfrz
    PLUGIN_CODE             Pzro
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "PoleZero"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(PoleZero PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ZPlaneComponent.cpp)

target_compile_definitions(PoleZero PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PoleZero PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
