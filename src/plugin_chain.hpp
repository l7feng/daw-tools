#pragma once
class PluginChain {
public:
 static te::PluginList& list(Session& s,int track){require(track>=-1,"Invalid plugin track");return track<0?s.edit->getMasterPluginList():s.trackAt(track).pluginList;}
 static juce::Array<te::ExternalPlugin*> external(Session& s,int track){return list(s,track).getPluginsOfType<te::ExternalPlugin>();}
 static te::ExternalPlugin& at(Session& s,int track,int index){auto plugins=external(s,track);require(index>=0&&index<plugins.size(),"Select an external plugin");return *plugins[index];}
 static void editable(Session& s,int track){if(track>=0)require(!s.trackAt(track).isFrozen(te::Track::individualFreeze),"Unfreeze track before editing plugins");}
 static te::ExternalPlugin& add(Session& s,int track,const juce::PluginDescription& desc){
  editable(s,track);auto& chain=list(s,track);require(chain.canInsertPlugin(),"Plugin limit reached");s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(desc);
  auto item=s.edit->getPluginCache().createNewPlugin(te::ExternalPlugin::xmlTypeName,desc);auto* p=dynamic_cast<te::ExternalPlugin*>(item.get());require(p!=nullptr,"Plugin creation failed");p->initialiseFully();require(p->getAudioPluginInstance()!=nullptr,"Plugin instance failed; chain unchanged");
  s.begin("Insert external plugin");auto* fader=chain.findFirstPluginOfType<te::VolumeAndPanPlugin>();chain.insertPlugin(item,fader?chain.indexOf(fader):chain.size(),nullptr);return *p;
 }
 static void bypass(Session& s,int track,int index){editable(s,track);auto& p=at(s,track,index);s.begin("Bypass external plugin");p.setEnabled(!p.isEnabled());}
 static void remove(Session& s,int track,int index){editable(s,track);s.begin("Remove external plugin");at(s,track,index).deleteFromParent();}
 static void move(Session& s,int track,int index,int destination){
  editable(s,track);auto items=external(s,track);require(index>=0&&index<items.size()&&destination>=0&&destination<items.size(),"Invalid plugin order");if(index==destination)return;
  auto parent=items[index]->state.getParent();require(parent==items[destination]->state.getParent(),"Plugin chains differ");s.begin("Reorder external plugins");parent.moveChild(parent.indexOf(items[index]->state),parent.indexOf(items[destination]->state),&s.edit->getUndoManager());
 }
 static void repair(Session& s,int track,int index,const juce::PluginDescription& desc){
  editable(s,track);auto& old=at(s,track,index);old.initialiseFully();require(old.getAudioPluginInstance()==nullptr,"Plugin is available; repair applies to missing instances");
  auto identity=te::ExternalPlugin::create(s.engine,desc);for(auto key:{"uniqueId","uid","manufacturer"})require(old.state.getProperty(key).toString()==identity.getProperty(key).toString(),"Replacement is a different plugin identity");
  for(auto* audio:te::getAudioTracks(*s.edit))for(auto* p:audio->pluginList.getPluginsOfType<te::ExternalPlugin>())require(!p->getAudioPluginInstance()||!p->getAudioPluginInstance()->getActiveEditor(),"Close plugin editors before repair");
  for(auto* p:s.edit->getMasterPluginList().getPluginsOfType<te::ExternalPlugin>())require(!p->getAudioPluginInstance()||!p->getAudioPluginInstance()->getActiveEditor(),"Close plugin editors before repair");
  s.edit->flushState();auto snapshot=s.edit->state.createCopy();auto id=old.state.getProperty("id");bool found=false;
  std::function<void(juce::ValueTree)> visit=[&](juce::ValueTree node){if(node.getType().toString()=="PLUGIN"&&node.getProperty("id")==id){node.setProperty("filename",desc.fileOrIdentifier,nullptr);found=true;}for(auto child:node)visit(child);};visit(snapshot);require(found,"Plugin state not found");
  s.engine.getPluginManager().initialise();s.engine.getPluginManager().knownPluginList.addType(desc);
  te::Edit::Options options{s.engine,snapshot,s.edit->getProjectItemID()};options.role=te::Edit::forRendering;options.numAudioTracks=0;options.editFileRetriever=[&s]{return s.projectFile;};auto next=std::make_unique<te::Edit>(options);
  auto candidates=(track<0?next->getMasterPluginList():te::getAudioTracks(*next)[track]->pluginList).getPluginsOfType<te::ExternalPlugin>();require(index<candidates.size(),"Repair chain differs");candidates[index]->initialiseFully();require(candidates[index]->getAudioPluginInstance()!=nullptr,"Replacement failed; current edit retained");
  require(candidates[index]->state.getProperty("state")==old.state.getProperty("state"),"Repair changed stored plugin state");s.checkpoint();s.edit=std::move(next);s.edit->markAsChanged();
 }
};
class PluginScanJob {
 juce::ChildProcess process;double deadline=0;
public:
 juce::File folder;bool active=false;
 void report(const juce::String& status,const juce::String& error={}){auto* record=new juce::DynamicObject;record->setProperty("status",status);record->setProperty("exit_code",int(process.getExitCode()));record->setProperty("error",error);folder.getChildFile("scan-status.json").replaceWithText(juce::JSON::toString(juce::var(record)));}
 ~PluginScanJob(){if(active&&process.isRunning())process.kill();}
 void start(Session& s,const juce::File& path,int timeoutMs=20000){
  require(!active&&path.exists()&&path.hasFileExtension("vst3"),"Invalid plugin candidate or scan already active");require(timeoutMs>0&&timeoutMs<=300000,"Invalid scan timeout");folder=s.root.getChildFile("scan-"+juce::Uuid().toString());require(folder.createDirectory().wasOk(),"Cannot create scan directory");require(folder.getChildFile("studio-validation.marker").replaceWithText("isolated plugin scan"),"Cannot mark scan");auto* request=new juce::DynamicObject;request->setProperty("path",path.getFullPathName());require(folder.getChildFile("scan-request.json").replaceWithText(juce::JSON::toString(juce::var(request))),"Cannot write scan request");
  require(process.start(juce::StringArray{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--workspace",folder.getFullPathName(),"--scan-plugin"},0),"Cannot launch plugin scan");deadline=juce::Time::getMillisecondCounterHiRes()+timeoutMs;active=true;report("running");
 }
 void cancel(){if(active){require(!process.isRunning()||process.kill()||!process.isRunning(),"Cannot cancel plugin scan");active=false;folder.getChildFile("CANCELLED").replaceWithText("cancelled");report("cancelled");}}
 bool poll(juce::OwnedArray<juce::PluginDescription>& results){
  if(!active)return false;if(process.isRunning()){if(juce::Time::getMillisecondCounterHiRes()<deadline)return false;require(process.kill()||!process.isRunning(),"Cannot stop timed-out plugin scan");active=false;folder.getChildFile("TIMED_OUT").replaceWithText("timeout; candidate not added");report("timed_out");throw std::runtime_error("Plugin scan timed out; candidate not added");}
  active=false;if(process.getExitCode()!=0)report("failed","native scanner exited without successful result");require(process.getExitCode()==0,"Plugin scanner failed; candidate not added");auto xml=juce::XmlDocument::parse(folder.getChildFile("scan-result.xml"));require(xml&&xml->hasTagName("PLUGIN_SCAN"),"Invalid plugin scan result");
  for(auto* node:xml->getChildIterator()){auto description=std::make_unique<juce::PluginDescription>();require(description->loadFromXml(*node),"Invalid plugin description");results.add(description.release());}require(!results.isEmpty(),"No plugin found");report("complete");return true;
 }
};
