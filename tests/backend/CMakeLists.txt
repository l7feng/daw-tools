cmake_minimum_required(VERSION 3.22)
project(StudioBackendProbe VERSION 0.1.0 LANGUAGES C CXX)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(MSVC)
 add_compile_options(/utf-8)
 set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_subdirectory("${REPO}/third_party/JUCE-19edd538429c93d277bf95b55aaa7e3eb545f951" juce)
add_subdirectory("${REPO}/third_party/tracktion_engine-3.2.0/modules" engine)
juce_add_gui_app(BackendProbe PRODUCT_NAME "Studio Backend Probe")
juce_generate_juce_header(BackendProbe)
target_sources(BackendProbe PRIVATE main.cpp)
target_compile_features(BackendProbe PRIVATE cxx_std_20)
target_compile_definitions(BackendProbe PRIVATE STUDIO_SOURCE_ROOT="${REPO}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1 TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)
target_link_libraries(BackendProbe PRIVATE tracktion::tracktion_engine tracktion::tracktion_core tracktion::tracktion_graph juce::juce_audio_utils juce::juce_cryptography)

juce_add_plugin(StudioTestDelay COMPANY_NAME "StudioWorkbench" PLUGIN_MANUFACTURER_CODE Stwb
 PLUGIN_CODE Stdl FORMATS VST3 PRODUCT_NAME "Studio Test Delay" VERSION 0.1.0
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(StudioTestDelay PRIVATE delay_fixture.cpp)
target_compile_features(StudioTestDelay PRIVATE cxx_std_20)
target_compile_definitions(StudioTestDelay PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(StudioTestDelay PRIVATE juce::juce_audio_utils)

# Pinned official ASIO SDK, local validation only; no driver installation.
set(STUDIO_ASIO_COMMON "${REPO}/third_party/asio-2.3.4/ASIOSDK/common")
if(NOT EXISTS "${STUDIO_ASIO_COMMON}/iasiodrv.h")
 message(FATAL_ERROR "Restore pinned ASIO SDK from dependencies.lock.json")
endif()
target_include_directories(BackendProbe PRIVATE "${STUDIO_ASIO_COMMON}")
target_compile_definitions(BackendProbe PRIVATE JUCE_ASIO=1)

include(midi_targets.cmake)

juce_add_plugin(StudioTestTail COMPANY_NAME "StudioWorkbench" PLUGIN_MANUFACTURER_CODE Stwb
 PLUGIN_CODE Sttl FORMATS VST3 PRODUCT_NAME "Studio Test Tail" VERSION 0.1.0
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(StudioTestTail PRIVATE tail_fixture.cpp)
target_compile_features(StudioTestTail PRIVATE cxx_std_20)
target_compile_definitions(StudioTestTail PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(StudioTestTail PRIVATE juce::juce_audio_utils)
