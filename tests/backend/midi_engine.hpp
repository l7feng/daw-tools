#pragma once
static void testMidiEngine(const juce::File& root){
 auto dir=root.getChildFile("midi-engine");dir.createDirectory();Session s(dir);auto* track=s.addTrack();
 auto beat=[](double v){return tracktion::BeatPosition::fromBeats(v);};
 s.edit->tempoSequence.getTempo(0)->set(beat(0),120,1,false);s.edit->tempoSequence.insertTempo(beat(2),60,1);
 auto clip=track->insertMIDIClip("Timing",{Session::time(0),Session::time(144000)},nullptr);require(clip!=nullptr,"MIDI clip creation failed");
 auto& seq=clip->getSequence();seq.addNote(60,beat(1),tracktion::BeatDuration::fromBeats(0.5),127,0,nullptr);seq.addNote(67,beat(3),tracktion::BeatDuration::fromBeats(0.5),64,0,nullptr);
 auto fixture=juce::File(STUDIO_SOURCE_ROOT).getChildFile("build/backend/StudioMidiFixture_artefacts/Release/VST3/Studio MIDI Timing Fixture.vst3");
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,fixture.getFullPathName());require(descriptions.size()==1,"MIDI fixture scan failed");
 s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(*descriptions[0]);
 auto item=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*descriptions[0]);auto* external=dynamic_cast<te::ExternalPlugin*>(item.get());require(external!=nullptr,"MIDI plugin creation failed");track->pluginList.insertPlugin(item,0,nullptr);external->initialiseFully();require(external->getAudioPluginInstance()!=nullptr,"MIDI instrument load failed");
 // Pinned MidiNote::getEditEndTime subtracts 0.0001s: 4.8 samples at 48kHz.
 float error=0;int cases=0;
 auto verify=[&](const char* name,int block){auto output=dir.getChildFile(juce::String(name)+".wav");s.render(output,block,144000);juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));require(reader&&reader->lengthInSamples==144000&&reader->numChannels==2&&reader->sampleRate==48000,"MIDI render format differs");juce::AudioBuffer<float> data(2,144000);require(reader->read(&data,0,144000,0,true,true),"MIDI WAV read failed");
  int first=-1,second=-1;
  for(int i=0;i<144000;++i){if(data.getSample(0,i)>1e-5f){if(first<0)first=i;if(i>48000&&second<0)second=i;}
   if(std::abs(i-24000)<=2||std::abs(i-35995)<=1||std::abs(i-96000)<=2||std::abs(i-119995)<=1)continue;
   float expected=i>=24000&&i<35995?0.001f:(i>=96000&&i<119995?0.001f*64/127:0);
   for(int c=0;c<2;++c)error=std::max(error,std::abs(data.getSample(c,i)-expected));
  }
  dir.getChildFile(juce::String(name)+"-edges.txt").replaceWithText(juce::String(first)+","+juce::String(second)+",error="+juce::String(error,10));
  require(std::abs(first-24000)<=1&&std::abs(second-96000)<=1&&error<1e-7f,"Tempo or MIDI render differs");++cases;
 };
 verify("block128",128);verify("block256",256);s.save();clip=nullptr;item=nullptr;s.reopen();verify("reopened128",128);verify("reopened256",256);
 require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"MIDI render opened device");
 auto* report=new juce::DynamicObject;juce::var value(report);report->setProperty("cases",cases);report->setProperty("max_error",error);report->setProperty("device_opened",false);dir.getChildFile("midi-engine-result.json").replaceWithText(juce::JSON::toString(value));
}
