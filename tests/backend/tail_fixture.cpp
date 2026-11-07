// Original deterministic test processor; not a third-party production effect.
#include <juce_audio_processors/juce_audio_processors.h>
class TailFixture final : public juce::AudioProcessor {
    juce::AudioParameterFloat* gain;
    juce::AudioBuffer<float> delay; int cursor=0;
public:
    TailFixture():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)) {
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",0.0f,1.0f,0.5f));
    }
    const juce::String getName() const override {return "Studio Test Tail";}
    void prepareToPlay(double,int) override {setLatencySamples(0);delay.setSize(2,24000);delay.clear();cursor=0;}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {for(int i=0;i<buffer.getNumSamples();++i){for(int c=0;c<buffer.getNumChannels();++c){float value=delay.getSample(c,cursor);delay.setSample(c,cursor,buffer.getSample(c,i));buffer.setSample(c,i,buffer.getSample(c,i)+value*gain->get());}cursor=(cursor+1)%24000;}}
    juce::AudioProcessorEditor* createEditor() override {return new juce::GenericAudioProcessorEditor(*this);}
    bool hasEditor() const override {return true;}
    double getTailLengthSeconds() const override {return 0.5;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Default";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override {juce::MemoryOutputStream stream(state,false);stream.writeInt(1);stream.writeFloat(gain->get());}
    void setStateInformation(const void* data,int size) override {if(size!=8)return;juce::MemoryInputStream stream(data,size,false);if(stream.readInt()!=1)return;float value=stream.readFloat();if(std::isfinite(value)&&value>=0&&value<=1)gain->setValueNotifyingHost(value);}
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new TailFixture();}
