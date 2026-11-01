// Original deterministic MIDI timing instrument. Test-only DC output; never route to speakers.
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
class MidiFixture final:public juce::AudioProcessor {
 std::array<std::array<float,128>,16> notes{};
 std::array<float,16> volume{},bend{};
 juce::AudioParameterFloat* gain=nullptr;
 void resetVoices(){for(auto& channel:notes)channel.fill(0);volume.fill(1);bend.fill(1);}
 void event(const juce::MidiMessage& message){
  int channel=message.getChannel()-1;if(channel<0||channel>=16)return;
  if(message.isNoteOn())notes[channel][message.getNoteNumber()]=message.getFloatVelocity();
  else if(message.isNoteOff())notes[channel][message.getNoteNumber()]=0;
  else if(message.isAllNotesOff()||message.isAllSoundOff())notes[channel].fill(0);
  else if(message.isController()&&message.getControllerNumber()==7)volume[channel]=message.getControllerValue()/127.0f;
  else if(message.isPitchWheel())bend[channel]=1.0f+0.25f*(message.getPitchWheelValue()-8192)/8192.0f;
 }
 float value()const{float sum=0;for(int c=0;c<16;++c)for(float velocity:notes[c])sum+=velocity*volume[c]*bend[c];return 0.001f*gain->get()*sum;}
public:
 MidiFixture():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){
  addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",0.0f,1.0f,1.0f));resetVoices();
 }
 const juce::String getName()const override{return "Studio MIDI Timing Fixture";}
 void prepareToPlay(double,int)override{resetVoices();}
 void releaseResources()override{resetVoices();}
 void reset()override{resetVoices();}
 bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.getMainInputChannelSet().isDisabled()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
 void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{
  audio.clear();int cursor=0;float level=value();
  for(const auto metadata:midi){int at=juce::jlimit(0,audio.getNumSamples(),metadata.samplePosition);
   for(int c=0;c<audio.getNumChannels();++c)juce::FloatVectorOperations::fill(audio.getWritePointer(c,cursor),level,at-cursor);
   cursor=at;event(metadata.getMessage());level=value();
  }
  for(int c=0;c<audio.getNumChannels();++c)juce::FloatVectorOperations::fill(audio.getWritePointer(c,cursor),level,audio.getNumSamples()-cursor);
  midi.clear();
 }
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}
 bool hasEditor()const override{return true;}
 juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "Timing";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& state)override{juce::MemoryOutputStream stream(state,false);stream.writeInt(1);stream.writeFloat(gain->get());}
 void setStateInformation(const void* data,int size)override{if(size!=8)return;juce::MemoryInputStream stream(data,size,false);if(stream.readInt()!=1)return;float v=stream.readFloat();if(std::isfinite(v)&&v>=0&&v<=1){gain->setValueNotifyingHost(v);resetVoices();}}
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new MidiFixture();}
