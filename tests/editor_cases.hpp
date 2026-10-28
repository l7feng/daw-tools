#pragma once
static void editorTest(Session& original) {
    auto dir=original.root.getChildFile("editing");require(dir.createDirectory().wasOk(),"test directory failed");Session s(dir);
    auto source=dir.getChildFile(juce::String::fromUTF8("测试音频.wav"));
    juce::AudioBuffer<float> fixture(2,48000);fixture.clear();fixture.setSample(0,4800,0.001f);fixture.setSample(1,9600,-0.0005f);
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),48000,2,32,{},0));
    require(writer && writer->writeFromAudioSampleBuffer(fixture,0,48000),"fixture write failed");writer.reset();
    auto sourceHash=juce::SHA256(source).toHexString();
    int checks=0;
    auto expectRejected=[&](auto fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}require(failed,"invalid operation accepted");++checks;};
    expectRejected([&]{s.importAudio(dir.getChildFile("missing.wav"),0,0);});require(te::getAudioTracks(*s.edit).isEmpty(),"invalid import created track");
    auto* clip=s.importAudio(source,0,0);require(s.clips().size()==1,"import clip count");++checks;
    s.move(*clip,4800);require(Session::samples(clip->getPosition().getStart())==4800,"move position");++checks;
    s.undo();require(Session::samples(s.clips()[0]->getPosition().getStart())==0,"undo move");++checks;
    s.redo();require(Session::samples(s.clips()[0]->getPosition().getStart())==4800,"redo move");++checks;
    clip=dynamic_cast<te::WaveAudioClip*>(s.clips()[0]);require(clip!=nullptr,"audio clip lost");
    expectRejected([&]{s.move(*clip,-1);});
    expectRejected([&]{s.trim(*clip,0,100);});
    s.trim(*clip,7200,48000);require(std::llround(clip->getPosition().getOffset().inSeconds()*48000)==2400,"trim source offset");++checks;
    expectRejected([&]{s.split(*clip,7200);});
    s.split(*clip,12000);require(s.clips().size()==2,"split count");++checks;
    auto* copy=dynamic_cast<te::WaveAudioClip*>(s.duplicate(*clip,24000));require(copy!=nullptr && s.clips().size()==3,"copy count");++checks;
    s.setEnvelope(*copy,-6.0205999f,480,480);++checks;
    s.erase(*copy);require(s.clips().size()==2,"delete clip");++checks;
    s.undo();require(s.clips().size()==3,"undo delete clip");++checks;
    s.save();auto beforeCount=s.clips().size();
    auto invalid=dir.getChildFile("invalid.tracktionedit");invalid.replaceWithText("<NOT_AN_EDIT/>");
    expectRejected([&]{s.openProject(invalid);});require(s.clips().size()==beforeCount,"invalid open changed current project");
    s.reopen();require(s.clips().size()==3,"save/open clip count");++checks;
    s.saveAs(dir.getChildFile("copy.tracktionedit"));++checks;
    expectRejected([&]{s.saveAs(dir.getChildFile("copy.tracktionedit"));});
    expectRejected([&]{s.render(source);});
    auto savedProject=s.projectFile;auto savedHash=juce::SHA256(savedProject).toHexString();
    auto blocker=dir.getChildFile("not-a-directory");blocker.replaceWithText("block parent creation");
    expectRejected([&]{s.saveAs(blocker.getChildFile("failed.tracktionedit"));});
    require(s.projectFile==savedProject&&juce::SHA256(savedProject).toHexString()==savedHash&&s.clips().size()==3,"failed save damaged active project");++checks;
    s.save();require(juce::XmlDocument::parse(savedProject)!=nullptr,"atomic replacement invalid");++checks;
    s.render(dir.getChildFile("edit-float.wav"),128);s.render(dir.getChildFile("edit-pcm24.wav"),256,48000,24);
    juce::AudioBuffer<float> expected(2,48000);expected.clear();expected.setSample(0,9600,0.001f);expected.setSample(1,14400,-0.0005f);expected.setSample(0,26400,0.0005f);
    float maxError=0;
    for(auto name:{"edit-float.wav","edit-pcm24.wav"}){auto rendered=readWave(dir.getChildFile(name));for(int c=0;c<2;++c)for(int n=0;n<48000;++n)maxError=std::max(maxError,std::abs(rendered.getSample(c,n)-expected.getSample(c,n)));++checks;}
    require(maxError<=1e-6f,"edited render differs from sample expectation");
    require(juce::SHA256(source).toHexString()==sourceHash,"source audio changed");++checks;
    require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"audio device opened");++checks;
    auto* linked=dynamic_cast<te::WaveAudioClip*>(s.clips()[0]);require(linked!=nullptr,"expected wave clip");
    linked->getSourceFileReference().setToDirectFileReference(dir.getChildFile("absent.wav"),false);
    require(s.missingMedia().size()==1,"missing media not reported");++checks;
    expectRejected([&]{s.relink(*linked,invalid);});
    s.relink(*linked,source);require(s.missingMedia().isEmpty(),"relink did not resolve media");++checks;
    auto recovery=s.checkpoint();require(recovery.existsAsFile(),"checkpoint absent");s.openProject(recovery);require(s.clips().size()==3,"checkpoint lost clips");++checks;
    auto userTemplate=dir.getChildFile("user.swtemplate");s.saveTemplate(userTemplate);
    s.loadTemplate(userTemplate);require(s.clips().isEmpty()&&te::getAudioTracks(*s.edit).size()==1,"user template did not preserve track structure without media");++checks;
    dir.getChildFile("editor-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"max_error\":"+juce::String(maxError,9)+",\"source_unchanged\":true,\"device_opened\":false}");
}

static void crossfadeTest(Session& original){
    auto dir=original.root.getChildFile("crossfade");require(dir.createDirectory().wasOk(),"crossfade dir failed");Session s(dir);
    auto file=dir.getChildFile("constant.wav");juce::AudioBuffer<float> audio(2,48000);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)audio.setSample(c,i,0.001f);
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),48000,2,32,{},0));require(writer&&writer->writeFromAudioSampleBuffer(audio,0,48000),"constant source failed");writer.reset();
    auto* left=s.importAudio(file,0,0);auto* right=s.importAudio(file,0,24000);s.crossfade(*left,*right);
    s.render(dir.getChildFile("crossfade.wav"),128,72000);auto rendered=readWave(dir.getChildFile("crossfade.wav"),72000);float error=0;
    for(int c=0;c<2;++c)for(int i=25000;i<47000;++i)error=std::max(error,std::abs(rendered.getSample(c,i)-0.001f));
    require(error<=1e-6f,"linear crossfade gain not continuous");dir.getChildFile("crossfade-result.json").replaceWithText("{\"max_overlap_error\":"+juce::String(error,9)+"}");
}
