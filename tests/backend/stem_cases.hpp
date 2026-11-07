#pragma once
static void testStems(const juce::File& root){
 auto dir=root.getChildFile("stems");dir.createDirectory();Session s(dir);auto source=root.getChildFile("source.wav");auto reference=readWave(source);
 s.importAudio(source,0,0);s.importAudio(source,1,0);s.setMix(0,-6.0205999f,0,false,false);s.setMaster(-6.0205999f);
 int checks=0;float error=0;
 auto verify=[&](const char* name,int track,float gain,int block,bool master=false){auto output=dir.getChildFile(juce::String(name)+".wav");s.render(output,block,48000,32,0,track,master);auto audio=readWave(output);
 for(int c=0;c<2;++c)for(int i=0;i<48000;++i)error=std::max(error,std::abs(audio.getSample(c,i)-reference.getSample(c,i)*gain));require(error<1e-6f,name);++checks;return audio;};
 auto first=verify("track0",0,0.5f,128);auto second=verify("track1",1,1,256);auto mix=verify("mix",-1,0.75f,128,true);
 for(int c=0;c<2;++c)for(int i=0;i<48000;++i)error=std::max(error,std::abs((first.getSample(c,i)+second.getSample(c,i))*0.5f-mix.getSample(c,i)));require(error<1e-6f,"Stem sum differs");++checks;
 int bus=s.addBus();s.route(0,bus);s.route(1,bus);s.setMix(bus,-6.0205999f,0,false,false);
 verify("source-through-bus",0,0.25f,256);verify("bus-sum",bus,0.75f,128);verify("bus-with-master",bus,0.375f,256,true);
 auto state=s.edit->state.toXmlString();auto saved=dir.getChildFile("mix.wav");auto hash=juce::SHA256(saved).toHexString();bool rejected=false;try{s.render(saved,128,48000,32,0,0,false);}catch(const std::exception&){rejected=true;}require(rejected&&juce::SHA256(saved).toHexString()==hash,"Stem overwrote existing output");++checks;
 auto invalid=dir.getChildFile("invalid.wav");rejected=false;try{s.render(invalid,128,48000,32,0,99,false);}catch(const std::exception&){rejected=true;}require(rejected&&!invalid.exists(),"Invalid stem accepted");++checks;
 require(s.edit->state.toXmlString()==state,"Stem failure changed session");s.save();s.reopen();verify("bus-reopened",bus,0.75f,256);

 for(bool pre:{false,true}){s.projectFile=dir.getChildFile(pre?"send-pre.tracktionedit":"send-post.tracktionedit");s.reset();s.importAudio(source,0,0);s.importAudio(source,1,0);s.setMix(0,-6.0205999f,0,false,false);int shared=s.addBus();s.addSend(0,shared,0,pre);s.addSend(1,shared,-6.0205999f,pre);
 verify(pre?"pre-source":"post-source",0,pre?1.5f:1.0f,128);verify(pre?"pre-return":"post-return",shared,pre?1.5f:1.0f,256);
 int next=s.addBus();s.route(shared,next);s.setMix(next,-6.0205999f,0,false,false);verify(pre?"pre-return-chain":"post-return-chain",shared,pre?0.75f:0.5f,128);
 }
 auto* report=new juce::DynamicObject;juce::var value(report);report->setProperty("checks",checks);report->setProperty("max_error",error);report->setProperty("device_opened",false);dir.getChildFile("stems-result.json").replaceWithText(juce::JSON::toString(value));
}
