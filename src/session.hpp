#pragma once
#include <JuceHeader.h>
#include <stdexcept>
#include <filesystem>
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
    juce::File progressFile;
public:
    explicit OfflineUI(juce::File root):progressFile(root.getChildFile("render-progress.txt")){}
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& task) override {
        float last=-1;
        while(task.runJob() != juce::ThreadPoolJob::jobHasFinished) {float now=task.getCurrentTaskProgress();if(now-last>=0.02f){progressFile.replaceWithText(juce::String(now,3));last=now;}}
        progressFile.replaceWithText("1");
    }
};
static void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}
class Session {
public:
    juce::File root;
    te::Engine engine;
    juce::File projectFile;
    std::unique_ptr<te::Edit> edit;
    explicit Session(juce::File f) : root(f), engine(std::make_unique<LocalStorage>(f),
        std::make_unique<OfflineUI>(f), std::make_unique<OfflineBehaviour>()) {
        require(engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"audio device unexpectedly opened"); projectFile=root.getChildFile("session.tracktionedit"); reset(); }
    void reset() {
        te::Edit::Options options {engine, te::createEmptyEdit(engine), te::ProjectItemID::createNewID(0)};
        options.role=te::Edit::forRendering;
        options.numAudioTracks=0;
        options.defaultMasterVolumedB=0;
        options.editFileRetriever=[this] { return projectFile; };
        edit=std::make_unique<te::Edit>(options);
        edit->resetChangedStatus();
    }
    te::AudioTrack* addTrack() {
        auto t=edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit),nullptr,false);
        require(t!=nullptr,"cannot insert track");
        t->pluginList.addDefaultTrackPlugins(false);
        t->getVolumePlugin()->setVolumeDb(0);t->getVolumePlugin()->setPanLaw(te::PanLawLinear);
        t->setName("Track " + juce::String(te::getAudioTracks(*edit).size()));
        return t.get();
    }
    void writeProject(const juce::File& target,bool allowReplace,bool updateCurrent=true) {
        require(!target.isDirectory()&&target.getParentDirectory().isDirectory(),"Invalid project destination");
        require(allowReplace||!target.exists(),"Project destination exists");
        auto staging=target.getSiblingFile(target.getFileName()+"."+juce::Uuid().toString()+".partial");
        edit->state.setProperty("studioVersion","0.1.0",nullptr);
        edit->state.setProperty("studioProjectID",edit->getProjectItemID().toString(),nullptr);
        for(auto* track:te::getAudioTracks(*edit))if(track->isFrozen(te::Track::individualFreeze)){
            auto source=te::TemporaryFileManager::getFreezeFileForTrack(*track);require(source.existsAsFile(),"Frozen cache missing before save");auto hash=juce::SHA256(source).toHexString();
            auto assets=target.getSiblingFile(target.getFileName()+".assets").getChildFile("freeze");require(assets.createDirectory().wasOk(),"Cannot create freeze assets");auto asset=assets.getChildFile(hash+".freeze");
            if(!asset.exists())require(source.copyFileTo(asset),"Freeze asset copy failed");require(juce::SHA256(asset).toHexString()==hash,"Freeze asset hash mismatch");
            track->state.setProperty("studioFreezeAsset",asset.getRelativePathFrom(target.getParentDirectory()),nullptr);track->state.setProperty("studioFreezeHash",hash,nullptr);track->state.setProperty("studioFreezeName",source.getFileName(),nullptr);
        }
        require(te::EditFileOperations(*edit).writeToFile(staging,false),"Project staging write failed");
        auto xml=juce::XmlDocument::parse(staging);require(xml&&xml->hasTagName("EDIT"),"Staged project validation failed");
        if(allowReplace&&target.existsAsFile())require(staging.replaceFileIn(target),"Project replacement failed; previous save retained");
        else std::filesystem::rename(std::filesystem::path(staging.getFullPathName().toWideCharPointer()),std::filesystem::path(target.getFullPathName().toWideCharPointer()));
        if(updateCurrent){projectFile=target;edit->editFileRetriever=[this]{return projectFile;};edit->resetChangedStatus();}
    }
    void save() {writeProject(projectFile,true);}
    void openProject(const juce::File& f) {
        juce::XmlDocument document(f);auto xml=document.getDocumentElement();
        require(document.getLastParseError().isEmpty() && xml && xml->hasTagName("EDIT"),"Invalid project: complete EDIT document required");
        auto text=f.loadFileAsString().trimEnd();
        require(text.endsWith("</EDIT>") || (xml->getFirstChildElement()==nullptr && text.endsWith("/>")),"Invalid project: unterminated EDIT document");
        auto state=juce::ValueTree::fromXml(*xml);
        auto id=te::ProjectItemID(state.getProperty("studioProjectID").toString());if(!id.isValid())id=te::ProjectItemID::createNewID(0);
        std::function<void(juce::ValueTree)> restoreFreeze=[&](juce::ValueTree node){
            if(bool(node.getProperty("frozenIndividually"))&&node.hasProperty("studioFreezeAsset")){
                auto asset=f.getParentDirectory().getChildFile(node.getProperty("studioFreezeAsset").toString());auto hash=node.getProperty("studioFreezeHash").toString();auto name=node.getProperty("studioFreezeName").toString();
                require(asset.isAChildOf(f.getParentDirectory())&&asset.existsAsFile()&&juce::SHA256(asset).toHexString()==hash,"Missing or corrupt freeze asset");require(name.startsWith("trackFreeze_0_")&&!name.containsChar('/')&&!name.containsChar('\\'),"Invalid freeze cache name");
                auto cache=engine.getTemporaryFileManager().getTempDirectory().getChildFile("edit_"+id.toStringSuitableForFilename());require(cache.createDirectory().wasOk(),"Cannot restore freeze cache");auto dest=cache.getChildFile(name);
                if(!dest.existsAsFile()||juce::SHA256(dest).toHexString()!=hash)require(asset.copyFileTo(dest),"Freeze cache restore failed");
            }
            for(auto child:node)restoreFreeze(child);
        };restoreFreeze(state);
        te::Edit::Options options{engine,state,id};
        options.role=te::Edit::forRendering; options.numAudioTracks=0;
        options.editFileRetriever=[f]{return f;};
        auto next=std::make_unique<te::Edit>(options);
        projectFile=f;edit=std::move(next);
    }
    void reopen() {openProject(projectFile);}
    void saveAs(const juce::File& f) {
        writeProject(f,false);
    }
    static auto time(int64_t samples) {return tracktion::TimePosition::fromSamples(samples,48000.0);}
    static auto duration(int64_t samples) {return tracktion::TimeDuration::fromSamples(samples,48000.0);}
    static int64_t samples(tracktion::TimePosition t) {return std::llround(t.inSeconds()*48000);}
    juce::Array<te::Clip*> clips() const {
        juce::Array<te::Clip*> result;
        for(auto* t:te::getAudioTracks(*edit))result.addArray(t->getClips());
        return result;
    }
    bool contains(te::Clip& clip) const {return clips().contains(&clip);}
    void begin(const char* label) {edit->getUndoManager().beginNewTransaction(label);}
    te::WaveAudioClip* importAudio(const juce::File& file,int track,int64_t at) {
        require(at>=0,"Negative import position");
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        require(reader && reader->lengthInSamples>0 && reader->sampleRate>0,"Missing or invalid audio file");
        require(reader->numChannels==1 || reader->numChannels==2,"Only mono/stereo supported");
        const auto length=std::llround(double(reader->lengthInSamples)*48000.0/reader->sampleRate);
        auto tracks=te::getAudioTracks(*edit);
        require(track>=0 && track<=tracks.size(),"Invalid destination track");
        begin("Import audio");
        auto* target=track==tracks.size()?addTrack():tracks[track];
        auto clip=target->insertWaveClip(file.getFileNameWithoutExtension(),file,{{time(at),time(at+length)},duration(0)},false);
        require(clip!=nullptr,"Audio import failed");
        clip->setAutoTempo(false);clip->setAutoPitch(false);
        clip->state.setProperty("studioSourceSHA256",juce::SHA256(file).toHexString(),&edit->getUndoManager());
        return clip.get();
    }
    void move(te::Clip& clip,int64_t at) {
        require(contains(clip) && at>=0,"Invalid clip or start");begin("Move clip");clip.setStart(time(at),false,true);
    }
    void trim(te::Clip& clip,int64_t start,int64_t end) {
        require(contains(clip) && start>=samples(clip.getPosition().getStart()) && end<=samples(clip.getPosition().getEnd()) && start<end,"Trim must stay inside clip");
        begin("Trim clip");clip.setStart(time(start),true,false);clip.setEnd(time(end),true);
    }
    te::Clip* split(te::Clip& clip,int64_t at) {
        require(contains(clip) && at>samples(clip.getPosition().getStart()) && at<samples(clip.getPosition().getEnd()),"Split must be inside clip");
        auto* track=dynamic_cast<te::ClipTrack*>(clip.getTrack());require(track!=nullptr,"Invalid track");
        begin("Split clip");auto* next=track->splitClip(clip,time(at));require(next!=nullptr,"Split failed");return next;
    }
    te::Clip* duplicate(te::Clip& clip,int64_t at) {
        require(contains(clip) && at>=0,"Invalid copy position");
        auto* track=dynamic_cast<te::ClipTrack*>(clip.getTrack());require(track!=nullptr,"Invalid track");
        auto position=clip.getPosition();position.time={time(at),time(at)+position.getLength()};
        begin("Duplicate clip");auto* next=track->insertClipWithState(clip.state.createCopy(),clip.getName(),clip.type,position,false,false);
        require(next!=nullptr && next->itemID!=clip.itemID,"Copy identity failure");return next;
    }
    void erase(te::Clip& clip) {require(contains(clip),"Invalid clip");begin("Remove clip");clip.removeFromParent();}
    void setEnvelope(te::WaveAudioClip& clip,float db,int64_t fadeIn,int64_t fadeOut) {
        const auto length=samples(clip.getPosition().getEnd())-samples(clip.getPosition().getStart());
        require(contains(clip) && std::isfinite(db) && db>=-120 && db<=24 && fadeIn>=0 && fadeOut>=0 && fadeIn+fadeOut<=length,"Invalid gain/fades");
        begin("Clip envelope");clip.setGainDB(db);clip.setFadeIn(duration(fadeIn));clip.setFadeOut(duration(fadeOut));
    }
    void crossfade(te::WaveAudioClip& left,te::WaveAudioClip& right) {
        auto a=left.getPosition(),b=right.getPosition();
        require(contains(left)&&contains(right)&&left.getTrack()==right.getTrack()&&a.getStart()<b.getStart()&&a.getEnd()>b.getStart()&&a.getEnd()<b.getEnd(),"Crossfade requires two partially overlapping clips on the same track");
        begin("Crossfade");auto length=a.getEnd()-b.getStart();
        left.setFadeOutType(te::AudioFadeCurve::linear);right.setFadeInType(te::AudioFadeCurve::linear);
        left.setFadeOut(length);right.setFadeIn(length);
    }
    juce::StringArray missingMedia() const {
        juce::StringArray result;
        for(auto* clip:clips())if(auto* audio=dynamic_cast<te::WaveAudioClip*>(clip))if(!audio->getCurrentSourceFile().existsAsFile())result.add(audio->getCurrentSourceFile().getFullPathName());
        return result;
    }
    void relink(te::WaveAudioClip& clip,const juce::File& replacement) {
        require(contains(clip)&&replacement.existsAsFile(),"Invalid replacement file");
        auto expected=clip.state.getProperty("studioSourceSHA256").toString();
        require(expected.isNotEmpty()&&juce::SHA256(replacement).toHexString()==expected,"Replacement content does not match the original media identity");
        begin("Relink media");clip.getSourceFileReference().setToDirectFileReference(replacement,false);
    }
    static void stripTemplateMedia(juce::ValueTree state) {
        for(auto key:{"studioProjectID","studioFreezeAsset","studioFreezeHash","studioFreezeName"})state.removeProperty(key,nullptr);
        for(auto key:{"frozenIndividually","frozen"})if(state.hasProperty(key))state.setProperty(key,false,nullptr);
        for(int i=state.getNumChildren();--i>=0;){auto child=state.getChild(i);if(child.getType().toString().endsWith("CLIP")||child.getProperty("type").toString()=="freezePoint")state.removeChild(i,nullptr);else stripTemplateMedia(child);}
    }
    void saveTemplate(const juce::File& target) {
        require(!target.exists(),"Template target exists");edit->flushState();auto state=edit->state.createCopy();stripTemplateMedia(state);
        state.setProperty("studioTemplate",1,nullptr);state.removeProperty("projectID",nullptr);state.removeProperty("modifiedBy",nullptr);
        require(state.createXml()->writeTo(target),"Template write failed");
    }
    void loadTemplate(const juce::File& file) {
        auto xml=juce::XmlDocument::parse(file);require(xml&&xml->hasTagName("EDIT")&&xml->getIntAttribute("studioTemplate")==1,"Invalid user template");
        auto state=juce::ValueTree::fromXml(*xml);stripTemplateMedia(state);state.removeProperty("studioTemplate",nullptr);
        auto target=root.getNonexistentChildFile("untitled",".tracktionedit",false);
        auto id=te::ProjectItemID::createNewID(0);
        te::Edit::Options options{engine,state,id};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[target]{return target;};
        auto next=std::make_unique<te::Edit>(options);projectFile=target;edit=std::move(next);
    }
    juce::File checkpoint() {
        auto folder=root.getChildFile("recovery");require(folder.createDirectory().wasOk(),"Recovery directory failed");
        auto target=folder.getNonexistentChildFile("snapshot",".tracktionedit",false);
        writeProject(target,false,false);return target;
    }
    te::AudioTrack& trackAt(int index) const {
        auto tracks=te::getAudioTracks(*edit);
        require(index>=0 && index<tracks.size(),"Invalid track index");return *tracks[index];
    }
    void setMix(int index,float db,float pan,bool mute,bool solo) {
        require(std::isfinite(db)&&db>=-120&&db<=24&&std::isfinite(pan)&&pan>=-1&&pan<=1,"Invalid mixer values");
        auto& track=trackAt(index);auto* volume=track.getVolumePlugin();require(volume!=nullptr,"Track fader unavailable");
        begin("Track mix");volume->setPanLaw(te::PanLawLinear);volume->setVolumeDb(db);volume->setPan(pan);track.setMute(mute);track.setSolo(solo);
    }
    void setMaster(float db) {
        require(std::isfinite(db)&&db>=-120&&db<=24,"Invalid master volume");begin("Master volume");edit->getMasterVolumePlugin()->setVolumeDb(db);
    }
    bool reaches(te::AudioTrack& from,te::AudioTrack& target,juce::Array<te::AudioTrack*>& visited) const {
        if(&from==&target)return true;if(visited.contains(&from))return false;visited.add(&from);
        if(auto* next=from.getOutput().getDestinationTrack();next&&reaches(*next,target,visited))return true;
        for(auto* send:from.pluginList.getPluginsOfType<te::AuxSendPlugin>())
            for(auto* other:te::getAudioTracks(*edit))
                for(auto* ret:other->pluginList.getPluginsOfType<te::AuxReturnPlugin>())
                    if(ret->busNumber.get()==send->busNumber.get()&&reaches(*other,target,visited))return true;
        return false;
    }
    void validateEdge(te::AudioTrack& from,te::AudioTrack& to) const {
        juce::Array<te::AudioTrack*> visited;require(!reaches(to,from,visited),"Feedback routing is not supported");
    }
    void route(int from,int to) {
        require(to>=-1,"Invalid output index");auto& source=trackAt(from);auto* target=to<0?nullptr:&trackAt(to);
        if(target)validateEdge(source,*target);begin("Track output");
        if(target)source.getOutput().setOutputToTrack(target);else source.getOutput().setOutputToDefaultDevice(false);
    }
    int addBus() {
        begin("Add bus");auto* track=addTrack();track->state.setProperty("studioBus",true,&edit->getUndoManager());
        int index=te::getAudioTracks(*edit).indexOf(track);track->setName("Bus "+juce::String(index+1));return index;
    }
    void addSend(int from,int to,float db,bool preFader) {
        require(std::isfinite(db)&&db>=-120&&db<=12,"Invalid send gain");auto& source=trackAt(from);auto& target=trackAt(to);validateEdge(source,target);
        require(bool(target.state.getProperty("studioBus")),"Send destination must be a bus");
        auto* ret=target.pluginList.findFirstPluginOfType<te::AuxReturnPlugin>();
        int bus=-1;
        if(ret)bus=ret->busNumber;
        else {juce::Array<int> used;for(auto* t:te::getAudioTracks(*edit)){for(auto* r:t->pluginList.getPluginsOfType<te::AuxReturnPlugin>())used.addIfNotAlreadyThere(r->busNumber);for(auto* a:t->pluginList.getPluginsOfType<te::AuxSendPlugin>())used.addIfNotAlreadyThere(a->busNumber);}for(int i=0;i<32;++i)if(!used.contains(i)){bus=i;break;}require(bus>=0,"No available auxiliary bus");}
        for(auto* existing:source.pluginList.getPluginsOfType<te::AuxSendPlugin>())require(existing->busNumber.get()!=bus,"A send to this bus already exists");
        auto sendPlugin=edit->getPluginCache().createNewPlugin(te::AuxSendPlugin::xmlTypeName,{});
        auto* send=dynamic_cast<te::AuxSendPlugin*>(sendPlugin.get());require(send!=nullptr,"Send creation failed");
        begin("Add send");
        if(!ret){auto returnPlugin=edit->getPluginCache().createNewPlugin(te::AuxReturnPlugin::xmlTypeName,{});ret=dynamic_cast<te::AuxReturnPlugin*>(returnPlugin.get());require(ret!=nullptr,"Return creation failed");ret->busNumber=bus;target.pluginList.insertPlugin(returnPlugin,0,nullptr);}
        send->busNumber=bus;send->setGainDb(db);
        auto index=source.pluginList.indexOf(source.getVolumePlugin());require(index>=0,"Fader missing");source.pluginList.insertPlugin(sendPlugin,index+(preFader?0:1),nullptr);
    }
    void setTrackFrozen(int index,bool frozen) {
        auto& track=trackAt(index);if(track.isFrozen(te::Track::individualFreeze)==frozen)return;
        if(frozen){require(track.getOutput().getDestinationTrack()==nullptr,"Freeze the destination bus instead");
            require(track.pluginList.getPluginsOfType<te::AuxSendPlugin>().isEmpty(),"Freeze with sends is not supported yet");
            require(std::abs(engine.getDeviceManager().getSampleRate()-48000)<0.01&&engine.getDeviceManager().getBlockSize()>0,"Configure the engine clock before freezing");
            for(auto* plugin:track.pluginList.getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot freeze unavailable plugin");}
        }
        track.setFrozen(frozen,te::Track::individualFreeze);
        auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
            if(track.isFrozen(te::Track::individualFreeze)==frozen&&(!frozen||te::TemporaryFileManager::getFreezeFileForTrack(track).existsAsFile()))return;}
        throw std::runtime_error("Freeze/unfreeze did not complete");
    }
    void undo(){edit->undo();edit->getUndoManager().beginNewTransaction();}
    void redo(){edit->redo();edit->getUndoManager().beginNewTransaction();}
    void render(const juce::File& output, int blockSize=128, int64_t endSample=48000, int bitDepth=32, int64_t startSample=0, int selectedTrack=-1, bool includeMaster=true) {
        require(!output.exists(),"output already exists");
        for(auto* track:te::getAudioTracks(*edit))for(auto* plugin:track->pluginList.getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot export: unavailable external plugin");}
        for(auto* plugin:edit->getMasterPluginList().getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot export: unavailable master plugin");}
        std::unique_ptr<te::Edit> stemEdit;
        if(selectedTrack>=0){auto& target=trackAt(selectedTrack);juce::Array<int> included;auto originals=te::getAudioTracks(*edit);
            for(int i=0;i<originals.size();++i){juce::Array<te::AudioTrack*> forward,backward;if(reaches(target,*originals[i],forward)||reaches(*originals[i],target,backward))included.add(i);}
            te::Edit::Options options{engine,edit->state.createCopy(),te::ProjectItemID::createNewID(0)};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[this]{return projectFile;};stemEdit=std::make_unique<te::Edit>(options);
            auto copies=te::getAudioTracks(*stemEdit);for(int i=0;i<copies.size();++i)if(!included.contains(i)){copies[i]->setMute(true);copies[i]->setSolo(false);}
            if(bool(target.state.getProperty("studioBus")))for(int i=0;i<copies.size();++i)if(i!=selectedTrack&&copies[i]->getOutput().getDestinationTrack()==nullptr){
                juce::Array<te::AudioTrack*> downstream;if(reaches(target,*originals[i],downstream))continue;
                auto attenuation=stemEdit->getPluginCache().createNewPlugin(te::VolumeAndPanPlugin::xmlTypeName,{});auto* volume=dynamic_cast<te::VolumeAndPanPlugin*>(attenuation.get());require(volume!=nullptr,"Bus stem output gate failed");volume->setVolumeDb(-1000);copies[i]->pluginList.insertPlugin(attenuation,-1,nullptr);
            }
        }
        auto& renderingEdit=stemEdit?*stemEdit:*edit;
        juce::WavAudioFormat wav;
        te::Renderer::Parameters p(renderingEdit);
        p.destFile=output; p.audioFormat=&wav; p.sampleRateForAudio=48000;
        require(startSample>=0 && endSample>startSample && (bitDepth==24 || bitDepth==32),"Invalid export range/format");
        p.bitDepth=bitDepth; p.blockSizeForAudio=blockSize; p.canRenderInMono=false; p.useMasterPlugins=includeMaster;
        p.time={time(startSample),time(endSample)};
        p.tracksToDo=te::toBitSet(te::getAllTracks(renderingEdit));
        require(selectedTrack>=-1,"Invalid stem track");
        require(te::Renderer::renderToFile("Offline validation",p).existsAsFile(),"render failed");
    }
};
