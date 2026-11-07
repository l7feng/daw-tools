#pragma once
#include <filesystem>
#include "render_guard.hpp"
// Render an immutable project snapshot in a device-disabled child process.
// Partial media stays in the job workspace and is never presented as a final export.
class ExportJob {
    juce::ChildProcess process;
    juce::File destination,staging;
    bool cancelled=false;RenderGuard guard;
public:
    juce::File folder;
    bool active=false;
    ~ExportJob(){if(active&&process.isRunning())process.kill();}
    void start(Session& session,const juce::File& target,int64_t startSample,int64_t endSample,int depth,int timeoutMs=600000){
        require(timeoutMs>0&&timeoutMs<=86400000,"Invalid render timeout");require(!active,"An export is already active");require(!target.exists(),"Export destination exists");
        require(startSample>=0&&endSample>startSample&&(depth==24||depth==32),"Invalid export settings");
        require(session.missingMedia().isEmpty(),"Resolve missing media before exporting");
        folder=session.root.getChildFile("export-"+juce::Uuid().toString());require(folder.createDirectory().wasOk(),"Cannot create export workspace");
        require(folder.getChildFile("studio-validation.marker").replaceWithText("isolated export"),"Cannot mark workspace");
        session.writeProject(folder.getChildFile("p.tracktionedit"),false,false);captureRenderInputs(session,folder);
        staging=target.getSiblingFile(target.getFileName()+"."+juce::Uuid().toString()+".partial");
        auto request=std::make_unique<juce::DynamicObject>();request->setProperty("staging",staging.getFullPathName());request->setProperty("start",juce::int64(startSample));request->setProperty("end",juce::int64(endSample));request->setProperty("depth",depth);request->setProperty("timeoutMs",timeoutMs);
        require(folder.getChildFile("request.json").replaceWithText(juce::JSON::toString(juce::var(request.release()))),"Cannot write export request");
        destination=target;cancelled=false;
        juce::StringArray args{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--workspace",folder.getFullPathName(),"--render-worker"};
        guard.start(folder,timeoutMs);require(process.start(args,0),"Cannot start render worker");active=true;
    }
    float progress()const{return std::clamp(folder.getChildFile("render-progress.txt").loadFileAsString().getFloatValue(),0.0f,1.0f);}
    void cancel(){if(!active)return;cancelled=true;require(!process.isRunning()||process.kill()||!process.isRunning(),"Cannot stop render worker");}
    bool poll(juce::String& result){
        if(!active)return false;if(!cancelled)guard.enforce(process);if(process.isRunning())return false;active=false;
        if(cancelled){renderJobStatus(folder,"cancelled");result="cancelled";return true;}
        try{guard.finished(process);guard.validate();
        require(process.getExitCode()==0&&folder.getChildFile("RENDER_DONE").existsAsFile(),"Render worker failed; inspect job FAIL.txt");
        require(staging.existsAsFile(),"Rendered staging file absent");
        require(!destination.exists(),"Destination appeared during export; no overwrite");
        guard.validate();
        std::filesystem::rename(std::filesystem::path(staging.getFullPathName().toWideCharPointer()),std::filesystem::path(destination.getFullPathName().toWideCharPointer()));
        renderJobStatus(folder,"complete");result=destination.getFullPathName();return true;
        }catch(const std::exception& error){renderJobStatus(folder,guard.failureStatus,error.what());throw;}
    }
};
static void renderWorker(Session& session){
    auto request=juce::JSON::parse(session.root.getChildFile("request.json"));require(request.isObject(),"Invalid render request");
    int timeout=int(request["timeoutMs"]);require(timeout>0&&timeout<=86400000,"Invalid worker timeout");static std::unique_ptr<RenderDeadline> deadline;deadline=std::make_unique<RenderDeadline>(session.root,timeout);RenderInputWatch inputs;inputs.start(session.root);loadRenderSnapshot(session);
    session.render(session.root.getChildFile("render.wav"),256,juce::int64(request["end"]),int(request["depth"]),juce::int64(request["start"]));
    inputs.validate();validateRenderInputs(session.root);
    auto staging=request["staging"].toString();if(staging.isNotEmpty()){renderJobStatus(session.root,"copying");copyRenderStaging(session.root.getChildFile("render.wav"),juce::File(staging));inputs.validate();validateRenderInputs(session.root);}
    require(session.root.getChildFile("RENDER_DONE").replaceWithText("complete"),"Cannot write completion marker");
}
