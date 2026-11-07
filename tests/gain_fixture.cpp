// Original deterministic test processor; not a third-party production effect.
#include <juce_audio_processors/juce_audio_processors.h>
class GainFixture final : public juce::AudioProcessor {
    juce::AudioParameterFloat* gain;juce::AudioParameterFloat* saturation;
public:
    GainFixture():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)) {
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",0.0f,1.0f,0.5f));
        addParameter(saturation=new juce::AudioParameterFloat(juce::ParameterID{"saturation",1},"Saturation",0.0f,1.0f,0.0f));
    }
    const juce::String getName() const override {return "Studio Test Gain";}
    void prepareToPlay(double,int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {buffer.applyGain(gain->get());if(saturation->get()>0)for(int c=0;c<buffer.getNumChannels();++c)for(int i=0;i<buffer.getNumSamples();++i){auto x=buffer.getSample(c,i);auto wet=std::tanh(x*1000)/1000;buffer.setSample(c,i,x+(wet-x)*saturation->get());}}
    juce::AudioProcessorEditor* createEditor() override {return new juce::GenericAudioProcessorEditor(*this);}
    bool hasEditor() const override {return true;}
    double getTailLengthSeconds() const override {return 0;}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Default";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override {juce::MemoryOutputStream stream(state,false);stream.writeInt(2);stream.writeFloat(gain->get());stream.writeFloat(saturation->get());}
    void setStateInformation(const void* data,int size) override {if(size!=8&&size!=12)return;juce::MemoryInputStream stream(data,size,false);auto version=stream.readInt();if(version!=1&&version!=2)return;float value=stream.readFloat();if(std::isfinite(value)&&value>=0&&value<=1)gain->setValueNotifyingHost(value);if(version==2&&size==12){auto amount=stream.readFloat();if(std::isfinite(amount)&&amount>=0&&amount<=1)saturation->setValueNotifyingHost(amount);}}
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new GainFixture();}
