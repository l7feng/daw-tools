from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
src=r/'third_party/tracktion_engine-3.2.0/examples/TestRunner/TestRunner.h'
assert hashlib.sha256(src.read_bytes()).hexdigest() == 'b7a9b84da9600d9c8326d582abc97baeb837382e07ee73db98158469fc365e98'
s=src.read_text(encoding='utf-8')
# Only the upstream driver is adapted; all upstream test implementations stay unchanged.
s='\n'.join(line for line in s.splitlines() if '.deleteRecursively (' not in line)+'\n'
needle='    //==============================================================================\n    void removeProperty (SettingID) override {}'
replacement='    juce::File getAppPrefsFolder() override { return isolatedRoot().getChildFile ("prefs"); }\n    juce::File getAppCacheFolder() override { return isolatedRoot().getChildFile ("cache"); }\n    static juce::File isolatedRoot() {\n        auto path = juce::SystemStats::getEnvironmentVariable ("STUDIO_TEST_ROOT", {});\n        if (!juce::File::isAbsolutePath(path)) std::abort();\n        juce::File root(path);\n        if (!root.getChildFile("studio-validation.marker").existsAsFile()) std::abort();\n        return root;\n    }\n    //==============================================================================\n    void removeProperty (SettingID) override {}'
assert s.count(needle)==1
s=s.replace(needle,replacement)
# This baseline runs only core/graph tests. Engine integration is tested separately.
s='\n'.join(line for line in s.splitlines() if not any(x in line for x in ['getTestsInCategory ("tracktion_engine")', 'getTestsInCategory ("Tracktion")', 'getTestsInCategory ("Tracktion:Longer")','addFilter("test-suite", "tracktion_engine")']))+'\n'
out=r/'build/generated';out.mkdir(parents=True,exist_ok=True)
(out/'TestRunnerIsolated.h').write_text(s,encoding='utf-8')
print('Upstream driver SHA256:',hashlib.sha256(src.read_bytes()).hexdigest())
