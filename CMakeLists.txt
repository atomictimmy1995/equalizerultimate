cmake_minimum_required(VERSION 3.22)

project(EqualizerUltimate VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EQU_BUILD_TESTS "Build the headless DSP/processor tests" ON)
set(EQU_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout (otherwise JUCE is downloaded)")

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------
if(EQU_JUCE_PATH)
    add_subdirectory("${EQU_JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.6
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
# Plugin
# ------------------------------------------------------------------------------
juce_add_plugin(EqualizerUltimate
    COMPANY_NAME                "EqualizerUltimate"
    PRODUCT_NAME                "Equalizer Ultimate"
    BUNDLE_ID                   "com.equalizerultimate.equalizerultimate"
    PLUGIN_MANUFACTURER_CODE    Equl
    PLUGIN_CODE                 Eq8b
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx EQ
    COPY_PLUGIN_AFTER_BUILD     FALSE)

set(EQU_SOURCES
    Source/EQBand.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ResponseCurve.cpp)

target_sources(EqualizerUltimate PRIVATE ${EQU_SOURCES})

set(EQU_COMPILE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(EqualizerUltimate PUBLIC ${EQU_COMPILE_DEFINITIONS})

set(EQU_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics)

target_link_libraries(EqualizerUltimate
    PRIVATE
        ${EQU_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Tests (headless console app that runs the processor offline)
# ------------------------------------------------------------------------------
if(EQU_BUILD_TESTS)
    juce_add_console_app(EqualizerUltimateTests PRODUCT_NAME "EqualizerUltimateTests")
    target_sources(EqualizerUltimateTests PRIVATE Tests/Main.cpp ${EQU_SOURCES})
    target_include_directories(EqualizerUltimateTests PRIVATE Source)
    target_compile_definitions(EqualizerUltimateTests PRIVATE ${EQU_COMPILE_DEFINITIONS})
    target_link_libraries(EqualizerUltimateTests
        PRIVATE
            ${EQU_JUCE_MODULES}
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME EqualizerUltimateTests COMMAND EqualizerUltimateTests)
endif()
