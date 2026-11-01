// Standalone real VST3 event-timing verification; no audio device is created.
#include <juce_audio_utils/juce_audio_utils.h>
#include <stdexcept>
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI initialiser;
 if(argc!=3)return 2;
 try{
  juce::File fixture=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
  juce::File report=juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
  check(!report.exists(),"Refusing existing report");
  juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> descriptions;format.findAllTypesForFile(descriptions,fixture.getFullPathName());
  check(descriptions.size()==1&&descriptions[0]->name=="Studio MIDI Timing Fixture","Unexpected fixture description");
  float error=0;int cases=0;
  for(int block:{128,256}){
   juce::String loadError;auto plugin=format.createInstanceFromDescription(*descriptions[0],48000,block,loadError);check(plugin!=nullptr,"VST3 instantiate failed");
   juce::AudioProcessor::BusesLayout layout;layout.outputBuses.add(juce::AudioChannelSet::stereo());check(plugin->setBusesLayout(layout),"Instrument bus rejected");
   plugin->setRateAndBufferSizeDetails(48000,block);plugin->prepareToPlay(48000,block);
   struct Event{int sample;juce::MidiMessage message;};
   const std::vector<Event> events{
    {17,juce::MidiMessage::noteOn(1,60,(juce::uint8)127)},
    {33,juce::MidiMessage::noteOn(2,67,(juce::uint8)64)},
    {65,juce::MidiMessage::controllerEvent(1,7,64)},
    {97,juce::MidiMessage::pitchWheel(1,16383)},
    {129,juce::MidiMessage::noteOff(1,60)},
    {257,juce::MidiMessage::allSoundOff(2)},
    {300,juce::MidiMessage::noteOn(1,72,(juce::uint8)127)},
    {400,juce::MidiMessage::allNotesOff(1)}};
   for(int offset=0;offset<512;offset+=block){
    juce::MidiBuffer midi;for(const auto& event:events)if(event.sample>=offset&&event.sample<offset+block)midi.addEvent(event.message,event.sample-offset);
    juce::AudioBuffer<float> audio(2,block);audio.clear();plugin->processBlock(audio,midi);
    for(int i=0;i<block;++i){int sample=offset+i;float expected=0;
     if(sample>=17&&sample<129)expected+=0.001f*(sample>=65?64.0f/127:1)*(sample>=97?1.0f+0.25f*8191/8192:1);
     if(sample>=33&&sample<257)expected+=0.001f*64/127;
     if(sample>=300&&sample<400)expected+=0.001f*64/127*(1.0f+0.25f*8191/8192);
     for(int c=0;c<2;++c)error=std::max(error,std::abs(audio.getSample(c,i)-expected));
    }
   }
   check(error<1e-7f,"MIDI sample timing or channel isolation differs");++cases;
   juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(3,76,(juce::uint8)127),0);audio.clear();plugin->processBlock(audio,midi);check(audio.getMagnitude(0,block)>0.0009f,"Reset test did not activate a voice");plugin->reset();midi.clear();audio.clear();plugin->processBlock(audio,midi);check(audio.getMagnitude(0,block)<1e-8f,"Reset left hanging note");++cases;
   plugin->releaseResources();
  }
  auto* object=new juce::DynamicObject;juce::var result(object);object->setProperty("cases",cases);object->setProperty("max_error",error);object->setProperty("device_opened",false);
  check(report.replaceWithText(juce::JSON::toString(result)),"Report write failed");return 0;
 }catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 1;}
}
