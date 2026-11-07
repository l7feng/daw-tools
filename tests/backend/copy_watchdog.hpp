#pragma once
#include "../../src/render_guard.hpp"
static void copyWatchdogWorker(Session& s){
 auto source=s.root.getChildFile("copy-source.bin");juce::MemoryBlock bytes(1048576,true);require(source.replaceWithData(bytes.getData(),bytes.getSize()),"Cannot create copy watchdog source");static std::unique_ptr<RenderDeadline> deadline;deadline=std::make_unique<RenderDeadline>(s.root,200);
 copyRenderStaging(source,s.root.getChildFile("copy.partial"),[&](int64_t,int64_t){require(s.root.getChildFile("COPY_ENTERED").replaceWithText("inside native CopyFileEx callback"),"Cannot mark copy entry");juce::Thread::sleep(30000);});s.root.getChildFile("COPY_DONE").replaceWithText("complete");
}
static void testCopyWatchdog(const juce::File& root){
 auto dir=root.getChildFile("copy-watchdog");dir.createDirectory();dir.getChildFile("studio-validation.marker").replaceWithText("owned copy watchdog");juce::ChildProcess worker;require(worker.start(juce::StringArray{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--workspace",dir.getFullPathName(),"--copy-watchdog"},0),"Copy worker launch failed");require(worker.waitForProcessToFinish(10000),"Copy watchdog did not finish");require(worker.getExitCode()==124&&dir.getChildFile("COPY_ENTERED").existsAsFile()&&dir.getChildFile("TIMEOUT").existsAsFile()&&!dir.getChildFile("COPY_DONE").exists(),"Copy phase not covered by worker deadline");dir.getChildFile("copy-watchdog-result.json").replaceWithText("{\"checks\":1,\"native_copy_entered\":true,\"deadline_exit\":124,\"physical_device_opened\":false}");
}
