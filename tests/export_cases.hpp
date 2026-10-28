#pragma once
static void exportTest(Session& original){
    auto dir=original.root.getChildFile("export-tests");require(dir.createDirectory().wasOk(),"export test directory");Session s(dir);
    auto source=original.root.getChildFile("mixer/signal.wav");s.importAudio(source,0,0);int checks=0;
    auto wait=[&](ExportJob& job){auto deadline=juce::Time::getMillisecondCounterHiRes()+20000;juce::String result;while(!job.poll(result)){require(juce::Time::getMillisecondCounterHiRes()<deadline,"export timeout");juce::Thread::sleep(10);}return result;};
    ExportJob successful;auto output=dir.getChildFile("snapshot-export.wav");successful.start(s,output,6000,30000,32);s.setMix(0,0,0,true,false);wait(successful);
    auto buffer=readWave(output,24000);require(std::abs(buffer.getSample(0,6000)-0.001f)<1e-6f&&std::abs(buffer.getSample(1,18000)+0.0005f)<1e-6f,"export snapshot or range changed");++checks;
    s.setMix(0,0,0,false,false);ExportJob cancelled;auto absent=dir.getChildFile("cancelled.wav");cancelled.start(s,absent,0,48000*60,24);cancelled.cancel();require(wait(cancelled)=="cancelled"&&!absent.exists(),"cancelled export published file");++checks;
    ExportJob collision;auto occupied=dir.getChildFile("collision.wav");collision.start(s,occupied,0,48000,24);occupied.replaceWithText("do not overwrite");bool rejected=false;try{wait(collision);}catch(const std::exception&){rejected=true;}require(rejected&&occupied.loadFileAsString()=="do not overwrite","export overwrote a concurrently created destination");++checks;
    dir.getChildFile("export-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"snapshot_isolated\":true,\"cancel_no_final_file\":true,\"collision_preserved\":true}");
}
