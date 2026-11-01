# Pending activation after realtime benchmark completes.
juce_add_plugin(StudioMidiFixture COMPANY_NAME "StudioWorkbench" PLUGIN_MANUFACTURER_CODE Stwb
 PLUGIN_CODE Stmi FORMATS VST3 PRODUCT_NAME "Studio MIDI Timing Fixture" VERSION 0.1.0
 IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(StudioMidiFixture PRIVATE midi_fixture.cpp)
target_compile_features(StudioMidiFixture PRIVATE cxx_std_20)
target_compile_definitions(StudioMidiFixture PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(StudioMidiFixture PRIVATE juce::juce_audio_utils)
juce_add_console_app(MidiVerify PRODUCT_NAME "Studio MIDI Verify")
target_sources(MidiVerify PRIVATE midi_verify.cpp)
target_compile_features(MidiVerify PRIVATE cxx_std_20)
target_compile_definitions(MidiVerify PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(MidiVerify PRIVATE juce::juce_audio_utils)
