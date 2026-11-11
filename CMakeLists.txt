cmake_minimum_required(VERSION 3.22)

project(Smoosher VERSION 1.1.0)

option(SMOOSHER_COPY_PLUGIN_AFTER_BUILD "Install plugins after building" OFF)

set(SMOOSHER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SMOOSHER_FORMATS AU)
endif()

# Fetch JUCE from GitHub
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Add the plugin target
juce_add_plugin(Smoosher
    COMPANY_NAME "Helvete Sound, LLC"
    BUNDLE_ID com.helvetesound.smoosher
    PLUGIN_MANUFACTURER_CODE Smsh
    PLUGIN_CODE Smsh
    FORMATS ${SMOOSHER_FORMATS}
    PRODUCT_NAME "Smoosher"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SMOOSHER_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
)

# Add source files
target_sources(Smoosher
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Link required JUCE modules
target_link_libraries(Smoosher
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Generate JUCE header
juce_generate_juce_header(Smoosher)

# Set C++ standard
target_compile_features(Smoosher PRIVATE cxx_std_17)

# Compiler definitions
target_compile_definitions(Smoosher
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

option(SMOOSHER_BUILD_TESTS "Build processor and preset regression tests" OFF)
if(SMOOSHER_BUILD_TESTS)
    enable_testing()
    add_executable(SmoosherTests tests/ProcessorTests.cpp)
    target_include_directories(SmoosherTests PRIVATE
        Source
        "${CMAKE_CURRENT_BINARY_DIR}/Smoosher_artefacts/JuceLibraryCode"
        "${juce_SOURCE_DIR}/modules"
    )
    target_link_libraries(SmoosherTests PRIVATE Smoosher)
    target_compile_features(SmoosherTests PRIVATE cxx_std_17)
    add_test(NAME SmoosherProcessor COMMAND SmoosherTests)
endif()
