#pragma once
#include "plugin_chain.hpp"
class HostedPluginWindow final:public juce::DocumentWindow {
 te::Plugin::Ptr keepAlive;
public:
 explicit HostedPluginWindow(te::ExternalPlugin& p):DocumentWindow(p.getName(),juce::Colour(0xff202d3c),closeButton),keepAlive(&p){
  p.initialiseFully();auto* processor=p.getAudioPluginInstance();require(processor!=nullptr,"Plugin is missing");auto* editor=processor->createEditorIfNeeded();require(editor!=nullptr,"Plugin has no editor");setUsingNativeTitleBar(true);setContentOwned(editor,true);centreWithSize(editor->getWidth(),editor->getHeight());setVisible(true);
 }
 ~HostedPluginWindow()override{clearContentComponent();}
 void closeButtonPressed()override{clearContentComponent();setVisible(false);}
};
class PluginPanel final:public juce::Component,private juce::Timer {
 Session& s;juce::ComboBox tracks,chain,candidates;juce::Label status;
 juce::TextButton scan{zh("扫描 VST3")},insert{zh("插入")},repair{zh("修复缺失")},open{zh("打开编辑器")},bypass{zh("旁通 / 启用")},remove{zh("移除")},up{zh("上移")},down{zh("下移")},cancel{zh("取消扫描")};
 std::unique_ptr<juce::FileChooser> chooser;PluginScanJob job;juce::OwnedArray<juce::PluginDescription> descriptions;std::unique_ptr<HostedPluginWindow> editor;
 int track()const{return tracks.getSelectedId()-2;}
 int selected()const{return chain.getSelectedId()-1;}
 template<class F> void action(F fn){try{editor.reset();fn();status.setText(zh("操作完成"),juce::dontSendNotification);refresh();}catch(const std::exception& e){status.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
 void refresh(){int chosen=chain.getSelectedId();chain.clear(juce::dontSendNotification);auto items=PluginChain::external(s,track());for(int i=0;i<items.size();++i){auto* p=items[i];p->initialiseFully();chain.addItem(juce::String(i+1)+" · "+p->getName()+(p->getAudioPluginInstance()==nullptr?zh(" [缺失]"):p->isEnabled()?"":zh(" [旁通]")),i+1);}chain.setSelectedId(std::clamp(chosen,1,std::max(1,items.size())),juce::dontSendNotification);}
 void timerCallback()override{if(!job.active)return;try{juce::OwnedArray<juce::PluginDescription> found;if(job.poll(found)){for(auto* description:found)addCandidate(*description);status.setText(zh("扫描完成；选择插入或修复缺失"),juce::dontSendNotification);}}catch(const std::exception& e){status.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
public:
 void addCandidate(const juce::PluginDescription& desc){
  for(int i=0;i<descriptions.size();++i)if(descriptions[i]->uniqueId==desc.uniqueId&&descriptions[i]->deprecatedUid==desc.deprecatedUid&&pluginBundle(descriptions[i]->fileOrIdentifier)==pluginBundle(desc.fileOrIdentifier)){candidates.setSelectedId(i+1);return;}
  s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(desc);descriptions.add(new juce::PluginDescription(desc));candidates.addItem(desc.name,descriptions.size());candidates.setSelectedId(descriptions.size());
 }
 void scanCandidate(const juce::File& file){job.start(s,file);}
 bool scanActive()const{return job.active;}
 int candidateCount()const{return descriptions.size();}
 explicit PluginPanel(Session& session,int initial):s(session){
  for(auto* component:std::initializer_list<juce::Component*>{&tracks,&chain,&candidates,&status,&scan,&insert,&repair,&open,&bypass,&remove,&up,&down,&cancel})addAndMakeVisible(*component);
  tracks.addItem("Master",1);auto audio=te::getAudioTracks(*s.edit);for(int i=0;i<audio.size();++i)tracks.addItem(audio[i]->getName(),i+2);tracks.setSelectedId(initial>=0&&initial<audio.size()?initial+2:1,juce::dontSendNotification);tracks.onChange=[this]{editor.reset();refresh();};chain.onChange=[this]{editor.reset();};
  scan.onClick=[this]{chooser=std::make_unique<juce::FileChooser>(zh("选择要隔离扫描的 VST3"),s.root,"*.vst3");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<PluginPanel>(this)](const juce::FileChooser& f){if(!safe||f.getResult()==juce::File())return;safe->action([safe,&f]{safe->scanCandidate(f.getResult());});});};
  cancel.onClick=[this]{action([this]{job.cancel();});};
  insert.onClick=[this]{action([this]{int i=candidates.getSelectedId()-1;require(i>=0&&i<descriptions.size(),"Scan and select a plugin first");PluginChain::add(s,track(),*descriptions[i]);});};
  repair.onClick=[this]{action([this]{int i=candidates.getSelectedId()-1;require(i>=0&&i<descriptions.size(),"Scan the matching replacement first");PluginChain::repair(s,track(),selected(),*descriptions[i]);});};
  open.onClick=[this]{action([this]{editor=std::make_unique<HostedPluginWindow>(PluginChain::at(s,track(),selected()));});};
  bypass.onClick=[this]{action([this]{PluginChain::bypass(s,track(),selected());});};remove.onClick=[this]{action([this]{PluginChain::remove(s,track(),selected());});};
  up.onClick=[this]{action([this]{int i=selected();PluginChain::move(s,track(),i,i-1);chain.setSelectedId(i,juce::dontSendNotification);});};down.onClick=[this]{action([this]{int i=selected();PluginChain::move(s,track(),i,i+1);chain.setSelectedId(i+2,juce::dontSendNotification);});};
  status.setText(zh("选择轨道或 Master；扫描明确选择的 VST3 后插入"),juce::dontSendNotification);for(auto desc:s.engine.getPluginManager().knownPluginList.getTypes())addCandidate(desc);setSize(720,300);refresh();startTimer(100);
 }
 ~PluginPanel()override{stopTimer();editor.reset();}
 void resized()override{tracks.setBounds(16,16,220,30);chain.setBounds(248,16,456,30);scan.setBounds(16,62,140,30);candidates.setBounds(168,62,300,30);insert.setBounds(480,62,100,30);repair.setBounds(592,62,112,30);open.setBounds(16,110,140,30);bypass.setBounds(168,110,140,30);up.setBounds(320,110,80,30);down.setBounds(412,110,80,30);remove.setBounds(504,110,90,30);cancel.setBounds(606,110,98,30);status.setBounds(16,160,688,110);}
};
class PluginsWindow final:public juce::DocumentWindow {
public:
 PluginsWindow(Session& s,int track):DocumentWindow(zh("插件链"),juce::Colour(0xff202d3c),closeButton){setUsingNativeTitleBar(true);setContentOwned(new PluginPanel(s,track),true);centreWithSize(720,300);setVisible(true);}
 void closeButtonPressed()override{setVisible(false);}
};
