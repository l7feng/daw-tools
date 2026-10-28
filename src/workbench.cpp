#include <JuceHeader.h>
#include <stdexcept>
namespace te = tracktion::engine;

class LocalStorage final : public te::PropertyStorage {
public:
    explicit LocalStorage(juce::File f) : PropertyStorage("StudioWorkbenchValidation"), root(f) {}
    juce::File getAppPrefsFolder() override { return root.getChildFile("prefs"); }
    juce::File getAppCacheFolder() override { return root.getChildFile("cache"); }
private: juce::File root;
};
class OfflineBehaviour final : public te::EngineBehaviour {
public:
    bool autoInitialiseDeviceManager() override { return false; }
    ClipDefaults getClipDefaults() override {
        auto defaults=te::EngineBehaviour::getClipDefaults();
        // The direct WaveNodeRealTime path compensates Lagrange base latency.
        // The pinned upstream proxy WaveNode path delays unity-rate impulses by 2 samples.
        defaults.useProxyFile=false;
        return defaults;
    }
};
class OfflineUI final : public te::UIBehaviour {
public:
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& task) override {
        while(task.runJob() != juce::ThreadPoolJob::jobHasFinished) {}
    }
};
static void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}
class Session {
public:
    juce::File root;
    te::Engine engine;
    std::unique_ptr<te::Edit> edit;
    explicit Session(juce::File f) : root(f), engine(std::make_unique<LocalStorage>(f),
        std::make_unique<OfflineUI>(), std::make_unique<OfflineBehaviour>()) {
        require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"audio device unexpectedly opened"); reset(); }
    void reset() {
        te::Edit::Options options {engine, te::createEmptyEdit(engine), te::ProjectItemID::createNewID(0)};
        options.role=te::Edit::forRendering;
        options.numAudioTracks=0;
        options.defaultMasterVolumedB=0;
        options.editFileRetriever=[this] { return root.getChildFile("session.tracktionedit"); };
        edit=std::make_unique<te::Edit>(options);
    }
    te::AudioTrack* addTrack() {
        auto t=edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit),nullptr,false);
        require(t!=nullptr,"cannot insert track");
        t->setName("Track " + juce::String(te::getAudioTracks(*edit).size()));
        return t.get();
    }
    void save() {
        require(te::EditFileOperations(*edit).save(true,true,false),"save failed");
    }
    void reopen() {
        auto f=root.getChildFile("session.tracktionedit");
        require(f.existsAsFile(),"no saved project");
        auto state=te::loadEditFromFile(engine,f,te::ProjectItemID::createNewID(0));
        require(state.isValid(),"invalid project");
        te::Edit::Options options{engine,state,te::ProjectItemID::createNewID(0)};
        options.role=te::Edit::forRendering; options.numAudioTracks=0;
        options.editFileRetriever=[f]{return f;};
        auto next=std::make_unique<te::Edit>(options); edit=std::move(next);
    }
    void render(const juce::File& output, int blockSize=128) {
        require(!output.exists(),"output already exists");
        juce::WavAudioFormat wav;
        te::Renderer::Parameters p(*edit);
        p.destFile=output; p.audioFormat=&wav; p.sampleRateForAudio=48000;
        p.bitDepth=32; p.blockSizeForAudio=blockSize; p.canRenderInMono=false; p.useMasterPlugins=false;
        p.time={tracktion::TimePosition::fromSeconds(0),tracktion::TimePosition::fromSeconds(1)};
        p.tracksToDo=te::toBitSet(te::getAllTracks(*edit));
        require(te::Renderer::renderToFile("Offline validation",p).existsAsFile(),"render failed");
    }
};
static juce::AudioBuffer<float> readWave(const juce::File& file) {
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
    require(reader!=nullptr,"wave decode failed");
    require(reader->sampleRate==48000 && reader->numChannels==2 && reader->lengthInSamples==48000,"unexpected render format/length");
    juce::AudioBuffer<float> b(2,48000);require(reader->read(&b,0,48000,0,true,true),"wave read failed");return b;
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
class MainPanel final : public juce::Component {
public:
    Session& session;
    juce::Label title,status;
    juce::TextButton add{juce::String::fromUTF8("添加空白轨道")},save{juce::String::fromUTF8("保存工程")},reopen{juce::String::fromUTF8("重新打开")};
    explicit MainPanel(Session& s):session(s) {
        title.setText(juce::String::fromUTF8("Studio Workbench · 原生底座 0.0.2"),juce::dontSendNotification);
        title.setFont(juce::FontOptions(24));
        for(auto* c:std::initializer_list<juce::Component*>{&title,&status,&add,&save,&reopen}) addAndMakeVisible(c);
        add.onClick=[this]{action([this]{session.addTrack();},"add-track");};
        save.onClick=[this]{action([this]{session.save();},"save");};
        reopen.onClick=[this]{action([this]{session.reopen();},"reopen");};
        update(juce::String::fromUTF8("空白工程已建立"));setSize(900,560);
    }
    void action(std::function<void()> fn,const juce::String& name) {
        try {fn();update(name+juce::String::fromUTF8(" 完成"));session.root.getChildFile("ui-events.log").appendText(name+" tracks="+juce::String(te::getAudioTracks(*session.edit).size())+"\n");}
        catch(const std::exception& e){update(juce::String(juce::String::fromUTF8("失败："))+e.what());}
    }
    void update(juce::String text) {status.setText(text+juce::String::fromUTF8("  |  离线验证模式：音频设备未打开"),juce::dontSendNotification);repaint();}
    void resized() override {title.setBounds(26,20,getWidth()-52,42); status.setBounds(26,66,getWidth()-52,32);add.setBounds(26,116,160,36);save.setBounds(200,116,160,36);reopen.setBounds(374,116,160,36);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171d26));g.setColour(juce::Colour(0xffdce5ef));
        auto tracks=te::getAudioTracks(*session.edit);
        if(tracks.isEmpty()) g.drawText(juce::String::fromUTF8("工程中没有轨道。由你决定轨道结构。"),26,208,getWidth()-52,30,juce::Justification::centredLeft);
        for(int i=0;i<tracks.size();++i) {g.setColour(juce::Colour(0xff263344));g.fillRoundedRectangle(26.f,190.f+i*50.f,float(getWidth()-52),40.f,5.f);g.setColour(juce::Colours::white);g.drawText(tracks[i]->getName(),40,190+i*50,300,40,juce::Justification::centredLeft);}
    }
};
class Window final:public juce::DocumentWindow {
public: Window(Session& s):DocumentWindow("Studio Workbench 0.0.2",juce::Colour(0xff171d26),allButtons) {
    setUsingNativeTitleBar(true);setContentOwned(new MainPanel(s),true);centreWithSize(900,560);setVisible(true);
} void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}
};
class App final:public juce::JUCEApplication {
public:
    std::unique_ptr<Session> session;std::unique_ptr<Window> window;
    const juce::String getApplicationName() override{return "StudioWorkbenchValidation";}
    const juce::String getApplicationVersion() override{return "0.0.2";}
    void initialise(const juce::String& line) override {
        auto args=juce::StringArray::fromTokens(line,false);int index=args.indexOf("--workspace");
        if(index<0 || index+1>=args.size() || !juce::File::isAbsolutePath(args[index+1])) {setApplicationReturnValue(2);quit();return;}
        juce::File root(args[index+1]);
        if(!root.isAChildOf(juce::File(STUDIO_SOURCE_ROOT).getChildFile("build")) || !root.getChildFile("studio-validation.marker").existsAsFile()) {setApplicationReturnValue(2);quit();return;}
        try {
            session=std::make_unique<Session>(root);
            if(args.contains("--self-test")) {offlineTest(*session);root.getChildFile("PASS.txt").replaceWithText("Offline signal and persistence passed; no audio devices opened.");quit();}
            else window=std::make_unique<Window>(*session);
        } catch(const std::exception& e) {root.getChildFile("FAIL.txt").replaceWithText(e.what());setApplicationReturnValue(1);quit();}
    }
    void shutdown() override {window.reset();session.reset();}
};
START_JUCE_APPLICATION(App)
