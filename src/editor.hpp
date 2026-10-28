#pragma once
#include <map>
inline juce::String zh(const char* text){return juce::String::fromUTF8(text);}
class Timeline final:public juce::Component,private juce::ChangeListener {
public:
    Session& session;
    int selected=-1,selectedTrack=0;
    int64_t cursor=0,grid=480;
    double scale=120;
    std::function<void()> changed;
    std::function<void(std::function<void()>)> execute;
    juce::AudioFormatManager formats;juce::AudioThumbnailCache cache{24};
    std::map<juce::String,std::unique_ptr<juce::AudioThumbnail>> thumbnails;
    explicit Timeline(Session& s):session(s){formats.registerBasicFormats();}
    ~Timeline() override {for(auto& item:thumbnails)item.second->removeChangeListener(this);}
    int x(int64_t sample) const {return 160+int(std::llround(double(sample)*scale/48000.0));}
    int64_t at(float px) const {auto n=std::max<int64_t>(0,std::llround((px-160)*48000.0/scale));return grid>1?std::llround(double(n)/grid)*grid:n;}
    te::Clip* selection()const {auto clips=session.clips();return selected>=0&&selected<clips.size()?clips[selected]:nullptr;}
    void refresh(){setSize(std::max(960,x(std::max<int64_t>(480000,Session::samples(tracktion::TimePosition::fromSeconds(session.edit->getLength().inSeconds()))+48000))),std::max(400,42+te::getAudioTracks(*session.edit).size()*90));repaint();}
    juce::Rectangle<int> bounds(te::Clip& c,int row)const {auto p=c.getPosition();return {x(Session::samples(p.getStart())),44+row*90,std::max(3,x(Session::samples(p.getEnd()))-x(Session::samples(p.getStart()))),72};}
    void paint(juce::Graphics& g)override {
        g.fillAll(juce::Colour(0xff151b24));auto area=g.getClipBounds();
        auto step=std::max(1.0/48000.0,std::pow(10.0,std::floor(std::log10(90.0/scale))));
        for(double t=std::max(0.0,std::floor((area.getX()-160)/scale/step)*step);160+t*scale<area.getRight();t+=step){int px=160+int(t*scale);g.setColour(juce::Colour(0xff344151));g.drawVerticalLine(px,30,float(getHeight()));g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(t,step<0.001?5:3),px+3,4,85,22,juce::Justification::left);}
        auto tracks=te::getAudioTracks(*session.edit);int index=0;
        for(int row=0;row<tracks.size();++row){g.setColour(row==selectedTrack?juce::Colour(0xff35465a):juce::Colour(0xff232d39));g.fillRect(0,40+row*90,158,88);g.setColour(juce::Colours::white);g.drawText(tracks[row]->getName(),12,48+row*90,140,26,juce::Justification::left);
            for(auto* clip:tracks[row]->getClips()){
                auto box=bounds(*clip,row);g.setColour(index==selected?juce::Colour(0xff3c8f9a):juce::Colour(0xff34506b));g.fillRoundedRectangle(box.toFloat(),4);g.setColour(juce::Colours::white);g.drawText(clip->getName(),box.reduced(5).removeFromTop(20),juce::Justification::left);
                if(auto* audio=dynamic_cast<te::WaveAudioClip*>(clip)){
                    auto file=audio->getAudioFile().getFile();auto key=file.getFullPathName();
                    if(!thumbnails.contains(key)&&file.existsAsFile()){auto thumb=std::make_unique<juce::AudioThumbnail>(256,formats,cache);thumb->addChangeListener(this);thumb->setSource(new juce::FileInputSource(file));thumbnails[key]=std::move(thumb);}
                    if(auto it=thumbnails.find(key);it!=thumbnails.end()){g.setColour(juce::Colour(0xffb5dfd6));auto wave=box.reduced(4);wave.removeFromTop(22);auto p=clip->getPosition();it->second->drawChannels(g,wave,p.getOffset().inSeconds(),p.getOffset().inSeconds()+p.getLength().inSeconds(),0.85f);}
                }
                ++index;
            }
        }
        if(tracks.isEmpty()){g.setColour(juce::Colours::lightgrey);g.drawText(zh("空白工程：添加轨道或拖入 WAV/BWF 文件"),180,90,600,30,juce::Justification::left);}
        g.setColour(juce::Colour(0xffe7b46c));g.drawVerticalLine(x(cursor),30,float(getHeight()));
    }
    void mouseDown(const juce::MouseEvent& e)override {
        selectedTrack=std::clamp((e.y-40)/90,0,std::max(0,te::getAudioTracks(*session.edit).size()-1));cursor=at(e.position.x);selected=-1;
        auto clips=session.clips();for(int i=clips.size();--i>=0;)if(clips[i]->getTrack()==te::getAudioTracks(*session.edit)[selectedTrack]&&bounds(*clips[i],selectedTrack).contains(e.getPosition())){selected=i;break;}
        dragX=e.x;if(auto* c=selection())dragStart=Session::samples(c->getPosition().getStart());
        repaint();if(changed)changed();
    }
    void mouseUp(const juce::MouseEvent& e)override {
        if(auto* c=selection();c&&std::abs(e.x-dragX)>3){auto target=std::max<int64_t>(0,dragStart+std::llround((e.x-dragX)*48000.0/scale));if(grid>1)target=std::llround(double(target)/grid)*grid;execute([this,c,target]{session.move(*c,target);});}
    }
