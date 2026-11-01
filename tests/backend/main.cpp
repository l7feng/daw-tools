#include "../../src/session.hpp"
#include "../offline_cases.hpp"
#include "device_probe.hpp"
#include "midi_engine.hpp"
#include "midi_transport.hpp"
#include "realtime_probe.hpp"
#include "plugin_recovery.hpp"
#include "tail_cases.hpp"
#include "stem_cases.hpp"
#include "freeze_cases.hpp"
#include "freeze_extended.hpp"
static void testChain(const juce::File& root,const juce::File& fixture){
 Session s(root);juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,fixture.getFullPathName());require(types.size()==1&&types[0]->name=="Studio Test Gain","Fixture scan failed");
 auto source=root.getChildFile("source.wav");juce::AudioBuffer<float> audio(2,48000);audio.clear();audio.setSample(0,12000,0.001f);audio.setSample(1,24000,-0.002f);juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),48000,2,32,{},0));require(writer&&writer->writeFromAudioSampleBuffer(audio,0,48000),"Source write failed");writer.reset();s.importAudio(source,0,0);
 s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(*types[0]);
 auto plugin=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*types[0]);auto* external=dynamic_cast<te::ExternalPlugin*>(plugin.get());require(external!=nullptr,"Plugin creation failed");s.trackAt(0).pluginList.insertPlugin(plugin,0,nullptr);external->initialiseFully();if(!external->getAudioPluginInstance())throw std::runtime_error(("Plugin load failed: "+external->getLoadError()).toStdString());
 int initialInputs=external->getNumInputs(),initialOutputs=external->getNumOutputs();
 require(external->setBusLayout(juce::AudioChannelSet::stereo(),true,0)&&external->setBusLayout(juce::AudioChannelSet::stereo(),false,0),"Stereo layout rejected");
 int checks=0;float maxError=0;
 auto verify=[&](const char* name,float gain,int block){auto output=root.getChildFile(juce::String(name)+".wav");s.render(output,block);auto result=readWave(output);for(int c=0;c<2;++c)for(int i=0;i<48000;++i)maxError=std::max(maxError,std::abs(result.getSample(c,i)-audio.getSample(c,i)*gain));require(maxError<=1e-6f,name);++checks;};
 verify("inserted",0.5f,128);external->setEnabled(false);verify("bypass",1,256);external->setEnabled(true);
 external->getAudioPluginInstance()->getParameters()[0]->setValueNotifyingHost(0.25f);external->flushPluginStateToValueTree();verify("parameter",0.25f,128);s.save();plugin=nullptr;s.reopen();verify("reopened",0.25f,256);
 auto* restored=s.trackAt(0).pluginList.findFirstPluginOfType<te::ExternalPlugin>();require(restored!=nullptr,"Plugin missing after reopen");restored->deleteFromParent();verify("removed",1,128);
 // N05/N07: identical plugin types must retain independent instance state.
 auto addGain=[&](float value){
  auto item=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*types[0]);
  auto* effect=dynamic_cast<te::ExternalPlugin*>(item.get());
  require(effect!=nullptr,"Second chain creation failed");
  s.trackAt(0).pluginList.insertPlugin(item,0,nullptr);effect->initialiseFully();
  require(effect->getAudioPluginInstance()!=nullptr,"Second chain load failed");
  effect->getAudioPluginInstance()->getParameters()[0]->setValueNotifyingHost(value);
  effect->flushPluginStateToValueTree();return effect;
 };
 auto* first=addGain(0.25f);auto* second=addGain(0.5f);
 verify("two_instances",0.125f,128);
 second->getAudioPluginInstance()->getParameters()[0]->setValueNotifyingHost(0.75f);
 second->flushPluginStateToValueTree();
 require(std::abs(first->getAudioPluginInstance()->getParameters()[0]->getValue()-0.25f)<1e-7f,"Instance parameters leaked");
 verify("independent_parameters",0.1875f,256);
 s.save();s.reopen();
 verify("two_instances_reopened",0.1875f,128);
 juce::Array<te::ExternalPlugin*> chain;
 for(auto* item:s.trackAt(0).pluginList)if(auto* effect=dynamic_cast<te::ExternalPlugin*>(item))chain.add(effect);
 require(chain.size()==2,"Restored instance count differs");
 require(std::abs(chain[0]->getAudioPluginInstance()->getParameters()[0]->getValue()-0.75f)<1e-7f&&std::abs(chain[1]->getAudioPluginInstance()->getParameters()[0]->getValue()-0.25f)<1e-7f,"Restored per-instance state or order differs");
 chain[0]->setEnabled(false);verify("one_instance_bypassed",0.25f,256);
 s.save();s.reopen();verify("bypass_reopened",0.25f,128);
 auto* bypassed=s.trackAt(0).pluginList.findFirstPluginOfType<te::ExternalPlugin>();
 require(bypassed!=nullptr&&!bypassed->isEnabled(),"Bypass state lost");
 bypassed->deleteFromParent();verify("one_instance_removed",0.25f,256);
 require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"Audio device opened");
 root.getChildFile("backend-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"max_error\":"+juce::String(maxError,9)+",\"initial_inputs\":"+juce::String(initialInputs)+",\"initial_outputs\":"+juce::String(initialOutputs)+",\"device_opened\":false}");
}
static void testPdc(const juce::File& root){
 auto dir=root.getChildFile("pdc");dir.createDirectory();Session s(dir);
 auto source=root.getChildFile("source.wav");s.importAudio(source,0,0);s.importAudio(source,1,0);
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;
 format.findAllTypesForFile(types,juce::File(STUDIO_SOURCE_ROOT).getChildFile("build/backend/StudioTestDelay_artefacts/Release/VST3/Studio Test Delay.vst3").getFullPathName());
 require(types.size()==1,"Delay fixture scan failed");s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(*types[0]);
 auto item=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,*types[0]);
 auto* effect=dynamic_cast<te::ExternalPlugin*>(item.get());require(effect!=nullptr,"Delay creation failed");
 s.trackAt(1).pluginList.insertPlugin(item,0,nullptr);effect->initialiseFully();require(effect->getAudioPluginInstance()!=nullptr,"Delay load failed");
 auto reference=readWave(source);float error=0;int checks=0;
 for(int block:{128,256}){auto output=dir.getChildFile("parallel-"+juce::String(block)+".wav");s.render(output,block);auto result=readWave(output);
  require(result.getNumSamples()==48000,"PDC output length differs");
  for(int c=0;c<2;++c)for(int i=0;i<48000;++i)error=std::max(error,std::abs(result.getSample(c,i)-2*reference.getSample(c,i)));
  require(error<=1e-6f,"Parallel PDC misalignment");++checks;
 }
 require(effect->getAudioPluginInstance()->getLatencySamples()==257,"Fixture latency not reported");
 require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"PDC opened device");
 dir.getChildFile("pdc-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"latency_samples\":257,\"max_error\":"+juce::String(error,9)+",\"device_opened\":false}");
}
class Probe:public juce::JUCEApplication{
public:
 const juce::String getApplicationName()override{return "Studio Backend Probe";}
 const juce::String getApplicationVersion()override{return "0.1.0";}
 void initialise(const juce::String& command)override{auto args=juce::StringArray::fromTokens(command,true);if(args.size()<2||args.size()>5){setApplicationReturnValue(2);quit();return;}juce::File root(args[0].unquoted()),fixture(args[1].unquoted());if(!root.isAChildOf(juce::File(STUDIO_SOURCE_ROOT).getChildFile("build"))||!root.getChildFile("studio-validation.marker").existsAsFile()){setApplicationReturnValue(2);quit();return;}try{if(args[1]=="--realtime")realtimeProbe(root,args.size()>=3?args[2].getIntValue():1,args.size()>=4?args[3].getIntValue():0);else if(args[1]=="--devices"||args[1]=="--capture")deviceProbe(root,args[1]=="--capture",args.size()>=3?args[2].getIntValue():256,args.size()>=4?args[3].getIntValue():5,args.size()==5?args[4].unquoted():"Windows Audio");else{testChain(root,fixture);testPdc(root);testMidiEngine(root);testMidiTransport(root);testPluginRecovery(root);testPluginRecovery(root,true);testTail(root);testStems(root);testFreeze(root);testExtendedFreeze(root);}}catch(const std::exception& e){root.getChildFile("FAIL.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();}
 void shutdown()override{}
};
START_JUCE_APPLICATION(Probe)
