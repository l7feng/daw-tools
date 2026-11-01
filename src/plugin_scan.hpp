#pragma once
// This path constructs no Session/Engine or device manager.
static void scanPlugin(const juce::File& root) {
    auto request=juce::JSON::parse(root.getChildFile("scan-request.json"));
    require(request.isObject(),"Invalid scan request");
    auto path=request["path"].toString();
    require(juce::File::isAbsolutePath(path),"Plugin path must be absolute");
    juce::File file(path);require(file.exists()&&file.hasFileExtension("vst3"),"VST3 candidate missing or invalid");
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,path);
    require(!descriptions.isEmpty(),"No VST3 descriptions found");
    if(bool(request["verifyStudioFixture"])) {
        require(descriptions.size()==1&&descriptions[0]->name=="Studio Test Gain","Fixture identity mismatch");
        juce::String error;auto instance=format.createInstanceFromDescription(*descriptions[0],48000,128,error);
        require(instance!=nullptr,"Fixture instance creation failed");
        require(instance->getParameters().size()>=1,"Fixture parameter missing");
        auto measure=[&](juce::AudioPluginInstance& plugin,int block,float expected){
            plugin.setNonRealtime(true);plugin.setPlayConfigDetails(2,2,48000,block);plugin.prepareToPlay(48000,block);
            require(plugin.getTotalNumInputChannels()==2&&plugin.getTotalNumOutputChannels()==2,"Configured fixture must have two input/output channels");
            juce::AudioBuffer<float> audio(2,block);audio.clear();audio.setSample(0,17,0.001f);audio.setSample(1,31,-0.002f);juce::MidiBuffer midi;
            plugin.processBlock(audio,midi);float maxError=0;
            for(int c=0;c<2;++c)for(int n=0;n<block;++n){float value=(c==0&&n==17)?0.001f*expected:(c==1&&n==31)?-0.002f*expected:0;maxError=std::max(maxError,std::abs(audio.getSample(c,n)-value));}
            plugin.releaseResources();require(maxError<=1e-7f,"Fixture audio differs from expected gain");
        };
        measure(*instance,128,0.5f);instance->getParameters()[0]->setValueNotifyingHost(0.25f);
        measure(*instance,256,0.25f);juce::MemoryBlock state;instance->getStateInformation(state);
        auto restored=format.createInstanceFromDescription(*descriptions[0],48000,128,error);require(restored!=nullptr,"Restored fixture creation failed");
        restored->setStateInformation(state.getData(),int(state.getSize()));measure(*restored,128,0.25f);
        require(root.getChildFile("fixture-result.json").replaceWithText("{\"default_gain\":0.5,\"restored_gain\":0.25,\"blocks\":[128,256],\"configured_input_channels\":2,\"configured_output_channels\":2,\"checks\":3}"),"Fixture report failed");
    }
    juce::XmlElement list("PLUGIN_SCAN");list.setAttribute("path",path);
    for(auto* description:descriptions)list.addChildElement(description->createXml().release());
    require(list.writeTo(root.getChildFile("scan-result.xml")),"Cannot write scan result");
}