private:
    int dragX=0;int64_t dragStart=0;
    void changeListenerCallback(juce::ChangeBroadcaster*)override{repaint();}
};
class MainPanel final:public juce::Component,public juce::FileDragAndDropTarget,private juce::Timer {
public:
    enum Command {addTrack,importFile,saveProject,openProject,saveCopy,undoEdit,redoEdit,splitClip,copyClip,removeClip,moveClip,trimClip,envelope,exportWave,saveUserTemplate,openUserTemplate,relinkMedia,crossfadeClips,recovery};
    ExportJob exportJob;juce::TextButton cancelExport{zh("取消导出")};juce::ComboBox exportDepth;
    Session& session;Timeline timeline;juce::Viewport viewport;
    juce::Label title,status,help;
    juce::TextEditor start,end,cursor,gain,fadeIn,fadeOut;
    juce::TextEditor trackDb,trackPan,masterDb,sendDb;
    juce::ToggleButton mute{zh("静音")},solo{zh("独奏")},preSend{zh("推子前发送")};
    juce::ComboBox outputTrack;juce::Label mixHelp;
    juce::TextButton applyMix{zh("应用混音")},busButton{zh("添加总线")},routeButton{zh("设置输出")},sendButton{zh("添加发送")};
    juce::Slider zoom;juce::ToggleButton snap{zh("吸附 10ms")};
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::unique_ptr<juce::FileChooser> chooser;
    explicit MainPanel(Session& s):session(s),timeline(s){
        setWantsKeyboardFocus(true);viewport.setViewedComponent(&timeline,false);addAndMakeVisible(viewport);
        title.setText(zh("Studio Workbench · 编辑工作站 0.1.0"),juce::dontSendNotification);title.setFont(juce::FontOptions(23));addAndMakeVisible(title);addAndMakeVisible(status);addAndMakeVisible(help);
        const std::pair<const char*,Command> specs[]={{"添加轨道",addTrack},{"导入音频",importFile},{"保存",saveProject},{"打开",openProject},{"另存为",saveCopy},{"撤销",undoEdit},{"重做",redoEdit},{"切分",splitClip},{"复制",copyClip},{"删除片段",removeClip},{"移动",moveClip},{"裁剪",trimClip},{"应用增益/淡化",envelope},{"导出 WAV",exportWave},{"保存自定模板",saveUserTemplate},{"从模板新建",openUserTemplate},{"重定位素材",relinkMedia},{"交叉淡化",crossfadeClips},{"保存恢复点",recovery}};
        for(auto [label,command]:specs){auto b=std::make_unique<juce::TextButton>(zh(label));b->setWantsKeyboardFocus(false);b->onClick=[this,command]{perform(command);};addAndMakeVisible(*b);buttons.push_back(std::move(b));}
        for(auto* field:{&start,&end,&cursor,&gain,&fadeIn,&fadeOut}){field->setInputRestrictions(18,"0123456789.-");addAndMakeVisible(field);}
        start.setText("0");end.setText("48000");cursor.setText("0");gain.setText("0");fadeIn.setText("0");fadeOut.setText("0");
        help.setText(zh("起点 / 终点 / 光标：48kHz 采样位置     增益：dB     淡入 / 淡出：采样数；导出使用起点至终点"),juce::dontSendNotification);
        zoom.setRange(20,48000,1);zoom.setSkewFactorFromMidPoint(240);zoom.setValue(120);zoom.setSliderStyle(juce::Slider::LinearHorizontal);zoom.setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24);zoom.onValueChange=[this]{timeline.scale=zoom.getValue();timeline.refresh();};addAndMakeVisible(zoom);
        snap.setToggleState(true,juce::dontSendNotification);snap.onClick=[this]{timeline.grid=snap.getToggleState()?480:1;};addAndMakeVisible(snap);
        cursor.onReturnKey=[this]{timeline.cursor=cursor.getText().getLargeIntValue();timeline.repaint();};
        timeline.changed=[this]{grabKeyboardFocus();syncInspector();};timeline.execute=[this](auto fn){action(fn);};
        for(auto* field:{&trackDb,&trackPan,&masterDb,&sendDb}){field->setInputRestrictions(12,"0123456789.-");field->setText("0");addAndMakeVisible(field);}
        for(auto* control:std::initializer_list<juce::Component*>{&mute,&solo,&preSend,&outputTrack,&mixHelp,&applyMix,&busButton,&routeButton,&sendButton})addAndMakeVisible(*control);
        mixHelp.setText(zh("选中轨道：推子 dB / 声像 -1..1 / Master dB / 发送 dB"),juce::dontSendNotification);
        applyMix.onClick=[this]{action([this]{auto master=masterDb.getText().getDoubleValue();require(std::isfinite(master)&&master>=-120&&master<=24,"Invalid master volume");session.setMix(timeline.selectedTrack,float(trackDb.getText().getDoubleValue()),float(trackPan.getText().getDoubleValue()),mute.getToggleState(),solo.getToggleState());session.setMaster(float(masterDb.getText().getDoubleValue()));});};
        busButton.onClick=[this]{action([this]{timeline.selectedTrack=session.addBus();syncMixer();});};
        routeButton.onClick=[this]{action([this]{session.route(timeline.selectedTrack,outputTrack.getSelectedId()-2);});};
        sendButton.onClick=[this]{action([this]{session.addSend(timeline.selectedTrack,outputTrack.getSelectedId()-2,float(sendDb.getText().getDoubleValue()),preSend.getToggleState());});};
        exportDepth.addItem("PCM 24-bit",24);exportDepth.addItem("Float 32-bit",32);exportDepth.setSelectedId(24);addAndMakeVisible(exportDepth);addAndMakeVisible(cancelExport);
        cancelExport.onClick=[this]{action([this]{exportJob.cancel();});};
        setSize(1180,760);refresh();startTimer(200);
    }
    void action(std::function<void()> fn){try{fn();status.setText(zh("操作完成 · 离线模式，音频设备未打开"),juce::dontSendNotification);refresh();}catch(const std::exception& e){status.setText(zh("未执行：")+juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
    void refresh(){timeline.refresh();syncMixer();auto missing=session.missingMedia();if(!missing.isEmpty())status.setText(zh("缺失素材：")+juce::String(missing.size())+zh(" 个；请选中片段并重定位素材"),juce::dontSendNotification);repaint();}
    void syncInspector(){syncMixer();cursor.setText(juce::String(timeline.cursor));if(auto* c=timeline.selection()){start.setText(juce::String(Session::samples(c->getPosition().getStart())));end.setText(juce::String(Session::samples(c->getPosition().getEnd())));if(auto* a=dynamic_cast<te::WaveAudioClip*>(c)){gain.setText(juce::String(a->getGainDB()));fadeIn.setText(juce::String(std::llround(a->getFadeIn().inSeconds()*48000)));fadeOut.setText(juce::String(std::llround(a->getFadeOut().inSeconds()*48000)));}}}
    void syncMixer(){
        auto tracks=te::getAudioTracks(*session.edit);outputTrack.clear(juce::dontSendNotification);outputTrack.addItem("Master",1);
        for(int i=0;i<tracks.size();++i)outputTrack.addItem(tracks[i]->getName(),i+2);
        if(timeline.selectedTrack>=0&&timeline.selectedTrack<tracks.size()){
            auto* t=tracks[timeline.selectedTrack];auto* v=t->getVolumePlugin();if(v){trackDb.setText(juce::String(v->getVolumeDb(),2));trackPan.setText(juce::String(v->getPan(),3));}
            mute.setToggleState(t->isMuted(false),juce::dontSendNotification);solo.setToggleState(t->isSolo(false),juce::dontSendNotification);
            auto* target=t->getOutput().getDestinationTrack();outputTrack.setSelectedId(target?tracks.indexOf(target)+2:1,juce::dontSendNotification);
        }else outputTrack.setSelectedId(1,juce::dontSendNotification);
        masterDb.setText(juce::String(session.edit->getMasterVolumePlugin()->getVolumeDb(),2));
    }
    void choose(Command command){

        auto mode=(command==saveCopy||command==exportWave||command==saveUserTemplate)?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode;
        auto pattern=(command==importFile||command==relinkMedia)?"*.wav;*.bwf;*.aif;*.aiff;*.flac":command==exportWave?"*.wav":(command==saveUserTemplate||command==openUserTemplate)?"*.swtemplate":"*.tracktionedit";
        chooser=std::make_unique<juce::FileChooser>(zh("选择文件"),session.root,pattern);
        chooser->launchAsync(mode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<MainPanel>(this),command](const juce::FileChooser& f){if(!safe)return;auto file=f.getResult();if(file==juce::File())return;safe->action([safe,file,command]{
            if(command==importFile)safe->import(file);
            if(command==saveUserTemplate)safe->session.saveTemplate(file.withFileExtension("swtemplate"));
            if(command==openUserTemplate){require(!safe->session.edit->hasChangedSinceSaved(),"Save current project before template creation");safe->session.loadTemplate(file);safe->timeline.selected=-1;safe->timeline.selectedTrack=0;}
            if(command==relinkMedia){auto* audio=dynamic_cast<te::WaveAudioClip*>(safe->timeline.selection());require(audio!=nullptr,"Select an audio clip");safe->session.relink(*audio,file);}
            if(command==saveCopy)safe->session.saveAs(file.withFileExtension("tracktionedit"));
            if(command==openProject){require(!safe->session.edit->hasChangedSinceSaved(),"Save the current project before opening another");safe->session.openProject(file);safe->timeline.selected=-1;safe->timeline.selectedTrack=0;}
            if(command==exportWave)safe->exportJob.start(safe->session,file.withFileExtension("wav"),safe->start.getText().getLargeIntValue(),safe->end.getText().getLargeIntValue(),safe->exportDepth.getSelectedId());
        });});
    }
    void import(const juce::File& f){int track=std::min(timeline.selectedTrack,te::getAudioTracks(*session.edit).size());auto* imported=session.importAudio(f,track,timeline.cursor);timeline.selected=session.clips().indexOf(imported);syncInspector();}
    void perform(Command command){
        if(command==importFile||command==openProject||command==saveCopy||command==exportWave||command==saveUserTemplate||command==openUserTemplate||command==relinkMedia){choose(command);return;}
        action([this,command]{
            if(command==addTrack){session.begin("Add track");session.addTrack();return;}
            if(command==saveProject){session.save();return;}
            if(command==undoEdit){session.undo();timeline.selected=-1;return;}
            if(command==redoEdit){session.redo();timeline.selected=-1;return;}
            if(command==recovery){session.checkpoint();return;}
            auto* c=timeline.selection();require(c!=nullptr,"Select a clip first");
            auto pos=cursor.getText().getLargeIntValue();
            if(command==splitClip)session.split(*c,pos);
            if(command==copyClip)session.duplicate(*c,pos);
            if(command==removeClip){session.erase(*c);timeline.selected=-1;}
            if(command==moveClip)session.move(*c,start.getText().getLargeIntValue());
            if(command==trimClip)session.trim(*c,start.getText().getLargeIntValue(),end.getText().getLargeIntValue());
            if(command==envelope){auto* audio=dynamic_cast<te::WaveAudioClip*>(c);require(audio!=nullptr,"Select audio clip");session.setEnvelope(*audio,float(gain.getText().getDoubleValue()),fadeIn.getText().getLargeIntValue(),fadeOut.getText().getLargeIntValue());}
            if(command==crossfadeClips){auto* left=dynamic_cast<te::WaveAudioClip*>(c);require(left!=nullptr,"Select left audio clip");te::WaveAudioClip* right=nullptr;for(auto* item:session.clips())if(item!=left&&item->getTrack()==left->getTrack()&&item->getPosition().getStart()>left->getPosition().getStart()&&item->getPosition().getStart()<left->getPosition().getEnd()){right=dynamic_cast<te::WaveAudioClip*>(item);break;}require(right!=nullptr,"No overlapping right clip");session.crossfade(*left,*right);}
            syncInspector();
        });
    }
    bool keyPressed(const juce::KeyPress& key)override{
        if(key.getModifiers().isCommandDown()){
            if(key.getKeyCode()=='Z'){perform(key.getModifiers().isShiftDown()?redoEdit:undoEdit);return true;}
            if(key.getKeyCode()=='Y'){perform(redoEdit);return true;}
            if(key.getKeyCode()=='S'){perform(saveProject);return true;}
        }
        if(key.getKeyCode()==juce::KeyPress::deleteKey){perform(removeClip);return true;}return false;
    }
    bool isInterestedInFileDrag(const juce::StringArray& files)override{return !files.isEmpty();}
    void filesDropped(const juce::StringArray& files,int,int)override{action([this,files]{for(auto& f:files)import(juce::File(f));});}
    int64_t lastSnapshot=0;
    int snapshotTicks=0;
    void timerCallback()override{
        if(exportJob.active){try{juce::String result;if(exportJob.poll(result))status.setText(result=="cancelled"?zh("导出已取消；未生成目标文件"):zh("导出完成：")+result,juce::dontSendNotification);else status.setText(zh("后台导出：")+juce::String(int(exportJob.progress()*100))+"%",juce::dontSendNotification);}catch(const std::exception& e){status.setText(zh("导出未完成：")+juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
        if(++snapshotTicks<300)return;snapshotTicks=0;
        auto changed=session.edit->getTimeOfLastChange().toMilliseconds();if(session.edit->hasChangedSinceSaved()&&changed>lastSnapshot){action([this,changed]{session.checkpoint();lastSnapshot=changed;});}
    }
    void resized()override{
        title.setBounds(20,12,getWidth()-40,34);status.setBounds(20,48,getWidth()-40,28);
        for(size_t i=0;i<buttons.size();++i)buttons[i]->setBounds(20+int(i%7)*150,82+int(i/7)*38,140,30);
        help.setBounds(20,205,getWidth()-40,24);int x=20;for(auto* field:{&start,&end,&cursor,&gain,&fadeIn,&fadeOut}){field->setBounds(x,233,140,28);x+=150;}
        snap.setBounds(20,269,160,30);zoom.setBounds(200,269,350,30);mixHelp.setBounds(20,305,600,24);int mx=20;for(auto* field:{&trackDb,&trackPan,&masterDb,&sendDb}){field->setBounds(mx,333,95,28);mx+=105;}
        mute.setBounds(450,333,75,28);solo.setBounds(530,333,75,28);applyMix.setBounds(615,333,110,28);busButton.setBounds(740,333,110,28);
        outputTrack.setBounds(20,371,210,28);routeButton.setBounds(240,371,110,28);preSend.setBounds(370,371,150,28);sendButton.setBounds(530,371,110,28);
        exportDepth.setBounds(660,371,150,28);cancelExport.setBounds(825,371,110,28);
        viewport.setBounds(20,415,getWidth()-40,getHeight()-435);
    }
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff171f2a));}
};
