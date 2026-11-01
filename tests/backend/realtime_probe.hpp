#pragma once
struct EngineCallback final:juce::AudioIODeviceCallback {
 te::HostedAudioDeviceInterface& hosted;juce::AudioBuffer<float> scratch{2,8192};juce::MidiBuffer midi;
 std::atomic<int> phase{0},calls{0},errors{0};std::atomic<float> playPeak{0},pausePeak{0};std::atomic<double> maxProcessMs{0};
 explicit EngineCallback(te::HostedAudioDeviceInterface& h):hosted(h){midi.ensureSize(4096);}
 void audioDeviceAboutToStart(juce::AudioIODevice*)override{}
 void audioDeviceStopped()override{}
 void audioDeviceError(const juce::String&)override{++errors;}
 void audioDeviceIOCallbackWithContext(const float* const*,int,float* const* output,int outputs,int count,const juce::AudioIODeviceCallbackContext&)override{
  for(int c=0;c<outputs;++c)if(output[c])juce::FloatVectorOperations::clear(output[c],count);
  if(count>8192){++errors;return;}
  auto start=juce::Time::getMillisecondCounterHiRes();scratch.clear();midi.clear();
  float* channels[]={scratch.getWritePointer(0),scratch.getWritePointer(1)};juce::AudioBuffer<float> block(channels,2,count);
  hosted.processBlock(block,midi);float peak=block.getMagnitude(0,count);
  int p=phase.load();if(p==1)playPeak.store(std::max(playPeak.load(),peak));if(p==2)pausePeak.store(std::max(pausePeak.load(),peak));
  for(int c=0;c<std::min(outputs,2);++c)if(output[c])juce::FloatVectorOperations::copy(output[c],channels[c],count);
  maxProcessMs.store(std::max(maxProcessMs.load(),juce::Time::getMillisecondCounterHiRes()-start));++calls;
 }
};
static void realtimeProbe(const juce::File& root){
 Session s(root);s.edit.reset();auto& hosted=s.engine.getDeviceManager().getHostedAudioDeviceInterface();
 juce::AudioDeviceManager enumerator;juce::OwnedArray<juce::AudioIODeviceType> types;enumerator.createAudioDeviceTypes(types);
 juce::AudioIODeviceType* asio=nullptr;for(auto* t:types)if(t->getTypeName()=="ASIO"){t->scanForDevices();if(t->getDeviceNames(false).contains("Yamaha Steinberg USB ASIO"))asio=t;}
 require(asio!=nullptr,"Steinberg ASIO unavailable");std::unique_ptr<juce::AudioIODevice> device(asio->createDevice("Yamaha Steinberg USB ASIO",""));require(device!=nullptr,"ASIO device creation failed");
 juce::BigInteger outputs;outputs.setRange(0,2,true);auto error=device->open({},outputs,48000,256);if(error.isNotEmpty())throw std::runtime_error(error.toStdString());
 te::HostedAudioDeviceInterface::Parameters params;params.sampleRate=device->getCurrentSampleRate();params.blockSize=device->getCurrentBufferSizeSamples();params.inputChannels=0;params.outputChannels=2;hosted.initialise(params);hosted.prepareToPlay(params.sampleRate,params.blockSize);
 te::Edit::Options options{s.engine,te::createEmptyEdit(s.engine),te::ProjectItemID::createNewID(0)};options.role=te::Edit::forEditing;options.numAudioTracks=0;options.defaultMasterVolumedB=0;options.editFileRetriever=[&s]{return s.projectFile;};s.edit=std::make_unique<te::Edit>(options);
 auto source=root.getChildFile("realtime-source.wav");juce::AudioBuffer<float> signal(2,48000*8);
 for(int i=0;i<signal.getNumSamples();++i)for(int c=0;c<2;++c)signal.setSample(c,i,0.001f*std::sin(juce::MathConstants<double>::twoPi*(c?733:997)*i/48000));
 juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),48000,2,32,{},0));require(writer&&writer->writeFromAudioSampleBuffer(signal,0,signal.getNumSamples()),"Source write failed");writer.reset();s.importAudio(source,0,0);
 auto pump=[](int ms){auto end=juce::Time::getMillisecondCounterHiRes()+ms;while(juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);};
 EngineCallback callback(hosted);struct StopGuard{juce::AudioIODevice& d;~StopGuard(){d.stop();d.close();}} guard{*device};
 device->start(&callback);pump(250);auto& transport=s.edit->getTransport();transport.setPosition(Session::time(0));transport.play(false);callback.phase=1;pump(1200);auto played=transport.getPosition().inSeconds();
 transport.setPosition(Session::time(144000));pump(600);auto located=transport.getPosition().inSeconds();
 callback.phase=0;transport.stop(false,false);pump(250);auto paused=transport.getPosition().inSeconds();callback.phase=2;pump(350);auto pausedEnd=transport.getPosition().inSeconds();
 callback.phase=0;transport.setLoopRange({Session::time(24000),Session::time(48000)});transport.looping=true;transport.setPosition(Session::time(24000));transport.play(false);pump(1300);auto looped=transport.getPosition().inSeconds();transport.stop(false,true);pump(100);device->stop();device->close();
 auto* report=new juce::DynamicObject;juce::var result(report);report->setProperty("actual_block",params.blockSize);report->setProperty("sample_rate",params.sampleRate);report->setProperty("played_seconds",played);report->setProperty("located_seconds",located);report->setProperty("paused_drift",pausedEnd-paused);report->setProperty("loop_position",looped);report->setProperty("play_peak",callback.playPeak.load());report->setProperty("pause_peak",callback.pausePeak.load());report->setProperty("callbacks",callback.calls.load());report->setProperty("callback_errors",callback.errors.load());report->setProperty("max_process_ms",callback.maxProcessMs.load());report->setProperty("closed",!device->isOpen());root.getChildFile("realtime-result.json").replaceWithText(juce::JSON::toString(result));
 require(played>0.8&&played<1.8,"Playback position failed");require(located>3.3&&located<4.1,"Locate failed");require(std::abs(pausedEnd-paused)<1e-6,"Paused position moved");require(looped>=0.49&&looped<=1.02,"Loop position outside range");require(callback.playPeak>0.0009f&&callback.playPeak<0.0011f&&callback.pausePeak<1e-7f,"Realtime audio or pause silence failed");require(callback.errors==0&&!device->isOpen(),"Device lifecycle failed");
}
