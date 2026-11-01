#pragma once
#include <thread>
#include <condition_variable>
#include <chrono>
static void renderJobStatus(const juce::File& folder,const juce::String& status,const juce::String& error={}){
 auto* object=new juce::DynamicObject;object->setProperty("status",status);if(error.isNotEmpty())object->setProperty("error",error);folder.getChildFile("job-status.json").replaceWithText(juce::JSON::toString(juce::var(object)));
}
static void captureRenderInputs(Session& session,const juce::File& folder){
 juce::StringArray paths;juce::Array<juce::var> entries;
 auto add=[&](const juce::File& file){auto path=file.getFullPathName();if(paths.contains(path))return;require(file.existsAsFile(),"Referenced render input missing");paths.add(path);auto* entry=new juce::DynamicObject;entry->setProperty("path",path);entry->setProperty("sha256",juce::SHA256(file).toHexString());entries.add(juce::var(entry));};
 add(folder.getChildFile("p.tracktionedit"));for(auto* clip:session.clips())if(auto* wave=dynamic_cast<te::WaveAudioClip*>(clip))add(wave->getCurrentSourceFile());
 auto xml=juce::XmlDocument::parse(folder.getChildFile("p.tracktionedit"));require(xml!=nullptr,"Cannot inspect render snapshot");
 std::function<void(juce::XmlElement&)> inspect=[&](juce::XmlElement& node){if(node.hasTagName("PLUGIN")&&node.getStringAttribute("type")=="vst"){juce::File file(node.getStringAttribute("filename"));if(file.isDirectory()){auto files=file.findChildFiles(juce::File::findFiles,true);require(!files.isEmpty(),"Empty plugin bundle");for(auto child:files)add(child);}else add(file);}if(node.hasAttribute("studioFreezeAsset")&&node.getBoolAttribute("frozenIndividually"))add(folder.getChildFile(node.getStringAttribute("studioFreezeAsset")));for(auto* child:node.getChildIterator())inspect(*child);};inspect(*xml);
 require(folder.getChildFile("inputs.json").replaceWithText(juce::JSON::toString(entries)),"Cannot record render input identities");
}
static void validateRenderInputs(const juce::File& folder){
 auto inputs=juce::JSON::parse(folder.getChildFile("inputs.json"));require(inputs.isArray()&&!inputs.getArray()->isEmpty(),"Invalid render input manifest");
 for(auto entry:*inputs.getArray()){juce::File file(entry["path"].toString());if(!file.existsAsFile()||juce::SHA256(file).toHexString()!=entry["sha256"].toString())throw std::runtime_error(("Render input changed: "+file.getFullPathName()).toStdString());}
}
static void loadRenderSnapshot(Session& session){
 validateRenderInputs(session.root);session.engine.getPluginManager().initialise();auto xml=juce::XmlDocument::parse(session.root.getChildFile("p.tracktionedit"));require(xml!=nullptr,"Invalid render snapshot");juce::VST3PluginFormat format;juce::StringArray scanned;
 std::function<void(juce::XmlElement&)> visit=[&](juce::XmlElement& node){if(node.hasTagName("PLUGIN")&&node.getStringAttribute("type")=="vst"){auto path=node.getStringAttribute("filename");if(path.isNotEmpty()&&!scanned.contains(path)){scanned.add(path);juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,path);for(auto* type:types)session.engine.getPluginManager().knownPluginList.addType(*type);}}for(auto* child:node.getChildIterator())visit(*child);};visit(*xml);session.openProject(session.root.getChildFile("p.tracktionedit"));require(session.missingMedia().isEmpty(),"Snapshot has missing media");
}
class RenderDeadline {
 std::mutex mutex;std::condition_variable cv;bool finished=false;std::thread thread;
public:
 RenderDeadline(const juce::File& folder,int timeoutMs):thread([this,folder,timeoutMs]{std::unique_lock<std::mutex> lock(mutex);if(!cv.wait_for(lock,std::chrono::milliseconds(timeoutMs),[this]{return finished;})){lock.unlock();folder.getChildFile("TIMEOUT").replaceWithText("worker time limit exceeded");std::_Exit(124);}}){}
 ~RenderDeadline(){{std::lock_guard<std::mutex> lock(mutex);finished=true;}cv.notify_one();thread.join();}
};
class RenderGuard {
 double deadline=0;juce::File folder;bool timedOut=false;
public:
 juce::String failureStatus="failed";
 void start(const juce::File& f,int timeoutMs){require(timeoutMs>0&&timeoutMs<=86400000,"Invalid render timeout");folder=f;deadline=juce::Time::getMillisecondCounterHiRes()+timeoutMs;timedOut=false;failureStatus="failed";renderJobStatus(folder,"running");}
 void enforce(juce::ChildProcess& process){if(process.isRunning()&&juce::Time::getMillisecondCounterHiRes()>=deadline){timedOut=true;if(!process.kill()&&process.isRunning()){failureStatus="timed_out";renderJobStatus(folder,"timed_out","Cannot terminate renderer; retry required");throw std::runtime_error("Cannot terminate timed-out renderer");}}}
 void finished(juce::ChildProcess& process){if(timedOut||process.getExitCode()==124||folder.getChildFile("TIMEOUT").existsAsFile()){failureStatus="timed_out";throw std::runtime_error("Render task timed out; no delivery committed");}}
 void validate(){try{validateRenderInputs(folder);}catch(...){failureStatus="input_changed";throw;}}
};
