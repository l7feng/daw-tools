cmake_minimum_required(VERSION 3.22)
project(DawTools VERSION 0.1.6 LANGUAGES CXX)
if(MSVC)
  add_compile_options(/utf-8)
endif()
add_executable(PlatformProbe src/platform_probe.cpp)
target_compile_features(PlatformProbe PRIVATE cxx_std_17)
target_link_libraries(PlatformProbe PRIVATE winmm)

option(STUDIO_BUILD_ENGINE "Build pinned upstream engine validation runner" OFF)
if(STUDIO_BUILD_ENGINE)
  enable_language(C)
  if(MSVC)
    # This installed toolchain has Chinese resources only. Match UTF-8 /showIncludes.
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:")
  endif()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_subdirectory(third_party/JUCE-19edd538429c93d277bf95b55aaa7e3eb545f951)
  add_subdirectory(third_party/tracktion_engine-3.2.0/modules)
  add_subdirectory(third_party/tracktion_engine-3.2.0/examples/TestRunner)
  get_target_property(runner_sources TestRunner SOURCES)
  list(FILTER runner_sources EXCLUDE REGEX "TestRunner\\.(cpp|h)$")
  set_property(TARGET TestRunner PROPERTY SOURCES "${runner_sources}")
  target_sources(TestRunner PRIVATE tests/isolated_main.cpp)
  target_include_directories(TestRunner PRIVATE "${CMAKE_SOURCE_DIR}/build/generated")
  juce_add_plugin(StudioTestGain COMPANY_NAME "StudioWorkbench" PLUGIN_MANUFACTURER_CODE Stwb
    PLUGIN_CODE Stgn FORMATS VST3 PRODUCT_NAME "Studio Test Gain" VERSION 0.1.0
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(StudioTestGain PRIVATE tests/gain_fixture.cpp)
  target_compile_features(StudioTestGain PRIVATE cxx_std_20)
  target_compile_definitions(StudioTestGain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(StudioTestGain PRIVATE juce::juce_audio_utils)
  juce_add_gui_app(DawTools PRODUCT_NAME "DAW Tools" VERSION 0.1.6
    COMPANY_NAME "l7feng"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/daw-tools-512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/daw-tools-512.png")
  juce_generate_juce_header(DawTools)
  if(WIN32)
    # Pinned JUCE's RC custom command omits its generated Info.txt dependency.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DawTools_artefacts/JuceLibraryCode/DawTools_resources.rc"
      APPEND DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/DawTools_artefacts/JuceLibraryCode/Info.txt")
  endif()
  target_sources(DawTools PRIVATE src/workbench.cpp)
  set_property(SOURCE src/workbench.cpp PROPERTY OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/plugin_scan.hpp;${CMAKE_SOURCE_DIR}/src/export_job.hpp;${CMAKE_SOURCE_DIR}/src/session.hpp;${CMAKE_SOURCE_DIR}/src/editor.hpp;${CMAKE_SOURCE_DIR}/tests/offline_cases.hpp;${CMAKE_SOURCE_DIR}/tests/editor_cases.hpp;${CMAKE_SOURCE_DIR}/tests/mixer_cases.hpp;${CMAKE_SOURCE_DIR}/tests/export_cases.hpp")
  target_compile_features(DawTools PRIVATE cxx_std_20)
  target_compile_definitions(DawTools PRIVATE
    STUDIO_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1 TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)
  target_link_libraries(DawTools PRIVATE tracktion::tracktion_engine tracktion::tracktion_core
    tracktion::tracktion_graph juce::juce_audio_utils juce::juce_cryptography)
endif()
