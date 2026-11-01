#pragma once
#include <atomic>
struct CaptureCallback final:juce::AudioIODeviceCallback {
 juce::AudioBuffer<float> data{2,240000};std::atomic<int> frames{0},calls{0},errors{0};std::atomic<bool> measuring{false};
 void audioDeviceAboutToStart(juce::AudioIODevice*)override{data.clear();}
 void audioDeviceStopped()override{}
 void audioDeviceError(const juce::String&)override{++errors;}
 void audioDeviceIOCallbackWithContext(const float* const* input,int inputs,float* const* output,int outputs,int count,const juce::AudioIODeviceCallbackContext&)override{
  for(int c=0;c<outputs;++c)if(output[c])juce::FloatVectorOperations::clear(output[c],count);
  ++calls;if(!measuring.load())return;
  int at=frames.load();int n=std::min(count,240000-at);
  for(int c=0;c<std::min(inputs,2);++c)if(input[c]&&n>0)data.copyFrom(c,at,input[c],n);
  frames.store(at+n);
 }
};
static void deviceProbe(const juce::File& root,bool capture,int block){
 juce::AudioDeviceManager manager;juce::OwnedArray<juce::AudioIODeviceType> types;manager.createAudioDeviceTypes(types);
 juce::Array<juce::var> inventory;juce::AudioIODeviceType* selected=nullptr;juce::String in,out;
 for(auto* type:types){type->scanForDevices();auto* entry=new juce::DynamicObject;entry->setProperty("type",type->getTypeName());juce::Array<juce::var> ins,outs;
  for(auto name:type->getDeviceNames(true))ins.add(name);for(auto name:type->getDeviceNames(false))outs.add(name);
  entry->setProperty("inputs",ins);entry->setProperty("outputs",outs);inventory.add(entry);
  if(type->getTypeName()=="Windows Audio"){
   for(auto name:type->getDeviceNames(true))if(name.containsIgnoreCase("UR22C")&&name.startsWith("Input 1/2"))in=name;
   for(auto name:type->getDeviceNames(false))if(name.containsIgnoreCase("UR22C")&&name.startsWith("DAW ("))out=name;
   selected=type;
  }
 }
 root.getChildFile("devices.json").replaceWithText(juce::JSON::toString(inventory));if(!capture)return;
 require(selected&&in.isNotEmpty()&&out.isNotEmpty(),"Explicit UR22C shared input/output not found; no fallback opened");
 std::unique_ptr<juce::AudioIODevice> device(selected->createDevice(out,in));require(device!=nullptr,"Cannot create UR22C device");
 juce::BigInteger channels;channels.setRange(0,2,true);auto error=device->open(channels,channels,48000,block);
 if(error.isNotEmpty())throw std::runtime_error(error.toStdString());
 require(device->getCurrentSampleRate()==48000,"Device rate mismatch");CaptureCallback callback;
 auto* report=new juce::DynamicObject;juce::var result(report);
 report->setProperty("input",in);report->setProperty("output",out);report->setProperty("driver",selected->getTypeName());
 report->setProperty("sample_rate",device->getCurrentSampleRate());report->setProperty("block_size",device->getCurrentBufferSizeSamples());
 report->setProperty("reported_input_latency",device->getInputLatencyInSamples());report->setProperty("reported_output_latency",device->getOutputLatencyInSamples());
 device->start(&callback);auto warmupEnd=juce::Time::getMillisecondCounterHiRes()+1000;
 while(juce::Time::getMillisecondCounterHiRes()<warmupEnd)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
 int startupXruns=device->getXRunCount();report->setProperty("startup_xruns",startupXruns);report->setProperty("warmup_ms",1000);callback.measuring=true;
 auto deadline=juce::Time::getMillisecondCounterHiRes()+8000;
 while(callback.frames.load()<240000&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
 device->stop();int xruns=device->getXRunCount();report->setProperty("xruns",xruns);report->setProperty("measured_xruns",xruns-startupXruns);report->setProperty("stable",xruns==startupXruns);device->close();
 report->setProperty("frames",callback.frames.load());report->setProperty("callbacks",callback.calls.load());report->setProperty("callback_errors",callback.errors.load());report->setProperty("closed",!device->isOpen());report->setProperty("output_signal","digital silence");
 float peak=callback.data.getMagnitude(0,240000);report->setProperty("input_peak",peak);
 juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(root.getChildFile("captured.wav").createOutputStream().release(),48000,2,24,{},0));
 require(writer&&writer->writeFromAudioSampleBuffer(callback.data,0,callback.frames.load()),"Capture file write failed");writer.reset();
 root.getChildFile("device-result.json").replaceWithText(juce::JSON::toString(result));
 require(callback.frames==240000&&callback.calls>0&&callback.errors==0&&!device->isOpen(),"Capture incomplete or device error");
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(root.getChildFile("captured.wav")));
 require(reader&&reader->lengthInSamples==240000&&reader->numChannels==2,"Capture WAV validation failed");
 require(xruns==startupXruns,"Input discontinuities reported; stability not passed");
}
