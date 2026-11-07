#pragma once
static void mixerTest(Session& original){
    auto dir=original.root.getChildFile("mixer");require(dir.createDirectory().wasOk(),"mixer directory");Session s(dir);
    auto source=dir.getChildFile("signal.wav");juce::AudioBuffer<float> signal(2,48000);signal.clear();signal.setSample(0,12000,0.001f);signal.setSample(1,24000,-0.0005f);
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),48000,2,32,{},0));require(writer&&writer->writeFromAudioSampleBuffer(signal,0,48000),"mixer fixture");writer.reset();
    s.importAudio(source,0,0);s.importAudio(source,1,0);int checks=0;float maxError=0;
    auto verify=[&](const char* name,float left,float right){auto path=dir.getChildFile(juce::String(name)+".wav");s.render(path);auto result=readWave(path);for(int c=0;c<2;++c)for(int n=0;n<48000;++n){float expected=signal.getSample(c,n)*(c==0?left:right);maxError=std::max(maxError,std::abs(result.getSample(c,n)-expected));}require(maxError<=1e-6f,name);++checks;};
    auto reject=[&](auto operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}require(rejected,"invalid routing accepted");++checks;};
    verify("sum",2,2);
    s.setMix(0,-6.0205999f,0,false,false);verify("fader",1.5f,1.5f);
    s.setMix(1,0,0,true,false);verify("mute",0.5f,0.5f);
    s.setMix(1,0,0,false,false);s.setMix(0,0,0,false,true);verify("solo",1,1);
    s.setMix(0,0,-1,false,true);verify("pan-left",2,0);
    s.setMix(0,0,1,false,true);verify("pan-right",0,2);
    s.setMix(0,0,0,false,false);int bus=s.addBus();s.route(0,bus);s.route(1,bus);s.setMix(bus,-6.0205999f,0,false,false);verify("bus",1,1);
    reject([&]{s.route(bus,0);});reject([&]{s.route(0,0);});reject([&]{s.route(0,99);});
    s.setMaster(-6.0205999f);verify("master",0.5f,0.5f);s.setMaster(0);
    s.save();s.reopen();verify("saved-routing",1,1);
    auto userTemplate=dir.getChildFile("routing.swtemplate");s.saveTemplate(userTemplate);s.loadTemplate(userTemplate);require(s.trackAt(0).getOutput().getDestinationTrack()==&s.trackAt(bus),"template routing lost");++checks;
    // Fresh branches independently compare pre/post-fader sends.
    for(bool pre:{false,true}){
        s.projectFile=dir.getChildFile(pre?"pre.tracktionedit":"post.tracktionedit");s.reset();s.importAudio(source,0,0);int destination=s.addBus();s.setMix(0,-6.0205999f,0,false,false);s.addSend(0,destination,0,pre);
        verify(pre?"pre-send":"post-send",pre?1.5f:1.0f,pre?1.5f:1.0f);
        reject([&]{s.route(destination,0);});reject([&]{s.addSend(0,destination,0,pre);});
        s.save();s.reopen();verify(pre?"pre-saved":"post-saved",pre?1.5f:1.0f,pre?1.5f:1.0f);
    }
    require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"mixer opened audio device");
    dir.getChildFile("mixer-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"max_error\":"+juce::String(maxError,9)+",\"device_opened\":false}");
}
