#pragma once
static void testFreezeGuards(const juce::File& root){
 auto dir=root.getChildFile("freeze-guards");require(dir.createDirectory().wasOk(),"Freeze guard directory failed");Session s(dir);
 auto& hosted=s.engine.getDeviceManager().getHostedAudioDeviceInterface();te::HostedAudioDeviceInterface::Parameters params;params.sampleRate=48000;params.blockSize=128;params.inputChannels=0;params.outputChannels=2;hosted.initialise(params);hosted.prepareToPlay(48000,128);
 auto source=dir.getChildFile("source.wav");require(root.getChildFile("source.wav").copyFileTo(source),"Freeze guard copy failed");s.importAudio(source,0,0);
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,juce::File(STUDIO_SOURCE_ROOT).getChildFile("build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3").getFullPathName());require(types.size()==1,"Freeze guard fixture missing");s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(*types[0]);
 auto plugin=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*types[0]);s.trackAt(0).pluginList.insertPlugin(plugin,0,nullptr);plugin=nullptr;
 auto effect=[&]{auto* p=s.trackAt(0).pluginList.findFirstPluginOfType<te::ExternalPlugin>();require(p!=nullptr,"Freeze guard lost plugin");p->initialiseFully();require(p->getAudioPluginInstance()!=nullptr,"Freeze guard plugin unavailable");return p;};effect();s.save();int checks=0;
 auto rejected=[&](auto action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,message);++checks;};
 s.setTrackFrozen(0,true);s.save();s.reopen();s.validateFrozenTracks();++checks;
 auto* p=effect();p->getAudioPluginInstance()->getParameters()[0]->setValueNotifyingHost(0.25f);p->flushPluginStateToValueTree();
 auto hash=juce::SHA256(s.projectFile).toHexString();rejected([&]{s.save();},"Stale parameter save accepted");require(juce::SHA256(s.projectFile).toHexString()==hash,"Rejected save changed project");
 auto output=dir.getChildFile("stale.wav");rejected([&]{s.render(output);},"Stale parameter render accepted");require(!output.exists(),"Stale render published");s.setTrackFrozen(0,false);s.setTrackFrozen(0,true);s.validateFrozenTracks();++checks;
 juce::MemoryBlock bytes;require(source.loadFileAsData(bytes),"Cannot retain test media");require(source.replaceWithText("changed media"),"Cannot mutate test media");rejected([&]{s.validateFrozenTracks();},"Changed source accepted");require(source.replaceWithData(bytes.getData(),bytes.getSize()),"Cannot restore test media");s.validateFrozenTracks();
 s.edit->tempoSequence.getTempo(0)->setBpm(90);rejected([&]{s.validateFrozenTracks();},"Changed tempo accepted");s.edit->tempoSequence.getTempo(0)->setBpm(120);s.validateFrozenTracks();
 s.clips()[0]->setStart(Session::time(10),false,true);rejected([&]{s.validateFrozenTracks();},"Changed clip accepted");s.setTrackFrozen(0,false);
 bool observedProgress=false;rejected([&]{s.setTrackFrozen(0,true,[&](float progress){observedProgress=observedProgress||progress>0;return progress>0;});},"Cancelled freeze accepted");
 require(observedProgress&&!s.trackAt(0).isFrozen(te::Track::individualFreeze)&&s.clips().size()==1,"Cancelled freeze lost edit");require(std::abs(effect()->getAudioPluginInstance()->getParameters()[0]->getValue()-0.25f)<1e-7f,"Cancelled freeze lost state");
 s.setTrackFrozen(0,true);s.save();s.validateFrozenTracks();++checks;s.setTrackFrozen(0,false);
 // The native writer must fail cleanly when its cache destination is a directory.
 auto blocked=te::TemporaryFileManager::getFreezeFileForTrack(s.trackAt(0));if(blocked.existsAsFile()){auto retained=blocked.getSiblingFile(blocked.getFileName()+".retained");require(blocked.moveFileTo(retained),"Cannot retain probe cache");}require(blocked.createDirectory().wasOk(),"Cannot block probe cache");require(blocked.getChildFile("blocker.txt").replaceWithText("retain native writer failure"),"Cannot block native writer");
 rejected([&]{s.setTrackFrozen(0,true);},"Failed freeze accepted");require(!s.trackAt(0).isFrozen(te::Track::individualFreeze)&&s.clips().size()==1,"Failed freeze lost edit");effect();
 auto bus=s.addBus();s.route(0,bus);rejected([&]{s.setTrackFrozen(bus,true);},"Unverified input bus freeze accepted");s.route(0,-1);s.addSend(0,bus,0,true);rejected([&]{s.setTrackFrozen(0,true);},"Unsupported send freeze accepted");rejected([&]{s.setTrackFrozen(bus,true);},"Unverified return freeze accepted");
 auto* report=new juce::DynamicObject;report->setProperty("checks",checks);report->setProperty("cancel_during_render",observedProgress);report->setProperty("stale_save_render_rejected",true);report->setProperty("physical_device_opened",false);dir.getChildFile("freeze-guards-result.json").replaceWithText(juce::JSON::toString(juce::var(report)));
}
