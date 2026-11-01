#pragma once
#include <atomic>
struct CaptureCallback final:juce::AudioIODeviceCallback {
 juce::AudioFormatWriter::ThreadedWriter* writer=nullptr;int target=240000;float peak=0;std::atomic<int> dropped{0};std::atomic<int> frames{0},calls{0},errors{0};std::atomic<bool> measuring{false};
 void audioDeviceAboutToStart(juce::AudioIODevice*)override{}
 void audioDeviceStopped()override{}
 void audioDeviceError(const juce::String&)override{++errors;}
 void audioDeviceIOCallbackWithContext(const float* const* input,int inputs,float* const* output,int outputs,int count,const juce::AudioIODeviceCallbackContext&)override{
  for(int c=0;c<outputs;++c)if(output[c])juce::FloatVectorOperations::clear(output[c],count);
  ++calls;if(!measuring.load())return;
  int at=frames.load();int n=std::min(count,target-at);
  if(n>0){if(inputs<2||!input[0]||!input[1]){++errors;return;}
   for(int c=0;c<2;++c)for(int i=0;i<n;++i)peak=std::max(peak,std::abs(input[c][i]));
   if(!writer->write(input,n))++dropped;
  }
  frames.store(at+n);
 }
};
static void deviceProbe(const juce::File& root,bool capture,int block,int seconds,const juce::String& driver){
 require(seconds>=1&&seconds<=1800,"Duration out of range");
 juce::AudioDeviceManager manager;juce::OwnedArray<juce::AudioIODeviceType> types;manager.createAudioDeviceTypes(types);
 juce::Array<juce::var> inventory;juce::AudioIODeviceType* selected=nullptr;juce::String in,out;
 for(auto* type:types){type->scanForDevices();auto* entry=new juce::DynamicObject;entry->setProperty("type",type->getTypeName());juce::Array<juce::var> ins,outs;
  for(auto name:type->getDeviceNames(true))ins.add(name);for(auto name:type->getDeviceNames(false))outs.add(name);
  entry->setProperty("inputs",ins);entry->setProperty("outputs",outs);inventory.add(entry);
  if(type->getTypeName()==driver){
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
 require(device->getCurrentSampleRate()==48000,"Device rate mismatch");CaptureCallback callback;callback.target=48000*seconds;
 juce::TimeSliceThread diskThread("Capture disk writer");diskThread.startThread();
 juce::WavAudioFormat wav;auto* raw=wav.createWriterFor(root.getChildFile("captured.wav").createOutputStream().release(),48000,2,24,{},0);require(raw!=nullptr,"Capture writer creation failed");
 auto writer=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(raw,diskThread,32768);writer->setFlushInterval(48000);callback.writer=writer.get();
 auto* report=new juce::DynamicObject;juce::var result(report);
 report->setProperty("input",in);report->setProperty("output",out);report->setProperty("driver",selected->getTypeName());
 report->setProperty("sample_rate",device->getCurrentSampleRate());report->setProperty("block_size",device->getCurrentBufferSizeSamples());
 report->setProperty("reported_input_latency",device->getInputLatencyInSamples());report->setProperty("reported_output_latency",device->getOutputLatencyInSamples());
 device->start(&callback);auto warmupEnd=juce::Time::getMillisecondCounterHiRes()+1000;
 while(juce::Time::getMillisecondCounterHiRes()<warmupEnd)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
 int startupXruns=device->getXRunCount();report->setProperty("startup_xruns",startupXruns);report->setProperty("warmup_ms",1000);callback.measuring=true;
 auto deadline=juce::Time::getMillisecondCounterHiRes()+seconds*1000.0+5000;
 auto nextReport=juce::Time::getMillisecondCounterHiRes()+1000;
 while(callback.frames.load()<callback.target&&juce::Time::getMillisecondCounterHiRes()<deadline){juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
 if(juce::Time::getMillisecondCounterHiRes()>=nextReport){root.getChildFile("capture-progress.txt").replaceWithText(juce::String(callback.frames.load()));nextReport+=1000;}}
 device->stop();int xruns=device->getXRunCount();report->setProperty("xruns",xruns);report->setProperty("measured_xruns",xruns-startupXruns);report->setProperty("stable",xruns==startupXruns);device->close();
 report->setProperty("frames",callback.frames.load());report->setProperty("callbacks",callback.calls.load());report->setProperty("callback_errors",callback.errors.load());report->setProperty("closed",!device->isOpen());report->setProperty("output_signal","digital silence");
 report->setProperty("input_peak",callback.peak);report->setProperty("dropped_blocks",callback.dropped.load());report->setProperty("fifo_frames",32768);report->setProperty("duration_seconds",seconds);writer.reset();diskThread.stopThread(5000);
 root.getChildFile("device-result.json").replaceWithText(juce::JSON::toString(result));
 require(callback.frames==callback.target&&callback.dropped==0&&callback.calls>0&&callback.errors==0&&!device->isOpen(),"Capture incomplete or device error");
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(root.getChildFile("captured.wav")));
 require(reader&&reader->lengthInSamples==callback.target&&reader->numChannels==2,"Capture WAV validation failed");
 require(xruns==startupXruns,"Input discontinuities reported; stability not passed");
}
