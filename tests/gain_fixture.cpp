// Original deterministic test processor; not a third-party production effect.
#include <juce_audio_processors/juce_audio_processors.h>
class GainFixture final : public juce::AudioProcessor {
    juce::AudioParameterFloat* gain;
public:
    GainFixture():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)) {
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",0.0f,1.0f,0.5f));
    }
    const juce::String getName() const override {return "Studio Test Gain";}
    void prepareToPlay(double,int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {buffer.applyGain(gain->get());}
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
    void getStateInformation(juce::MemoryBlock& state) override {juce::MemoryOutputStream stream(state,false);stream.writeInt(1);stream.writeFloat(gain->get());}
    void setStateInformation(const void* data,int size) override {if(size!=8)return;juce::MemoryInputStream stream(data,size,false);if(stream.readInt()!=1)return;float value=stream.readFloat();if(std::isfinite(value)&&value>=0&&value<=1)gain->setValueNotifyingHost(value);}
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new GainFixture();}
