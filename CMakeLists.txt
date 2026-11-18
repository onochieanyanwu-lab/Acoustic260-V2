cmake_minimum_required(VERSION 3.22)
project(Acoustic260V2 VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime so the VST3 doesn't need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Acoustic260V2
    COMPANY_NAME "Onochie"
    PLUGIN_MANUFACTURER_CODE Onoc
    PLUGIN_CODE A2V2
    FORMATS VST3
    PRODUCT_NAME "Acoustic 260 V2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# All source files sit in the same folder as this file (flat layout, survives GitHub web upload)
target_sources(Acoustic260V2 PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(Acoustic260V2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Acoustic260V2
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
