#pragma once
static juce::AudioBuffer<float> readWave(const juce::File& file,int frames=48000) {
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
    require(reader!=nullptr,"wave decode failed");
    require(reader->sampleRate==48000 && reader->numChannels==2 && reader->lengthInSamples==frames,"unexpected render format/length");
    juce::AudioBuffer<float> b(2,frames);require(reader->read(&b,0,frames,0,true,true),"wave read failed");return b;
}
static void offlineCase(Session& s, int trackCount, int blockSize) {
    require(te::getAudioTracks(*s.edit).isEmpty(),"new project must have zero audio tracks");
    auto source=s.root.getChildFile("fixture.wav");
    require(!source.exists(),"fixture already exists");
    juce::AudioBuffer<float> fixture(2,48000); fixture.clear();
    fixture.setSample(0,4800,0.001f); fixture.setSample(1,9600,-0.0005f);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),48000,2,32,{},0));
    require(writer && writer->writeFromAudioSampleBuffer(fixture,0,48000),"fixture write failed"); writer.reset();
    for(int i=0;i<trackCount;++i) {
        auto* t=s.addTrack();
        auto clip=t->insertWaveClip("Synthetic impulse",source,
            {{tracktion::TimePosition::fromSeconds(0),tracktion::TimePosition::fromSeconds(1)},tracktion::TimeDuration::fromSeconds(0)},false);
        require(clip!=nullptr,"clip creation failed");
    }
    s.save(); s.render(s.root.getChildFile("before.wav"),blockSize);
    s.reopen(); require(te::getAudioTracks(*s.edit).size()==trackCount,"track count changed after reopen");
    s.render(s.root.getChildFile("after.wav"),blockSize);
    auto a=readWave(s.root.getChildFile("before.wav"));auto b=readWave(s.root.getChildFile("after.wav"));
    float signalError=0,roundtripError=0;
    for(int c=0;c<2;++c) for(int i=0;i<48000;++i) {
        signalError=std::max(signalError,std::abs(a.getSample(c,i)-fixture.getSample(c,i)*float(trackCount)));
        roundtripError=std::max(roundtripError,std::abs(a.getSample(c,i)-b.getSample(c,i)));
    }
    require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"audio device opened during offline render");
    juce::String result="{\"sample_rate\":48000,\"channels\":2,\"frames\":48000,\"tracks\":"+juce::String(trackCount)+",\"block_size\":"+juce::String(blockSize)+",\"max_signal_error\":"
        +juce::String(signalError,9)+",\"max_roundtrip_error\":"+juce::String(roundtripError,9)+",\"device_opened\":false}";
    require(s.root.getChildFile("signal-result.json").replaceWithText(result),"report write failed");
    require(signalError<=1e-6f,"summed signal differs from expected");
    require(roundtripError<=1e-6f,"save/reopen changes signal");
}
static void offlineTest(Session& original) {
    for(int count : {2,8,32}) for(int block : {128,256}) {
        auto dir=original.root.getChildFile("tracks-"+juce::String(count)+"-block-"+juce::String(block));
        require(dir.createDirectory().wasOk(),"case directory failed");
        Session test(dir);offlineCase(test,count,block);
    }
}
