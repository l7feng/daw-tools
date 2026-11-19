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
static juce::File pluginBundle(const juce::String& path){
    if(!juce::File::isAbsolutePath(path))return {};
    juce::File file(path);for(auto parent=file.getParentDirectory();parent!=parent.getParentDirectory();parent=parent.getParentDirectory())if(parent.hasFileExtension("vst3"))file=parent;return file;
}
class OfflineBehaviour final : public te::EngineBehaviour {
public:
    bool autoInitialiseDeviceManager() override { return false; }
    bool shouldLoadPlugin(te::ExternalPlugin& plugin) override {
        // The upstream name fallback must never silently substitute another binary.
        auto requested=pluginBundle(plugin.state.getProperty("filename").toString());
        auto reject=[&]{plugin.desc.fileOrIdentifier=plugin.state.getProperty("filename").toString();plugin.desc.name=plugin.state.getProperty("name").toString();plugin.desc.manufacturerName=plugin.state.getProperty("manufacturer").toString();plugin.desc.uniqueId=plugin.state.getProperty("uniqueId").toString().getHexValue32();plugin.desc.deprecatedUid=plugin.state.getProperty("uid").toString().getHexValue32();return false;};
        if(!requested.exists()||requested!=pluginBundle(plugin.desc.fileOrIdentifier))return reject();
        if(plugin.state.getProperty("uniqueId").toString()!=juce::String::toHexString(plugin.desc.uniqueId)
            ||plugin.state.getProperty("uid").toString()!=juce::String::toHexString(plugin.desc.deprecatedUid))return reject();
        return te::EngineBehaviour::shouldLoadPlugin(plugin);
    }
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
    std::function<bool(float)> cancelTask;
    bool taskCancelled=false;
    explicit OfflineUI(juce::File root):progressFile(root.getChildFile("render-progress.txt")){}
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& task) override {
        float last=-1;
        for(;;) {
            if(cancelTask&&cancelTask(task.getCurrentTaskProgress())){taskCancelled=true;task.signalJobShouldExit();}
            if(task.runJob()==juce::ThreadPoolJob::jobHasFinished)break;
            float now=task.getCurrentTaskProgress();if(now-last>=0.02f){progressFile.replaceWithText(juce::String(now,3));last=now;}}
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
    static bool descriptionMatches(const juce::PluginDescription& desc,juce::ValueTree node){
        return node.getProperty("type").toString()=="vst"&&desc.pluginFormatName=="VST3"
            &&juce::File::isAbsolutePath(node.getProperty("filename").toString())
            &&pluginBundle(desc.fileOrIdentifier)==pluginBundle(node.getProperty("filename").toString())
            &&node.getProperty("uniqueId").toString()==juce::String::toHexString(desc.uniqueId)
            &&node.getProperty("uid").toString()==juce::String::toHexString(desc.deprecatedUid)
            &&node.getProperty("manufacturer").toString()==desc.manufacturerName;
    }
    static juce::Array<juce::ValueTree> referencedPluginStates(juce::ValueTree state){
        juce::Array<juce::ValueTree> result;
        std::function<void(juce::ValueTree)> visit=[&](juce::ValueTree node){if(node.getType().toString()=="STUDIOPLUGINCATALOG")return;if(node.getProperty("type").toString()=="vst")result.add(node);for(auto child:node)visit(child);};visit(state);return result;
    }
    void updatePluginCatalog(){
        auto previous=edit->state.getChildWithName("STUDIOPLUGINCATALOG");juce::ValueTree catalog("STUDIOPLUGINCATALOG");catalog.setProperty("version",1,nullptr);juce::StringArray identities;
        auto known=engine.getPluginManager().knownPluginList.getTypes();auto references=referencedPluginStates(edit->state);
        for(auto node:references){
            std::unique_ptr<juce::PluginDescription> description;
            for(auto type:known)if(descriptionMatches(type,node)){description=std::make_unique<juce::PluginDescription>(type);break;}
            if(!description)for(auto child:previous){auto candidate=std::make_unique<juce::PluginDescription>();auto xml=child.createXml();if(xml&&candidate->loadFromXml(*xml)&&descriptionMatches(*candidate,node)){description=std::move(candidate);break;}}
            if(!description)continue; // Legacy/missing instances retain raw state without guessing metadata.
            description->fileOrIdentifier=pluginBundle(description->fileOrIdentifier).getFullPathName();auto key=description->fileOrIdentifier+":"+juce::String::toHexString(description->uniqueId)+":"+juce::String::toHexString(description->deprecatedUid);
            if(identities.contains(key))continue;identities.add(key);catalog.addChild(juce::ValueTree::fromXml(*description->createXml()),-1,nullptr);
        }
        if(previous.isValid()&&previous.isEquivalentTo(catalog))return;
        if(previous.isValid())edit->state.removeChild(previous,nullptr);
        if(!references.isEmpty())edit->state.addChild(catalog,-1,nullptr);
    }
    void registerLegacyProjectPlugins(juce::ValueTree state,int timeoutMs){
        require(timeoutMs>0&&timeoutMs<=300000,"Invalid project plugin scan timeout");auto references=referencedPluginStates(state);juce::Array<juce::File> scanned;auto& known=engine.getPluginManager().knownPluginList;
        for(auto node:references){bool registered=false;for(auto type:known.getTypes())if(descriptionMatches(type,node)){registered=true;break;}if(registered)continue;
            auto path=pluginBundle(node.getProperty("filename").toString());if(!path.exists()||!path.hasFileExtension("vst3")||scanned.contains(path))continue;scanned.add(path);
            auto folder=root.getChildFile("legacy-scan-"+juce::Uuid().toString());require(folder.createDirectory().wasOk(),"Cannot create project scan directory");require(folder.getChildFile("studio-validation.marker").replaceWithText("referenced legacy project plugin scan"),"Cannot mark project scan");auto* request=new juce::DynamicObject;request->setProperty("path",path.getFullPathName());require(folder.getChildFile("scan-request.json").replaceWithText(juce::JSON::toString(juce::var(request))),"Cannot write project scan request");
            auto record=[&](const juce::String& status,int code){auto* result=new juce::DynamicObject;result->setProperty("status",status);result->setProperty("exit_code",code);result->setProperty("path",path.getFullPathName());folder.getChildFile("scan-status.json").replaceWithText(juce::JSON::toString(juce::var(result)));};
            juce::ChildProcess process;if(!process.start(juce::StringArray{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--workspace",folder.getFullPathName(),"--scan-plugin"},0)){record("failed",-1);continue;}
            auto deadline=juce::Time::getMillisecondCounterHiRes()+timeoutMs;while(process.isRunning()&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(5);
            if(process.isRunning()){require(process.kill()||!process.isRunning(),"Cannot stop project plugin scanner");require(process.waitForProcessToFinish(2000),"Project plugin scanner did not exit");record("timed_out",int(process.getExitCode()));continue;}
            if(process.getExitCode()!=0){record("failed",int(process.getExitCode()));continue;}
            auto xml=juce::XmlDocument::parse(folder.getChildFile("scan-result.xml"));if(!xml||!xml->hasTagName("PLUGIN_SCAN")){record("invalid_result",0);continue;}
            int matched=0;for(auto* entry:xml->getChildIterator()){juce::PluginDescription description;if(!description.loadFromXml(*entry))continue;for(auto reference:references)if(descriptionMatches(description,reference)){known.addType(description);++matched;break;}}
            record(matched?"complete":"identity_mismatch",0);
        }
    }
    void registerProjectPlugins(juce::ValueTree state,int timeoutMs){
        auto catalog=state.getChildWithName("STUDIOPLUGINCATALOG");if(!catalog.isValid()){registerLegacyProjectPlugins(state,timeoutMs);return;}
        int count=0;for(auto child:state)if(child.getType().toString()=="STUDIOPLUGINCATALOG")++count;
        require(count==1&&int(catalog.getProperty("version"))==1,"Invalid project plugin catalog version");
        auto references=referencedPluginStates(state);juce::OwnedArray<juce::PluginDescription> selected;juce::StringArray identities;
        for(auto child:catalog){auto description=std::make_unique<juce::PluginDescription>();auto xml=child.createXml();require(xml&&description->loadFromXml(*xml)&&description->pluginFormatName=="VST3"&&juce::File::isAbsolutePath(description->fileOrIdentifier),"Invalid saved VST3 description");
            bool referenced=false;for(auto node:references)if(descriptionMatches(*description,node)){referenced=true;break;}if(!referenced)continue;
            description->fileOrIdentifier=pluginBundle(description->fileOrIdentifier).getFullPathName();auto key=description->fileOrIdentifier+":"+juce::String::toHexString(description->uniqueId)+":"+juce::String::toHexString(description->deprecatedUid);int duplicate=identities.indexOf(key);
            if(duplicate>=0){require(selected[duplicate]->createXml()->toString()==description->createXml()->toString(),"Conflicting saved plugin descriptions");continue;}
            identities.add(key);selected.add(description.release());
        }
        // Parse and validate all entries before mutating the engine-local catalog.
        auto& known=engine.getPluginManager().knownPluginList;
        for(auto* description:selected){bool registered=false;for(auto type:known.getTypes())for(auto node:references)if(descriptionMatches(*description,node)&&descriptionMatches(type,node)){registered=true;break;}if(!registered)known.addType(*description);}
    }
    std::unique_ptr<te::Edit> createProjectEdit(juce::ValueTree state,te::ProjectItemID id,const juce::File& file,int scanTimeoutMs=20000){
        engine.getPluginManager().initialise();auto& known=engine.getPluginManager().knownPluginList;auto before=known.createXml();
        try{registerProjectPlugins(state,scanTimeoutMs);te::Edit::Options options{engine,state,id};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[file]{return file;};auto next=std::make_unique<te::Edit>(options);
            for(auto node:referencedPluginStates(next->state)){auto item=next->getPluginCache().getOrCreatePluginFor(node);if(auto* plugin=dynamic_cast<te::ExternalPlugin*>(item.get())){plugin->initialiseFully();if(auto* instance=plugin->getAudioPluginInstance()){juce::PluginDescription actual;instance->fillInPluginDescription(actual);require(descriptionMatches(actual,node)&&actual.isInstrument==plugin->desc.isInstrument,"Saved plugin description differs from runtime identity");}}}
            validateFrozenEdit(*next);return next;}
        catch(...){if(before)known.recreateFromXml(*before);throw;}
    }
    void writeProject(const juce::File& target,bool allowReplace,bool updateCurrent=true,std::function<bool()> cancelBeforeCommit={}) {
        require(!target.isDirectory()&&target.getParentDirectory().isDirectory(),"Invalid project destination");
        require(allowReplace||!target.exists(),"Project destination exists");
        auto staging=target.getSiblingFile(target.getFileName()+"."+juce::Uuid().toString()+".partial");
        validateFrozenTracks();updatePluginCatalog();
        edit->state.setProperty("studioVersion","0.1.7",nullptr);
        edit->state.setProperty("studioProjectID",edit->getProjectItemID().toString(),nullptr);
        for(auto* track:te::getAudioTracks(*edit))if(track->isFrozen(te::Track::individualFreeze)){
            auto source=te::TemporaryFileManager::getFreezeFileForTrack(*track);require(source.existsAsFile(),"Frozen cache missing before save");auto hash=juce::SHA256(source).toHexString();
            auto assets=target.getSiblingFile(target.getFileName()+".assets").getChildFile("freeze");require(assets.createDirectory().wasOk(),"Cannot create freeze assets");auto asset=assets.getChildFile(hash+".freeze");
            if(!asset.exists())require(source.copyFileTo(asset),"Freeze asset copy failed");require(juce::SHA256(asset).toHexString()==hash,"Freeze asset hash mismatch");
            track->state.setProperty("studioFreezeAsset",asset.getRelativePathFrom(target.getParentDirectory()),nullptr);track->state.setProperty("studioFreezeHash",hash,nullptr);track->state.setProperty("studioFreezeName",source.getFileName(),nullptr);
        }
        require(te::EditFileOperations(*edit).writeToFile(staging,false),"Project staging write failed");
        auto xml=juce::XmlDocument::parse(staging);require(xml&&xml->hasTagName("EDIT"),"Staged project validation failed");
        require(!cancelBeforeCommit||!cancelBeforeCommit(),"Project save cancelled before commit; previous save retained");
        if(allowReplace&&target.existsAsFile())require(staging.replaceFileIn(target),"Project replacement failed; previous save retained");
        else std::filesystem::rename(std::filesystem::path(staging.getFullPathName().toWideCharPointer()),std::filesystem::path(target.getFullPathName().toWideCharPointer()));
        if(updateCurrent){projectFile=target;edit->editFileRetriever=[this]{return projectFile;};edit->resetChangedStatus();}
    }
    void save() {writeProject(projectFile,true);}
    void openProject(const juce::File& f,int scanTimeoutMs=20000) {
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
        auto next=createProjectEdit(state,id,f,scanTimeoutMs);projectFile=f;edit=std::move(next);
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
        for(auto key:{"studioProjectID","studioFreezeAsset","studioFreezeHash","studioFreezeName","studioFreezeIdentity"})state.removeProperty(key,nullptr);
        for(auto key:{"frozenIndividually","frozen"})if(state.hasProperty(key))state.setProperty(key,false,nullptr);
        for(int i=state.getNumChildren();--i>=0;){auto child=state.getChild(i);if(child.getType().toString().endsWith("CLIP")||child.getProperty("type").toString()=="freezePoint")state.removeChild(i,nullptr);else stripTemplateMedia(child);}
    }
    void saveTemplate(const juce::File& target) {
        require(!target.exists(),"Template target exists");edit->flushState();updatePluginCatalog();auto state=edit->state.createCopy();stripTemplateMedia(state);
        state.setProperty("studioTemplate",1,nullptr);state.removeProperty("projectID",nullptr);state.removeProperty("modifiedBy",nullptr);
        require(state.createXml()->writeTo(target),"Template write failed");
    }
    void loadTemplate(const juce::File& file,int scanTimeoutMs=20000) {
        auto xml=juce::XmlDocument::parse(file);require(xml&&xml->hasTagName("EDIT")&&xml->getIntAttribute("studioTemplate")==1,"Invalid user template");
        auto state=juce::ValueTree::fromXml(*xml);stripTemplateMedia(state);state.removeProperty("studioTemplate",nullptr);
        auto target=root.getNonexistentChildFile("untitled",".tracktionedit",false);
        auto id=te::ProjectItemID::createNewID(0);
        auto next=createProjectEdit(state,id,target,scanTimeoutMs);projectFile=target;edit=std::move(next);
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
        target.setSoloIsolate(true); // Keep the selected source's return audible when soloing.
        send->busNumber=bus;send->setGainDb(db);
        auto index=source.pluginList.indexOf(source.getVolumePlugin());require(index>=0,"Fader missing");source.pluginList.insertPlugin(sendPlugin,index+(preFader?0:1),nullptr);
    }
    // Canonical state avoids differences in XML property insertion order after reopen.
    static juce::String freezeState(juce::ValueTree state) {
        juce::String result="<"+state.getType().toString()+">";juce::StringArray keys;
        for(int i=0;i<state.getNumProperties();++i){auto key=state.getPropertyName(i).toString();
            if(!key.startsWith("studioFreeze")&&key!="frozen"&&key!="frozenIndividually")keys.add(key);}
        keys.sort(false);for(auto key:keys){auto value=state.getProperty(key).toString();
            if(key=="filename"&&state.getProperty("type").toString()=="vst"&&juce::File::isAbsolutePath(value))value=pluginBundle(value).getFullPathName();
            result+=juce::String(key.length())+":"+key+juce::String(value.length())+":"+value;}
        for(auto child:state)if(child.getProperty("type").toString()!="freezePoint")result+=freezeState(child);
        return result+"</>";
    }
    static juce::String freezeIdentity(te::Edit& owner,te::AudioTrack& track) {
        juce::String text=freezeState(track.state);
        for(auto type:{"TEMPOSEQUENCE","PITCHSEQUENCE"})text+=freezeState(owner.state.getChildWithName(type));
        juce::Array<te::AudioTrack*> visited;
        std::function<void(te::AudioTrack&)> inputs=[&](te::AudioTrack& t){if(visited.contains(&t))return;visited.add(&t);
            if(&t!=&track)text+=freezeState(t.state);
            for(auto* plugin:t.pluginList.getPluginsOfType<te::ExternalPlugin>()){
                auto bundle=pluginBundle(plugin->state.getProperty("filename").toString());require(bundle.exists(),"Frozen plugin binary missing");juce::StringArray paths;
                if(bundle.isDirectory()){for(auto file:bundle.findChildFiles(juce::File::findFiles,true))paths.add(file.getFullPathName());}else paths.add(bundle.getFullPathName());paths.sort(false);
                for(auto path:paths)text+=path+juce::SHA256(juce::File(path)).toHexString();
            }
            for(auto* clip:t.getClips())if(auto* wave=dynamic_cast<te::WaveAudioClip*>(clip)){auto file=wave->getCurrentSourceFile();require(file.existsAsFile(),"Frozen source missing");text+=file.getFullPathName()+juce::SHA256(file).toHexString();}
            for(auto* input:t.getInputTracks())if(auto* audio=dynamic_cast<te::AudioTrack*>(input))inputs(*audio);
        };inputs(track);
        return juce::SHA256(text.toRawUTF8(),size_t(text.getNumBytesAsUTF8())).toHexString();
    }
    static void validateFrozenEdit(te::Edit& owner) {
        owner.flushState();
        for(auto* track:te::getAudioTracks(owner))if(track->isFrozen(te::Track::individualFreeze)){
            require(track->state.getProperty("studioFreezeIdentity").toString()==freezeIdentity(owner,*track),"Frozen inputs or parameters changed: unfreeze before saving or rendering");
            auto file=te::TemporaryFileManager::getFreezeFileForTrack(*track);require(file.existsAsFile(),"Frozen cache missing");
            auto hash=track->state.getProperty("studioFreezeHash").toString();
            if(hash.isNotEmpty())require(juce::SHA256(file).toHexString()==hash,"Frozen cache changed");
        }
    }
    void validateFrozenTracks(){validateFrozenEdit(*edit);}
    void setTrackFrozen(int index,bool frozen,std::function<bool(float)> cancel={}) {
        auto& track=trackAt(index);
        if(track.isFrozen(te::Track::individualFreeze)==frozen){if(frozen)validateFrozenTracks();return;}
        if(frozen){require(track.getOutput().getDestinationTrack()==nullptr,"Freeze the destination bus instead");
            require(track.pluginList.getPluginsOfType<te::AuxSendPlugin>().isEmpty(),"Freeze with sends is not supported yet");
            require(track.getInputTracks().isEmpty()&&track.pluginList.getPluginsOfType<te::AuxReturnPlugin>().isEmpty(),"Bus and auxiliary return freezing is not supported yet");
            require(std::abs(engine.getDeviceManager().getSampleRate()-48000)<0.01&&engine.getDeviceManager().getBlockSize()>0,"Configure the engine clock before freezing");
            for(auto* plugin:track.pluginList.getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot freeze unavailable plugin");}
        }
        edit->flushState();auto previous=edit->state.createCopy();auto id=edit->getProjectItemID();bool wasDirty=edit->hasChangedSinceSaved();
        auto& ui=dynamic_cast<OfflineUI&>(engine.getUIBehaviour());ui.taskCancelled=false;ui.cancelTask=std::move(cancel);
        try{
            track.setFrozen(frozen,te::Track::individualFreeze);
            auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
            while(juce::Time::getMillisecondCounterHiRes()<deadline){juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
                if(ui.taskCancelled)throw std::runtime_error("Freeze cancelled; original edit restored");
                if(track.isFrozen(te::Track::individualFreeze)==frozen&&(!frozen||te::TemporaryFileManager::getFreezeFileForTrack(track).existsAsFile())){
                    ui.cancelTask={};
                    for(auto key:{"studioFreezeIdentity","studioFreezeAsset","studioFreezeHash","studioFreezeName"})track.state.removeProperty(key,nullptr);
                    if(frozen){auto file=te::TemporaryFileManager::getFreezeFileForTrack(track);std::unique_ptr<juce::AudioFormatReader> reader(engine.getAudioFileFormatManager().getFrozenFileFormat()->createReaderFor(file.createInputStream().release(),true));require(reader&&reader->lengthInSamples>0,"Freeze produced invalid audio");track.state.setProperty("studioFreezeHash",juce::SHA256(file).toHexString(),nullptr);edit->flushState();track.state.setProperty("studioFreezeIdentity",freezeIdentity(*edit,track),nullptr);}
                    return;
                }
            }
            throw std::runtime_error("Freeze/unfreeze did not complete");
        }catch(...){
            ui.cancelTask={};te::Edit::Options options{engine,previous,id};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[this]{return projectFile;};
            auto restored=std::make_unique<te::Edit>(options);edit=std::move(restored);if(!wasDirty)edit->resetChangedStatus();else edit->markAsChanged();throw;
        }
    }
    void undo(){edit->undo();edit->getUndoManager().beginNewTransaction();}
    void redo(){edit->redo();edit->getUndoManager().beginNewTransaction();}
    void render(const juce::File& output, int blockSize=128, int64_t endSample=48000, int bitDepth=32, int64_t startSample=0, int selectedTrack=-1, bool includeMaster=true) {
        require(!output.exists(),"output already exists");validateFrozenTracks();
        for(auto* track:te::getAudioTracks(*edit))for(auto* plugin:track->pluginList.getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot export: unavailable external plugin");}
        for(auto* plugin:edit->getMasterPluginList().getPluginsOfType<te::ExternalPlugin>()){plugin->initialiseFully();require(plugin->getAudioPluginInstance()!=nullptr,"Cannot export: unavailable master plugin");}
        // Each offline render uses fresh processor instances and latency metadata.
        edit->flushState();te::Edit::Options renderOptions{engine,edit->state.createCopy(),edit->getProjectItemID()};renderOptions.role=te::Edit::forRendering;renderOptions.numAudioTracks=0;renderOptions.editFileRetriever=[this]{return projectFile;};
        auto stemEdit=std::make_unique<te::Edit>(renderOptions);
        if(selectedTrack>=0){auto& target=trackAt(selectedTrack);juce::Array<int> included;auto originals=te::getAudioTracks(*edit);
            for(int i=0;i<originals.size();++i){juce::Array<te::AudioTrack*> forward,backward;if(reaches(target,*originals[i],forward)||reaches(*originals[i],target,backward))included.add(i);}

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
