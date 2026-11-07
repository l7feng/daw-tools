// Original native failure fixture: exercised only in disposable scan processes.
#define createPluginFilter createHealthyPluginFilter
#include "../gain_fixture.cpp"
#undef createPluginFilter
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){
    if(auto* temp=std::getenv("TEMP"))juce::File(juce::String::fromUTF8(temp)).getChildFile("native-scan-entered-"+juce::String(STUDIO_SCAN_FAULT)+".txt").replaceWithText("entered original VST3 factory");
#if STUDIO_SCAN_FAULT == 1
    std::_Exit(73);
#else
    juce::Thread::sleep(60000);
#endif
    return createHealthyPluginFilter();
}
