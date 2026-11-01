#pragma once
// This path constructs no Session/Engine or device manager.
static void scanPlugin(const juce::File& root) {
    auto request=juce::JSON::parse(root.getChildFile("scan-request.json"));
    require(request.isObject(),"Invalid scan request");
    auto path=request["path"].toString();
    require(juce::File::isAbsolutePath(path),"Plugin path must be absolute");
    juce::File file(path);require(file.exists()&&file.hasFileExtension("vst3"),"VST3 candidate missing or invalid");
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,path);
    require(!descriptions.isEmpty(),"No VST3 descriptions found");
    juce::XmlElement list("PLUGIN_SCAN");list.setAttribute("path",path);
    for(auto* description:descriptions)list.addChildElement(description->createXml().release());
    require(list.writeTo(root.getChildFile("scan-result.xml")),"Cannot write scan result");
}
