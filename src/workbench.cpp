#include "session.hpp"
#include "plugin_scan.hpp"
#include "../tests/offline_cases.hpp"
#include "../tests/editor_cases.hpp"
#include "../tests/mixer_cases.hpp"
#include "export_job.hpp"
#include "stem_job.hpp"
#include "../tests/export_cases.hpp"
#include "editor.hpp"
#include "../tests/plugin_ui_cases.hpp"
#include "../tests/workspace_cases.hpp"
#include "../tests/interaction_cases.hpp"
#include "../tests/backend/cold_projects.hpp"
class Window final:public juce::DocumentWindow {
    Session& owner;bool confirming=false;
public:
    Window(Session& s,bool keepLayout=true):DocumentWindow("DAW Tools 0.1.6",juce::Colour(0xff171d26),allButtons),owner(s){
        setUsingNativeTitleBar(true);setContentOwned(new MainPanel(s,keepLayout),true);setResizable(true,true);setResizeLimits(1240,850,4096,2160);centreWithSize(1600,960);setVisible(true);
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
    const juce::String getApplicationVersion() override{return "0.1.6";}
    void initialise(const juce::String& line) override {
        auto args=juce::StringArray::fromTokens(line,false);int index=args.indexOf("--workspace");
        if(index<0 || index+1>=args.size() || !juce::File::isAbsolutePath(args[index+1])) {setApplicationReturnValue(2);quit();return;}
        juce::File root(args[index+1]);
        if(!root.isAChildOf(juce::File(STUDIO_SOURCE_ROOT).getChildFile("build")) || !root.getChildFile("studio-validation.marker").existsAsFile()) {setApplicationReturnValue(2);quit();return;}
        try {
            if(args.contains("--scan-plugin")){scanPlugin(root);quit();return;}
            session=std::make_unique<Session>(root);
            if(args.contains("--cold-project-test")){coldProjectWorker(*session);quit();}
            else if(args.contains("--interaction-ui-test")){interactionBoundaryTest(*session);quit();}
            else if(args.contains("--workspace-ui-test")){workspaceUiTest(*session);quit();}
            else if(args.contains("--plugin-ui-test")){pluginUiTest(*session);quit();}
            else if(args.contains("--ui-snapshot")){MainPanel panel(*session);panel.setSize(1600,900);auto shot=panel.createComponentSnapshot(panel.getLocalBounds());juce::PNGImageFormat png;auto stream=root.getChildFile("gui-preview.png").createOutputStream();require(stream&&png.writeImageToStream(shot,*stream),"GUI snapshot failed");quit();}
            else if(args.contains("--stem-worker")){stemWorker(*session);quit();}
            else if(args.contains("--render-worker")){renderWorker(*session);quit();}
            else if(args.contains("--self-test")) {offlineTest(*session);editorTest(*session);crossfadeTest(*session);mixerTest(*session);exportTest(*session);root.getChildFile("PASS.txt").replaceWithText("Offline signal and persistence passed; no audio devices opened.");quit();}
            else {if(args.contains("--gui-input-probe")&&root.getChildFile("probe.tracktionedit").existsAsFile())session->openProject(root.getChildFile("probe.tracktionedit"));window=std::make_unique<Window>(*session,!args.contains("--gui-input-probe"));if(args.contains("--gui-input-probe"))if(auto* panel=dynamic_cast<MainPanel*>(window->getContentComponent()))panel->inputProbe=true;}
        } catch(const std::exception& e) {root.getChildFile("FAIL.txt").replaceWithText(e.what());setApplicationReturnValue(1);quit();}
    }
    void shutdown() override {window.reset();session.reset();}
};
START_JUCE_APPLICATION(App)
