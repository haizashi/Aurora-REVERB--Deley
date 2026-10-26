cmake_minimum_required(VERSION 3.22)
project(Aurora VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

option(AURORA_COPY_AFTER_BUILD "Copy the built plugin into the system plugin folder" ON)

juce_add_plugin(Aurora
    COMPANY_NAME "AuroraAudio"
    PLUGIN_MANUFACTURER_CODE Auaa
    PLUGIN_CODE Aurr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Aurora Reverb Delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${AURORA_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx Reverb Delay)

target_sources(Aurora PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Aurora PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Aurora
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
