#include "session.hpp"
#include "../tests/offline_cases.hpp"
#include "../tests/editor_cases.hpp"
#include "../tests/mixer_cases.hpp"
#include "export_job.hpp"
#include "../tests/export_cases.hpp"
#include "editor.hpp"
class Window final:public juce::DocumentWindow {
    Session& owner;bool confirming=false;
public:
    Window(Session& s):DocumentWindow("Studio Workbench 0.1.0",juce::Colour(0xff171d26),allButtons),owner(s){
        setUsingNativeTitleBar(true);setContentOwned(new MainPanel(s),true);setResizable(true,true);setResizeLimits(1100,700,4096,2160);centreWithSize(1180,760);setVisible(true);
    }
    void closeButtonPressed() override {
        if(!owner.edit->hasChangedSinceSaved()){juce::JUCEApplication::getInstance()->systemRequestedQuit();return;}
        if(confirming)return;confirming=true;
        auto options=juce::MessageBoxOptions().withTitle(zh("保存工程更改？")).withMessage(zh("关闭前可保存当前编辑。")).withButton(zh("保存并关闭")).withButton(zh("放弃更改")).withButton(zh("取消"));
        juce::AlertWindow::showAsync(options,[safe=juce::Component::SafePointer<Window>(this)](int result){if(!safe)return;safe->confirming=false;if(result==1){try{safe->owner.save();}catch(const std::exception&){return;}}if(result==1||result==2)juce::JUCEApplication::getInstance()->systemRequestedQuit();});
    }
};
class App final:public juce::JUCEApplication {
public:
    std::unique_ptr<Session> session;std::unique_ptr<Window> window;
    const juce::String getApplicationName() override{return "StudioWorkbenchValidation";}
    const juce::String getApplicationVersion() override{return "0.1.0";}
    void initialise(const juce::String& line) override {
        auto args=juce::StringArray::fromTokens(line,false);int index=args.indexOf("--workspace");
        if(index<0 || index+1>=args.size() || !juce::File::isAbsolutePath(args[index+1])) {setApplicationReturnValue(2);quit();return;}
        juce::File root(args[index+1]);
        if(!root.isAChildOf(juce::File(STUDIO_SOURCE_ROOT).getChildFile("build")) || !root.getChildFile("studio-validation.marker").existsAsFile()) {setApplicationReturnValue(2);quit();return;}
        try {
            session=std::make_unique<Session>(root);
            if(args.contains("--render-worker")){renderWorker(*session);quit();}
            else if(args.contains("--self-test")) {offlineTest(*session);editorTest(*session);crossfadeTest(*session);mixerTest(*session);exportTest(*session);root.getChildFile("PASS.txt").replaceWithText("Offline signal and persistence passed; no audio devices opened.");quit();}
            else window=std::make_unique<Window>(*session);
        } catch(const std::exception& e) {root.getChildFile("FAIL.txt").replaceWithText(e.what());setApplicationReturnValue(1);quit();}
    }
    void shutdown() override {window.reset();session.reset();}
};
START_JUCE_APPLICATION(App)
