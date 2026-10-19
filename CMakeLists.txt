cmake_minimum_required(VERSION 3.24)
project(Vocalis VERSION 1.0.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Vocalis requires Windows x64 and MSVC.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850)
    FetchContent_MakeAvailable(JUCE)
endif()

include(FetchContent)
foreach(dependency IN ITEMS signalsmith-linear signalsmith-stretch)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/${dependency}/CMakeLists.txt")
        add_subdirectory("external/${dependency}")
    else()
        if(dependency STREQUAL "signalsmith-linear")
            set(repository "linear")
            set(revision de55e6a50ffcf6f8f43f649692d94691c7025151)
        else()
            set(repository "signalsmith-stretch")
            set(revision a670068d9aeb64913331d5cc29337b19a457a7df)
        endif()
        FetchContent_Declare(${dependency}
            GIT_REPOSITORY "https://github.com/Signalsmith-Audio/${repository}.git"
            GIT_TAG ${revision}
            GIT_SUBMODULES "")
        FetchContent_MakeAvailable(${dependency})
    endif()
endforeach()

juce_add_binary_data(VocalisAssets SOURCES Assets/VocalisLogo.png)

juce_add_plugin(Vocalis
    COMPANY_NAME "Vocalis Audio"
    BUNDLE_ID "audio.vocalis.plugin"
    PLUGIN_MANUFACTURER_CODE Vcls
    PLUGIN_CODE Voca
    PRODUCT_NAME "Vocalis"
    FORMATS VST3
    VST3_CATEGORIES Fx Tools
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Vocalis PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/dsp/PitchShifter.cpp Source/dsp/PitchShifter.h
    Source/dsp/PitchDetector.cpp Source/dsp/PitchDetector.h
    Source/dsp/Humanizer.cpp Source/dsp/Humanizer.h
    Source/dsp/PitchCorrector.cpp Source/dsp/PitchCorrector.h
    Source/dsp/FormantProcessor.cpp Source/dsp/FormantProcessor.h Source/ParameterIDs.h
    Source/dsp/BloomMapping.h
    Source/presets/PresetManager.cpp Source/presets/PresetManager.h
    Source/ui/VocalisTheme.h
    Source/ui/VocalisLogo.cpp Source/ui/VocalisLogo.h
    Source/ui/BubbleButton.cpp Source/ui/BubbleButton.h
    Source/ui/PresetCapsule.cpp Source/ui/PresetCapsule.h
    Source/ui/SectionPanel.cpp Source/ui/SectionPanel.h
    Source/ui/LevelMeter.cpp Source/ui/LevelMeter.h
    Source/ui/BloomControl.cpp Source/ui/BloomControl.h
    Source/ui/BubbleKnob.cpp Source/ui/BubbleKnob.h
    Source/ui/VocalisLookAndFeel.cpp Source/ui/VocalisLookAndFeel.h
    Source/ui/PitchOrb.cpp Source/ui/PitchOrb.h)
target_compile_definitions(Vocalis PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Vocalis PRIVATE juce::juce_audio_utils signalsmith-stretch VocalisAssets
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
set_source_files_properties(Source/dsp/PitchShifter.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
add_custom_command(TARGET Vocalis_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES"
        "${CMAKE_BINARY_DIR}/Vocalis_artefacts/$<CONFIG>/VST3/Vocalis.vst3/Contents/Resources/Licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_BINARY_DIR}/Vocalis_artefacts/$<CONFIG>/VST3/Vocalis.vst3/Contents/Resources/THIRD_PARTY_NOTICES.md"
    VERBATIM)

option(VOCALIS_BUILD_TESTS "Build processor and VST3 smoke tests" OFF)
if(VOCALIS_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(VocalisTests PRODUCT_NAME "Vocalis Tests")
    # ULTRA research code is intentionally test-only until quality/latency gates pass.
    target_sources(VocalisTests PRIVATE Tests/ProcessorTests.cpp Tests/PitchTests.cpp
        Tests/LatencyBenchmark.cpp Tests/AuditBenchmark.cpp Tests/LatencyTests.cpp Tests/FormantTests.cpp Tests/CorrectionTests.cpp Tests/HumanizerTests.cpp Tests/PresetTests.cpp Tests/UiTests.cpp Tests/StabilityTests.cpp Tests/LiveStudy.cpp Tests/UltraLiveTests.cpp Source/dsp/live/UltraLiveEngine.cpp Tests/VocalValidation.cpp Tests/reference/Phase11UltraLiveEngine.cpp)
    target_compile_definitions(VocalisTests PRIVATE JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(VocalisTests PRIVATE Vocalis juce::juce_audio_utils signalsmith-stretch)
    add_dependencies(VocalisTests Vocalis_VST3)
    add_test(NAME VocalisReleaseRegression COMMAND VocalisTests
        "${CMAKE_BINARY_DIR}/Vocalis_artefacts/$<CONFIG>/VST3/Vocalis.vst3")
endif()

