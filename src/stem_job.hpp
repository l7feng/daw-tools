#pragma once
#include <filesystem>
#include "render_guard.hpp"
static void stemJobStatus(const juce::File& folder,const juce::String& status,const juce::String& error={}){
 auto report=juce::JSON::parse(folder.getChildFile("manifest.json"));if(!report.isObject())report=juce::var(new juce::DynamicObject);
 report.getDynamicObject()->setProperty("status",status);if(error.isNotEmpty())report.getDynamicObject()->setProperty("error",error);
 require(folder.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(report)),"Cannot persist stem manifest");
}
static void stemWorker(Session& session){
 auto request=juce::JSON::parse(session.root.getChildFile("request.json"));require(request.isObject()&&request["tracks"].isArray(),"Invalid stem request");
 int timeout=int(request["timeoutMs"]);require(timeout>0&&timeout<=86400000,"Invalid worker timeout");static std::unique_ptr<RenderDeadline> deadline;deadline=std::make_unique<RenderDeadline>(session.root,timeout);RenderInputWatch inputs;inputs.start(session.root);loadRenderSnapshot(session);
 auto outputs=session.root.getChildFile("outputs");require(outputs.createDirectory().wasOk(),"Cannot create stem outputs");juce::Array<juce::var> results;
 auto persist=[&](const juce::String& status){auto* obj=new juce::DynamicObject;obj->setProperty("source_semantics","isolated source plus downstream processing; nonlinear contributions need not sum to mix");obj->setProperty("bus_semantics","return bus including all currently audible contributors");obj->setProperty("status",status);obj->setProperty("results",results);require(session.root.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(juce::var(obj))),"Cannot persist stem results");};persist("running");
 for(auto index:*request["tracks"].getArray()){
  auto* result=new juce::DynamicObject;juce::var item(result);int track=int(index);result->setProperty("track",track);auto name="track-"+juce::String(track)+".wav";result->setProperty("file",name);
  try{inputs.validate();validateRenderInputs(session.root);auto output=outputs.getChildFile(name);session.render(output,256,juce::int64(request["end"]),32,juce::int64(request["start"]),track,bool(request["master"]));result->setProperty("status","complete");result->setProperty("sha256",juce::SHA256(output).toHexString());results.add(item);persist("running");}
  catch(const std::exception& error){result->setProperty("status","failed");result->setProperty("error",error.what());results.add(item);persist("failed");throw;}
 }
 inputs.validate();validateRenderInputs(session.root);
 auto stagingPath=request["staging"].toString();if(stagingPath.isNotEmpty()){
  juce::File staging(stagingPath);require(!staging.exists()&&staging.getParentDirectory().isDirectory(),"Stem staging target unavailable");require(std::filesystem::create_directory(std::filesystem::path(staging.getFullPathName().toWideCharPointer())),"Stem staging appeared; refusing collision");renderJobStatus(session.root,"copying");
  for(auto file:outputs.findChildFiles(juce::File::findFiles,false))copyRenderStaging(file,staging.getChildFile(file.getFileName()));inputs.validate();validateRenderInputs(session.root);
 }
 persist("rendered");require(session.root.getChildFile("STEMS_DONE").replaceWithText("complete"),"Cannot mark stems done");
}
class StemJob{
 juce::ChildProcess process;juce::File destination,staging;bool cancelled=false;RenderGuard guard;
public:
 juce::File folder;bool active=false;
 ~StemJob(){if(active&&process.isRunning())process.kill();}
 void start(Session& session,const juce::File& target,const juce::Array<int>& tracks,int64_t startSample,int64_t endSample,bool master=false,int timeoutMs=600000){
  require(timeoutMs>0&&timeoutMs<=86400000,"Invalid render timeout");require(!active&&!target.exists()&&target.getParentDirectory().isDirectory(),"Stem target unavailable");require(!tracks.isEmpty()&&startSample>=0&&endSample>startSample,"Invalid stem selection/range");juce::Array<int> unique;juce::Array<juce::var> ids;for(int track:tracks){require(track>=0&&!unique.contains(track),"Invalid or duplicate stem track");unique.add(track);ids.add(track);}require(session.missingMedia().isEmpty(),"Resolve missing media before stems");
  folder=session.root.getChildFile("stems-"+juce::Uuid().toString());require(folder.createDirectory().wasOk(),"Cannot create stem workspace");require(folder.getChildFile("studio-validation.marker").replaceWithText("isolated stem export"),"Cannot mark stem workspace");session.writeProject(folder.getChildFile("p.tracktionedit"),false,false);captureRenderInputs(session,folder);
  staging=target.getSiblingFile(target.getFileName()+"."+juce::Uuid().toString()+".partial");auto* request=new juce::DynamicObject;request->setProperty("staging",staging.getFullPathName());request->setProperty("tracks",ids);request->setProperty("start",juce::int64(startSample));request->setProperty("end",juce::int64(endSample));request->setProperty("master",master);request->setProperty("timeoutMs",timeoutMs);require(folder.getChildFile("request.json").replaceWithText(juce::JSON::toString(juce::var(request))),"Cannot write stem request");destination=target;cancelled=false;stemJobStatus(folder,"queued");
  juce::StringArray args{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--workspace",folder.getFullPathName(),"--stem-worker"};guard.start(folder,timeoutMs);require(process.start(args,0),"Cannot launch stem worker");active=true;
 }
 void cancel(){if(!active)return;cancelled=true;require(!process.isRunning()||process.kill()||!process.isRunning(),"Cannot stop stem worker");}
 bool poll(juce::String& result){
  if(!active)return false;if(!cancelled)guard.enforce(process);if(process.isRunning())return false;active=false;if(cancelled){stemJobStatus(folder,"cancelled");renderJobStatus(folder,"cancelled");result="cancelled";return true;}
  try{guard.finished(process);guard.validate();
   require(process.getExitCode()==0&&folder.getChildFile("STEMS_DONE").existsAsFile(),"Stem worker failed; inspect manifest and FAIL.txt");require(!destination.exists(),"Stem target appeared; refusing overwrite");require(staging.isDirectory(),"Stem staging directory absent");
   auto delivery=juce::JSON::parse(folder.getChildFile("manifest.json"));require(delivery.isObject(),"Invalid delivery manifest");delivery.getDynamicObject()->setProperty("status","complete");require(staging.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(delivery)),"Cannot write delivery manifest");guard.validate();std::filesystem::rename(std::filesystem::path(staging.getFullPathName().toWideCharPointer()),std::filesystem::path(destination.getFullPathName().toWideCharPointer()));folder.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(delivery));renderJobStatus(folder,"complete");result=destination.getFullPathName();return true;
  }catch(const std::exception& error){stemJobStatus(folder,guard.failureStatus,error.what());renderJobStatus(folder,guard.failureStatus,error.what());throw;}
 }
};
