#include "../../src/session.hpp"
#include "../offline_cases.hpp"
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
 require(s.engine.getDeviceManager().deviceManager.getCurrentAudioDevice()==nullptr,"Audio device opened");
 root.getChildFile("backend-result.json").replaceWithText("{\"checks\":"+juce::String(checks)+",\"max_error\":"+juce::String(maxError,9)+",\"initial_inputs\":"+juce::String(initialInputs)+",\"initial_outputs\":"+juce::String(initialOutputs)+",\"device_opened\":false}");
}
class Probe:public juce::JUCEApplication{
public:
 const juce::String getApplicationName()override{return "Studio Backend Probe";}
 const juce::String getApplicationVersion()override{return "0.1.0";}
 void initialise(const juce::String& command)override{auto args=juce::StringArray::fromTokens(command,true);if(args.size()!=2){setApplicationReturnValue(2);quit();return;}juce::File root(args[0].unquoted()),fixture(args[1].unquoted());if(!root.isAChildOf(juce::File(STUDIO_SOURCE_ROOT).getChildFile("build"))||!root.getChildFile("studio-validation.marker").existsAsFile()){setApplicationReturnValue(2);quit();return;}try{testChain(root,fixture);}catch(const std::exception& e){root.getChildFile("FAIL.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();}
 void shutdown()override{}
};
START_JUCE_APPLICATION(Probe)
