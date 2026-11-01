#pragma once
static void testMidiTransport(const juce::File& root){
 auto dir=root.getChildFile("midi-transport");dir.createDirectory();Session s(dir);s.edit.reset();
 auto& hosted=s.engine.getDeviceManager().getHostedAudioDeviceInterface();te::HostedAudioDeviceInterface::Parameters params;params.sampleRate=48000;params.blockSize=128;params.inputChannels=0;params.outputChannels=2;hosted.initialise(params);hosted.prepareToPlay(48000,128);
 te::Edit::Options options{s.engine,te::createEmptyEdit(s.engine),te::ProjectItemID::createNewID(0)};options.role=te::Edit::forEditing;options.numAudioTracks=0;options.defaultMasterVolumedB=0;options.editFileRetriever=[&s]{return s.projectFile;};s.edit=std::make_unique<te::Edit>(options);
 auto* track=s.addTrack();auto clip=track->insertMIDIClip("Held note",{Session::time(0),Session::time(192000)},nullptr);require(clip!=nullptr,"Held note clip failed");
 s.edit->tempoSequence.getTempo(0)->setBpm(120);clip->getSequence().addNote(60,tracktion::BeatPosition::fromBeats(0.5),tracktion::BeatDuration::fromBeats(5),127,0,nullptr);
 auto fixture=juce::File(STUDIO_SOURCE_ROOT).getChildFile("build/backend/StudioMidiFixture_artefacts/Release/VST3/Studio MIDI Timing Fixture.vst3");juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,fixture.getFullPathName());require(types.size()==1,"Transport fixture scan failed");s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(*types[0]);
 auto plugin=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*types[0]);auto* external=dynamic_cast<te::ExternalPlugin*>(plugin.get());require(external!=nullptr,"Transport plugin failed");track->pluginList.insertPlugin(plugin,0,nullptr);external->initialiseFully();require(external->getAudioPluginInstance()!=nullptr,"Transport instrument load failed");
 auto process=[&](int blocks){float peak=0;juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
  for(int i=0;i<blocks;++i){audio.clear();midi.clear();hosted.processBlock(audio,midi);peak=std::max(peak,audio.getMagnitude(0,128));juce::MessageManager::getInstance()->runDispatchLoopUntil(1);}return peak;};
 auto& transport=s.edit->getTransport();process(10);transport.setPosition(Session::time(0));transport.play(false);float playing=process(300);require(playing>0.0009f,"Held note never sounded");
 transport.setPosition(Session::time(144000));process(40);float located=process(60);require(located<1e-7f,"Locate left stuck note");
 transport.setPosition(Session::time(0));require(process(300)>0.0009f,"Playback after locate failed");transport.stop(false,false);process(40);float stopped=process(60);require(stopped<1e-7f,"Stop left stuck note");
 transport.stop(false,true);clip=nullptr;plugin=nullptr;
 auto* report=new juce::DynamicObject;juce::var result(report);report->setProperty("checks",4);report->setProperty("playing_peak",playing);report->setProperty("located_peak",located);report->setProperty("stopped_peak",stopped);report->setProperty("physical_device_opened",false);dir.getChildFile("midi-transport-result.json").replaceWithText(juce::JSON::toString(result));
}
