// Original deterministic test processor; not a third-party production effect.
#include <juce_audio_processors/juce_audio_processors.h>
class DelayFixture final : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener {
    juce::AudioParameterFloat* gain;juce::AudioParameterInt* latency;int activeDelay=257;
    juce::AudioBuffer<float> delay; int cursor=0;
public:
    DelayFixture():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)) {
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",0.0f,1.0f,0.5f));
        addParameter(latency=new juce::AudioParameterInt(juce::ParameterID{"latency",1},"Latency",0,4096,257));latency->addListener(this);setLatencySamples(257);
    }
    ~DelayFixture() override {latency->removeListener(this);}
    void parameterValueChanged(int,float) override {setLatencySamples(latency->get());}
    void parameterGestureChanged(int,bool) override {}
    const juce::String getName() const override {return "Studio Test Delay";}
    void prepareToPlay(double,int) override {activeDelay=latency->get();setLatencySamples(activeDelay);delay.setSize(2,4097);delay.clear();cursor=0;}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {
        auto next=latency->get();if(next!=activeDelay){activeDelay=next;delay.clear();cursor=0;setLatencySamples(activeDelay);}
        for(int i=0;i<buffer.getNumSamples();++i){for(int c=0;c<buffer.getNumChannels();++c){float input=buffer.getSample(c,i);float value=activeDelay==0?input:delay.getSample(c,(cursor+4097-activeDelay)%4097);delay.setSample(c,cursor,input);buffer.setSample(c,i,value);}cursor=(cursor+1)%4097;}
    }
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
    void getStateInformation(juce::MemoryBlock& state) override {juce::MemoryOutputStream stream(state,false);stream.writeInt(2);stream.writeFloat(gain->get());stream.writeInt(latency->get());}
    void setStateInformation(const void* data,int size) override {if(size!=8&&size!=12)return;juce::MemoryInputStream stream(data,size,false);int version=stream.readInt();if(version!=1&&version!=2)return;float value=stream.readFloat();if(std::isfinite(value)&&value>=0&&value<=1)gain->setValueNotifyingHost(value);if(version==2&&size==12){int n=stream.readInt();if(n>=0&&n<=4096)latency->setValueNotifyingHost(latency->convertTo0to1(n));}}

};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new DelayFixture();}
