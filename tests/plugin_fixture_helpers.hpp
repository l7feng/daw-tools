#pragma once
static void persistMissingPluginFixture(Session& s,int track,int index,const juce::File& missing){
 s.save();auto id=PluginChain::at(s,track,index).state.getProperty("id");auto state=s.edit->state.createCopy();bool found=false;
 std::function<void(juce::ValueTree)> visit=[&](juce::ValueTree node){if(node.getType().toString()=="PLUGIN"&&node.getProperty("id")==id){node.setProperty("filename",missing.getFullPathName(),nullptr);found=true;}for(auto child:node)visit(child);};visit(state);require(found&&state.createXml()->writeTo(s.projectFile),"Cannot persist missing fixture");s.reopen();
}
