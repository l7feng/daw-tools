#pragma once
#include <map>
#include <array>
#include <charconv>
#include "i18n.hpp"
// 界面文字统一入口：中文为源，英文按语言表查；调用处不再直接构造字符串。
inline juce::String zh(const char* text){return i18n::tr(text);}
#include "plugin_panel.hpp"
// Some Windows accessibility/remote input delivers C0 editing characters without scan codes.
// Keep Tab, Backspace, Return and Escape unchanged; normal physical shortcuts already pass through.
inline juce::KeyPress editInputKey(const juce::KeyPress& key){
#if JUCE_WINDOWS
 const int code=key.getKeyCode();if(code==1||code==3||code==15||code==19||code==22||code==24||code==25||code==26)return juce::KeyPress('A'+code-1,key.getModifiers().withFlags(juce::ModifierKeys::ctrlModifier),0);
#endif
 return key;
}
inline int64_t editSampleValue(const juce::TextEditor& field){auto text=field.getText().trim();auto utf=text.toStdString();int64_t value=0;auto result=std::from_chars(utf.data(),utf.data()+utf.size(),value);require(!utf.empty()&&result.ec==std::errc{}&&result.ptr==utf.data()+utf.size()&&value>=0,"Sample position must be a non-negative integer");return value;}
inline double editNumberValue(const juce::TextEditor& field){auto utf=field.getText().trim().toStdString();double value=0;auto result=std::from_chars(utf.data(),utf.data()+utf.size(),value);require(!utf.empty()&&result.ec==std::errc{}&&result.ptr==utf.data()+utf.size()&&std::isfinite(value),"Enter a valid finite number");return value;}
class EditTextEditor final:public juce::TextEditor {public:bool keyPressed(const juce::KeyPress& key)override{return juce::TextEditor::keyPressed(editInputKey(key));}};
namespace EditStyle {
static juce::Colour bg{0xff252729},panel{0xff303335},edge{0xff141719},text{0xffd8dadd},muted{0xff9ba2a5},accent{0xff2ba4d4},green{0xff47e78a};
static juce::Colour insertion{0xff1f6f8b},rowOdd{0xff2c3032},rowEven{0xff303436},rowSelected{0xff35434a},ruler{0xff373c3f};
// 布局尺寸：单一来源，改一处即全局生效。行高可运行时调整（见 MainPanel::setRowHeight）。
static int topHeight=88,rulerHeight=30,rowHeight=64,controlWidth=340,statusHeight=26;
static constexpr int rowHeightMin=44,rowHeightMax=140,rowHeightStep=8;
static juce::Colour trackColour(int index){const juce::uint32 colours[]={0xff4d9fd1,0xff39b6bc,0xffab75d3,0xffc6b850,0xff5bb878,0xffce7a65};return juce::Colour(colours[unsigned(index)%6]);}
static juce::String clock(int64_t n){auto ms=std::max<int64_t>(0,n)*1000/48000;return juce::String(ms/60000)+":"+juce::String((ms/1000)%60).paddedLeft('0',2)+"."+juce::String(ms%1000).paddedLeft('0',3);}
static juce::String timecode(int64_t n){auto f=std::max<int64_t>(0,n)*30/48000;return juce::String(f/108000).paddedLeft('0',2)+":"+juce::String((f/1800)%60).paddedLeft('0',2)+":"+juce::String((f/30)%60).paddedLeft('0',2)+":"+juce::String(f%30).paddedLeft('0',2);}
class Look final:public juce::LookAndFeel_V4 {
public:
 Look(){setColour(juce::PopupMenu::backgroundColourId,bg);setColour(juce::PopupMenu::textColourId,text);setColour(juce::PopupMenu::highlightedBackgroundColourId,accent.darker(0.4f));setColour(juce::ComboBox::backgroundColourId,panel);setColour(juce::ComboBox::textColourId,text);setColour(juce::ComboBox::outlineColourId,edge);setColour(juce::TextButton::textColourOffId,text);setColour(juce::TextButton::textColourOnId,juce::Colours::white);setColour(juce::Label::textColourId,text);setColour(juce::Slider::thumbColourId,accent);setColour(juce::Slider::trackColourId,accent);setColour(juce::Slider::rotarySliderFillColourId,accent);setColour(juce::Slider::rotarySliderOutlineColourId,edge.brighter(0.35f));setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xff61676b));}
 void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down)override{auto box=b.getLocalBounds().toFloat().reduced(0.5f);auto c=b.getToggleState()?accent.darker(0.35f):panel.brighter(over?0.15f:0.03f);if(down)c=c.darker();g.setColour(c.withAlpha(b.isEnabled()?1.0f:0.4f));g.fillRect(box);g.setColour(edge);g.drawRect(box,1);}
 juce::Font getTextButtonFont(juce::TextButton& b,int h)override{const auto id=b.getComponentID();if(id.startsWith("ins-slot"))return juce::Font(juce::FontOptions(9.0f));if(id.startsWith("transport"))return juce::Font(juce::FontOptions(11.0f));
if(id.startsWith("cmd"))return juce::Font(juce::FontOptions(b.getWidth()<70?9.5f:(b.getWidth()<80?11.0f:12.0f)));return juce::Font(juce::FontOptions(float(std::min(13,h-5))));}
 // 小尺寸旋钮：默认 V4 只画细弧，缩到 24px 后几乎不可见，这里改为实心盘+指针。
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&)override{
  auto area=juce::Rectangle<int>(x,y,w,h).toFloat().reduced(1.0f);
  const float radius=juce::jmin(area.getWidth(),area.getHeight())*0.5f-0.5f;
  const auto centre=area.getCentre();
  g.setColour(edge.brighter(0.55f));g.fillEllipse(centre.x-radius,centre.y-radius,radius*2.0f,radius*2.0f);
  g.setColour(muted.darker(0.35f));g.drawEllipse(centre.x-radius,centre.y-radius,radius*2.0f,radius*2.0f,1.0f);
  const float angle=start+pos*(end-start);
  g.setColour(accent);g.drawLine(centre.x,centre.y,centre.x+std::sin(angle)*radius*0.78f,centre.y-std::cos(angle)*radius*0.78f,1.6f);
 }
};
}
class SyncViewport final:public juce::Viewport {public:std::function<void(juce::Rectangle<int>)> moved;void visibleAreaChanged(const juce::Rectangle<int>& a)override{if(moved)moved(a);}};
class Timeline final:public juce::Component,private juce::ChangeListener {
public:
  Session& session;int selected=-1,selectedTrack=0;int64_t cursor=0,grid=480,selectionStart=0,selectionEnd=0;double scale=12;int tool=0;bool dragging=false;
 std::function<void()> changed;std::function<void(std::function<void()>)> execute;
 juce::AudioFormatManager formats;juce::AudioThumbnailCache cache{24};std::map<juce::String,std::unique_ptr<juce::AudioThumbnail>> thumbnails;
 explicit Timeline(Session& s):session(s){formats.registerBasicFormats();}
 ~Timeline()override{for(auto& item:thumbnails)item.second->removeChangeListener(this);}
 int x(int64_t n)const{return int(std::llround(double(n)*scale/48000.0));}
 int64_t at(float px)const{auto n=std::max<int64_t>(0,std::llround(px*48000.0/scale));return grid>1?std::llround(double(n)/grid)*grid:n;}
 te::Clip* selection()const{auto clips=session.clips();return selected>=0&&selected<clips.size()?clips[selected]:nullptr;}
 void refresh(){setSize(std::max(1200,x(std::max<int64_t>(2880000,Session::samples(tracktion::TimePosition::fromSeconds(session.edit->getLength().inSeconds()))+480000))),std::max(500,(te::getAudioTracks(*session.edit).size()+1)*EditStyle::rowHeight));repaint();}
 juce::Rectangle<int> bounds(te::Clip& c,int row)const{auto p=c.getPosition();return {x(Session::samples(p.getStart())),row*EditStyle::rowHeight+3,std::max(3,x(Session::samples(p.getEnd()))-x(Session::samples(p.getStart()))),EditStyle::rowHeight-6};}
 void paint(juce::Graphics& g)override{
  g.fillAll(EditStyle::bg);auto area=g.getClipBounds();auto tracks=te::getAudioTracks(*session.edit);
  for(int row=0;row<=tracks.size();++row){g.setColour(row==selectedTrack?juce::Colour(0xff35434a):juce::Colour(row%2?0xff34383a:0xff303436));g.fillRect(0,row*EditStyle::rowHeight,getWidth(),EditStyle::rowHeight-1);g.setColour(EditStyle::edge);g.drawHorizontalLine((row+1)*EditStyle::rowHeight-1,0,float(getWidth()));}
  double step=scale<20?10:scale<60?5:scale<180?1:0.25;for(double t=std::max(0.0,std::floor(area.getX()/scale/step)*step);t*scale<area.getRight();t+=step){g.setColour(juce::Colour(0xff454b4e));g.drawVerticalLine(int(t*scale),float(area.getY()),float(area.getBottom()));}
  if(selectionEnd>selectionStart){g.setColour(EditStyle::accent.withAlpha(0.10f));g.fillRect(x(selectionStart),0,x(selectionEnd)-x(selectionStart),getHeight());}
  int index=0;for(int row=0;row<tracks.size();++row)for(auto* clip:tracks[row]->getClips()){
   auto box=bounds(*clip,row);auto colour=EditStyle::trackColour(row);g.setColour(colour.darker(0.65f));g.fillRect(box);g.setColour(index==selected?juce::Colours::white:colour);g.drawRect(box,index==selected?2:1);g.setColour(colour.darker(0.9f));g.fillRect(box.withHeight(19).reduced(1));g.setColour(EditStyle::text);g.setFont(12);g.drawText(clip->getName(),box.reduced(5).removeFromTop(17),juce::Justification::left,true);
   if(auto* audio=dynamic_cast<te::WaveAudioClip*>(clip)){auto file=audio->getAudioFile().getFile();auto key=file.getFullPathName();if(!thumbnails.contains(key)&&file.existsAsFile()){auto thumb=std::make_unique<juce::AudioThumbnail>(128,formats,cache);thumb->addChangeListener(this);thumb->setSource(new juce::FileInputSource(file));thumbnails[key]=std::move(thumb);}if(auto it=thumbnails.find(key);it!=thumbnails.end()){g.setColour(colour.brighter(0.35f));auto wave=box.reduced(3);wave.removeFromTop(20);auto p=clip->getPosition();it->second->drawChannels(g,wave,p.getOffset().inSeconds(),p.getOffset().inSeconds()+p.getLength().inSeconds(),0.88f);}}++index;
  }
  if(tracks.isEmpty()){g.setColour(EditStyle::muted);g.setFont(14);g.drawText(zh("空白工程 · 拖入音频，或从“轨道”菜单新建"),28,135,700,30,juce::Justification::left);}g.setColour(juce::Colour(0xffe3c955));g.drawVerticalLine(x(cursor),0,float(getHeight()));
 }
 void cancelDrag(){dragging=false;}
 void setTimeSelection(int64_t a,int64_t b){selectionStart=std::max<int64_t>(0,std::min(a,b));selectionEnd=std::max<int64_t>(0,std::max(a,b));}
 void clearTimeSelection(){selectionStart=selectionEnd=cursor;}
 void resetView(){cancelDrag();selected=-1;selectedTrack=te::getAudioTracks(*session.edit).isEmpty()?-1:0;cursor=0;clearTimeSelection();for(auto& entry:thumbnails)entry.second->removeChangeListener(this);thumbnails.clear();}
 void mouseDown(const juce::MouseEvent& e)override{dragging=true;auto tracks=te::getAudioTracks(*session.edit);int row=e.y/EditStyle::rowHeight;selectedTrack=row<tracks.size()?row:-1;cursor=at(e.position.x);selectionStart=selectionEnd=cursor;selected=-1;auto clips=session.clips();if(selectedTrack>=0)for(int i=clips.size();--i>=0;)if(clips[i]->getTrack()==tracks[selectedTrack]&&bounds(*clips[i],selectedTrack).contains(e.getPosition())){selected=i;break;}dragX=e.x;if(auto* c=selection()){dragStart=Session::samples(c->getPosition().getStart());dragEnd=Session::samples(c->getPosition().getEnd());trimLeft=e.x<bounds(*c,selectedTrack).getCentreX();}repaint();if(changed)changed();}
 void mouseDrag(const juce::MouseEvent& e)override{if(dragging&&tool==0){setTimeSelection(at(float(dragX)),at(e.position.x));repaint();if(changed)changed();}}
 void mouseUp(const juce::MouseEvent& e)override{if(!dragging)return;dragging=false;if(auto* c=selection();c&&std::abs(e.x-dragX)>3&&tool!=0){auto delta=std::llround((e.x-dragX)*48000.0/scale);if(tool==2){auto target=std::max<int64_t>(0,dragStart+delta);if(grid>1)target=std::llround(double(target)/grid)*grid;execute([this,c,target]{session.move(*c,target);});}else{auto edge=at(e.position.x);execute([this,c,edge]{session.trim(*c,trimLeft?edge:dragStart,trimLeft?dragEnd:edge);});}}}
private:int dragX=0;int64_t dragStart=0,dragEnd=0;bool trimLeft=false;void changeListenerCallback(juce::ChangeBroadcaster*)override{repaint();}
};
class TimeRuler final:public juce::Component {
public:Timeline& timeline;int scrollX=0;explicit TimeRuler(Timeline& t):timeline(t){}
 void paint(juce::Graphics& g)override{g.fillAll(EditStyle::panel);double step=timeline.scale<20?10:timeline.scale<60?5:timeline.scale<180?1:0.25;g.setFont(11);for(int row=0;row<3;++row){g.setColour(EditStyle::edge);g.drawHorizontalLine(row*22+21,0,float(getWidth()));}for(double t=std::max(0.0,std::floor(scrollX/timeline.scale/step)*step);t*timeline.scale-scrollX<getWidth();t+=step){int px=int(t*timeline.scale)-scrollX;g.setColour(juce::Colour(0xff555c60));g.drawVerticalLine(px,0,44);g.setColour(EditStyle::text);auto samples=int64_t(t*48000);g.drawText(EditStyle::clock(samples),px+4,0,90,20,juce::Justification::left);g.setColour(EditStyle::muted);g.drawText(EditStyle::timecode(samples),px+4,22,90,20,juce::Justification::left);}g.setColour(juce::Colour(0xffe3c955));int px=timeline.x(timeline.cursor)-scrollX;g.fillRect(px,44,2,22);g.drawText(zh("光标"),px+5,44,60,20,juce::Justification::left);}
 void mouseDown(const juce::MouseEvent& e)override{timeline.cursor=timeline.at(float(e.x+scrollX));timeline.repaint();if(timeline.changed)timeline.changed();}
};
class TrackStrip final:public juce::Component {
public:
 Session& s;int index;std::function<void(const juce::String&)> trace;std::function<void(std::function<void()>)> act;std::function<void(int)> select;std::function<void(int,int)> insert;
 EditTextEditor name;juce::Label mode;juce::TextButton mute{"M"},solo{"S"},output,send{"+"};juce::Slider volume,pan;std::array<juce::TextButton,5> slots;bool syncing=false;
 TrackStrip(Session& session,int i):s(session),index(i){setComponentID("track-strip-"+juce::String(i));name.setComponentID("track-name-"+juce::String(i));name.setTitle(i<0?zh("主输出名称"):zh("轨道名称 ")+juce::String(i+1));
  for(auto* c:std::initializer_list<juce::Component*>{&name,&mode,&mute,&solo,&output,&send,&volume,&pan})addAndMakeVisible(*c);
  name.setFont(juce::FontOptions(12).withStyle("Bold"));name.setReadOnly(index<0);name.setMultiLine(false);name.setColour(juce::TextEditor::backgroundColourId,EditStyle::panel);name.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);name.setColour(juce::TextEditor::focusedOutlineColourId,EditStyle::accent);name.setColour(juce::TextEditor::textColourId,EditStyle::text);name.setTooltip(zh("轨道名称 · Enter确认"));name.onTextChange=[this]{if(trace)trace("track-name-edit-"+juce::String(index));};name.onReturnKey=[this]{if(trace)trace("track-name-return-"+juce::String(index));if(syncing||index<0)return;auto value=name.getText().trim();act([this,value]{require(value.isNotEmpty(),"Track name cannot be empty");s.begin("Rename track");s.trackAt(index).setName(value);});};name.onEscapeKey=[this]{if(index>=0)name.setText(s.trackAt(index).getName(),false);};
  mode.setText(index<0?zh("主输出"):zh("波形"),juce::dontSendNotification);mode.setFont(juce::FontOptions(11));
  for(auto* b:{&mute,&solo}){b->setClickingTogglesState(true);b->setWantsKeyboardFocus(false);}mute.onClick=[this]{applyMix();};solo.onClick=[this]{applyMix();};mute.setEnabled(index>=0);solo.setEnabled(index>=0);
  volume.setRange(-120,24,0.1);volume.setSliderStyle(juce::Slider::LinearHorizontal);volume.setTextBoxStyle(juce::Slider::TextBoxRight,false,30,16);volume.setTooltip(zh("音量 · dB；双击归零"));volume.setDoubleClickReturnValue(true,0);volume.onDragEnd=[this]{applyMix();};volume.onValueChange=[this]{if(!syncing&&!volume.isMouseButtonDown())applyMix();};pan.setRange(-1,1,0.01);pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);pan.setDoubleClickReturnValue(true,0);pan.setTooltip(zh("声像；双击居中"));pan.onDragEnd=[this]{applyMix();};pan.onValueChange=[this]{if(!syncing&&!pan.isMouseButtonDown())applyMix();};pan.setEnabled(index>=0);
  for(int j=0;j<5;++j){auto& b=slots[size_t(j)];b.setComponentID("ins-slot-"+juce::String(j));addAndMakeVisible(b);b.setWantsKeyboardFocus(false);b.onClick=[this,j]{if(select)select(index);if(insert)insert(index,j);};}output.setWantsKeyboardFocus(false);send.setWantsKeyboardFocus(false);output.onClick=[this]{routeMenu(false);};send.onClick=[this]{routeMenu(true);};output.setEnabled(index>=0);send.setEnabled(index>=0);sync();
 }
 void applyMix(){if(syncing)return;act([this]{if(index<0)s.setMaster(float(volume.getValue()));else s.setMix(index,float(volume.getValue()),float(pan.getValue()),mute.getToggleState(),solo.getToggleState());});}
 void sync(){syncing=true;te::VolumeAndPanPlugin* v=index<0?s.edit->getMasterVolumePlugin().get():s.trackAt(index).getVolumePlugin();if(!name.hasKeyboardFocus(true))name.setText(index<0?zh("主输出"):s.trackAt(index).getName(),juce::dontSendNotification);if(!volume.isMouseButtonDown()&&!volume.hasKeyboardFocus(true))volume.setValue(v->getVolumeDb(),juce::dontSendNotification);if(!pan.isMouseButtonDown())pan.setValue(v->getPan(),juce::dontSendNotification);
  if(index>=0){send.setEnabled(te::getAudioTracks(*s.edit).size()>1);auto& t=s.trackAt(index);mute.setToggleState(t.isMuted(false),juce::dontSendNotification);solo.setToggleState(t.isSolo(false),juce::dontSendNotification);auto* target=t.getOutput().getDestinationTrack();output.setButtonText(target?target->getName():zh("主输出"));output.setTooltip(zh("主输出：")+output.getButtonText());auto sends=t.pluginList.getPluginsOfType<te::AuxSendPlugin>();send.setButtonText(sends.isEmpty()?zh("+ 发送"):juce::String(sends.size())+zh(" 发送"));}else output.setButtonText(zh("主输出"));auto chain=PluginChain::external(s,index);for(int j=0;j<5;++j){auto& b=slots[size_t(j)];b.setButtonText(j<chain.size()?chain[j]->getName()+(!chain[j]->getAudioPluginInstance()?zh(" [缺失]"):chain[j]->isEnabled()?"":zh(" [旁通]")):juce::String::charToString(juce::juce_wchar('A'+j))+"   ·");b.setTooltip(j<chain.size()?chain[j]->getName()+zh(" · 点击打开编辑器"):zh("插入插件"));}syncing=false;repaint();
 }
 void routeMenu(bool isSend){juce::PopupMenu menu;if(!isSend)menu.addItem(1,"Master");auto tracks=te::getAudioTracks(*s.edit);for(int j=0;j<tracks.size();++j)if(j!=index){if(isSend){juce::PopupMenu mode;mode.addItem(100+j*2,zh("推子后 · 0 dB"));mode.addItem(101+j*2,zh("推子前 · 0 dB"));menu.addSubMenu(tracks[j]->getName(),mode);}else menu.addItem(j+2,tracks[j]->getName());}menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(isSend?&send:&output),[safe=juce::Component::SafePointer<TrackStrip>(this),isSend](int result){if(!safe||result==0)return;safe->act([safe,result,isSend]{if(isSend)safe->s.addSend(safe->index,(result-100)/2,0,(result-100)%2!=0);else safe->s.route(safe->index,result-2);});});}
 void resized()override{const int w=getWidth(),h=getHeight();const int slotH=std::max(9,(h-6)/5);name.setBounds(10,2,128,20);mute.setBounds(10,26,24,17);solo.setBounds(36,26,24,17);mode.setBounds(62,26,66,17);volume.setBounds(10,h-21,72,17);pan.setBounds(86,h-24,26,24);output.setBounds(140,2,74,20);send.setBounds(140,26,74,20);for(int j=0;j<5;++j)slots[size_t(j)].setBounds(218,3+j*slotH,std::max(40,w-222),slotH-1);}
 void paint(juce::Graphics& g)override{const int h=getHeight();g.fillAll(EditStyle::panel);g.setColour(index<0?juce::Colour(0xffba7694):EditStyle::trackColour(index));g.fillRect(0,0,8,h-1);g.setColour(EditStyle::edge);g.drawVerticalLine(138,0,float(h));g.drawVerticalLine(216,0,float(h));g.drawHorizontalLine(h-1,0,float(getWidth()));}
 void mouseDown(const juce::MouseEvent&)override{if(select)select(index);}
};
class SideList final:public juce::Component {
public:Session& s;bool clips;int selected=-1;std::function<void(int)> chosen;explicit SideList(Session& session,bool isClips):s(session),clips(isClips){}
 static constexpr int rowH=25;void paint(juce::Graphics& g)override{g.fillAll(EditStyle::bg);g.setFont(11);int n=clips?s.clips().size():te::getAudioTracks(*s.edit).size()+1;auto tracks=te::getAudioTracks(*s.edit);for(int i=0;i<n;++i){const int y=i*rowH;if(i==selected){g.setColour(EditStyle::accent.darker(0.5f));g.fillRect(0,y,getWidth(),rowH);}else if(i%2){g.setColour(EditStyle::rowOdd);g.fillRect(0,y,getWidth(),rowH);}g.setColour(!clips&&i==tracks.size()?juce::Colour(0xffba7694):EditStyle::trackColour(i));g.fillRect(5,y+4,8,rowH-8);g.setColour(EditStyle::text);juce::String text=clips?s.clips()[i]->getName():i<tracks.size()?tracks[i]->getName():zh("主输出");g.drawText(text,18,y,std::max(10,getWidth()-24),rowH,juce::Justification::left,true);}}
 void refresh(){setSize(std::max(100,getWidth()),std::max(150,(clips?s.clips().size():te::getAudioTracks(*s.edit).size()+1)*rowH));repaint();}
 void mouseDown(const juce::MouseEvent& e)override{int n=clips?s.clips().size():te::getAudioTracks(*s.edit).size()+1;int i=e.y/rowH;if(i<n&&chosen)chosen(i);}
};
class MainPanel final:public juce::Component,public juce::FileDragAndDropTarget,private juce::Timer,private juce::MenuBarModel {
public:
 enum Command {addTrack,importFile,saveProject,openProject,saveCopy,undoEdit,redoEdit,splitClip,copyClip,removeClip,moveClip,trimClip,envelope,exportWave,saveUserTemplate,openUserTemplate,relinkMedia,crossfadeClips,recovery,plugins};
 EditStyle::Look look;juce::TooltipWindow tips{this,650};ExportJob exportJob;juce::TextButton cancelExport{zh("取消导出")};juce::ComboBox exportDepth;
 Session& session;Timeline timeline;TimeRuler ruler;SyncViewport viewport,trackViewport;juce::Component strips;juce::Viewport navViewport,clipViewport;SideList nav,clipList;
 juce::MenuBarComponent menu{this};juce::Label title,status,help,counter;std::array<juce::Label,10> fieldLabels;juce::Label clipHeading,mixHeading;
 EditTextEditor start,end,cursor,gain,fadeIn,fadeOut,trackDb,trackPan,masterDb,sendDb;juce::ToggleButton mute{zh("静音")},solo{zh("独奏")},preSend{zh("推子前")};juce::ComboBox outputTrack;juce::Label mixHelp;
 juce::TextButton applyMix{zh("应用混音")},busButton{zh("添加总线")},routeButton{zh("设置输出")},sendButton{zh("添加发送")};juce::Slider zoom;juce::ToggleButton snap{zh("吸附")};
 std::vector<std::unique_ptr<juce::TextButton>> buttons;std::unique_ptr<juce::FileChooser> chooser;std::unique_ptr<PluginsWindow> pluginWindow;std::unique_ptr<HostedPluginWindow> inlineEditor;
 std::array<juce::TextButton,4> modes;std::array<juce::TextButton,3> tools;std::array<juce::TextButton,5> transport;std::vector<int> groupDividers;juce::TextButton rowMinus{zh("−")},rowPlus{zh("+")};juce::TextButton properties{zh("片段属性")},tasks{zh("任务")},routing{zh("路由属性")};juce::ComboBox gridSize;
 std::vector<std::unique_ptr<TrackStrip>> rows;bool syncingScroll=false,showLeft=true,showRight=true,persist=false,zoomGesture=false;double zoomAnchorSample=0;int zoomAnchorPixel=0;int leftWidth=165,rightWidth=190,controlWidth=480,bottomMode=0;int dragDivider=0;te::Edit* rowEdit=nullptr;int rowCount=-1;int64_t lastSnapshot=0;int snapshotTicks=0;
 explicit MainPanel(Session& s,bool keepLayout=false):session(s),timeline(s),ruler(timeline),nav(s,false),clipList(s,true),persist(keepLayout){
  setComponentID("edit-workspace");setTitle(zh("编辑工作区"));timeline.setComponentID("timeline");nav.setComponentID("track-navigation");clipList.setComponentID("clip-list");setLookAndFeel(&look);setWantsKeyboardFocus(true);viewport.setViewedComponent(&timeline,false);trackViewport.setViewedComponent(&strips,false);trackViewport.setScrollBarsShown(false,false);navViewport.setViewedComponent(&nav,false);clipViewport.setViewedComponent(&clipList,false);navViewport.setScrollBarsShown(true,false);clipViewport.setScrollBarsShown(true,false);
  for(auto* c:std::initializer_list<juce::Component*>{&menu,&viewport,&trackViewport,&ruler,&navViewport,&clipViewport,&status,&counter,&zoom,&snap,&properties,&tasks,&routing,&gridSize,&rowMinus,&rowPlus})addAndMakeVisible(*c);
  viewport.moved=[this](auto a){if(!syncingScroll){syncingScroll=true;trackViewport.setViewPosition(0,a.getY());syncingScroll=false;}ruler.scrollX=a.getX();ruler.repaint();};trackViewport.moved=[this](auto a){if(!syncingScroll){syncingScroll=true;viewport.setViewPosition(viewport.getViewPositionX(),a.getY());syncingScroll=false;}};
  nav.chosen=[this](int i){selectTrack(i<te::getAudioTracks(*session.edit).size()?i:-1,true);};clipList.chosen=[this](int i){timeline.cancelDrag();timeline.clearTimeSelection();timeline.selected=i;auto* c=session.clips()[i];timeline.selectedTrack=te::getAudioTracks(*session.edit).indexOf(dynamic_cast<te::AudioTrack*>(c->getTrack()));timeline.cursor=Session::samples(c->getPosition().getStart());viewport.setViewPosition(std::max(0,timeline.x(timeline.cursor)-40),timeline.selectedTrack*EditStyle::rowHeight);syncInspector();};
  const std::pair<const char*,Command> specs[]={{"添加轨道",addTrack},{"导入音频",importFile},{"保存",saveProject},{"打开",openProject},{"另存为",saveCopy},{"撤销",undoEdit},{"重做",redoEdit},{"切分",splitClip},{"复制",copyClip},{"删除片段",removeClip},{"移动",moveClip},{"裁剪",trimClip},{"应用增益/淡化",envelope},{"导出 WAV",exportWave},{"保存自定模板",saveUserTemplate},{"从模板新建",openUserTemplate},{"重定位素材",relinkMedia},{"交叉淡化",crossfadeClips},{"保存恢复点",recovery},{"插件链",plugins}};
  for(auto [label,command]:specs){auto b=std::make_unique<juce::TextButton>(zh(label));b->setWantsKeyboardFocus(false);b->onClick=[this,command]{perform(command);};addChildComponent(*b);buttons.push_back(std::move(b));}
  const char* modesText[]={"滑移","替换","自由","网格"};for(int i=0;i<4;++i){auto& b=modes[size_t(i)];b.setButtonText(zh(modesText[i]));b.setWantsKeyboardFocus(false);addAndMakeVisible(b);b.setEnabled(i>=2);b.setTooltip(i<2?zh("此编辑模式尚未接入"):zh("切换自由编辑/网格吸附"));b.onClick=[this,i]{snap.setToggleState(i==3,juce::dontSendNotification);setGrid();};}
  const char* toolText[]={"选择","裁剪","抓取"};for(int i=0;i<3;++i){auto& b=tools[size_t(i)];b.setButtonText(zh(toolText[i]));b.setWantsKeyboardFocus(false);addAndMakeVisible(b);b.onClick=[this,i]{setTool(i);};}setTool(0);rowMinus.setWantsKeyboardFocus(false);rowPlus.setWantsKeyboardFocus(false);rowMinus.setTooltip(zh("行高：变窄"));rowPlus.setTooltip(zh("行高：变宽"));rowMinus.onClick=[this]{setRowHeight(EditStyle::rowHeight-EditStyle::rowHeightStep);};rowPlus.onClick=[this]{setRowHeight(EditStyle::rowHeight+EditStyle::rowHeightStep);};
  const char* transportText[]={"|<","<<",">", "[]", "录音"};for(int i=0;i<5;++i){auto& b=transport[size_t(i)];b.setButtonText(i==4?zh(transportText[i]):juce::String(transportText[i]));b.setWantsKeyboardFocus(false);addAndMakeVisible(b);b.setEnabled(i==0);b.setTooltip(i==0?zh("定位到工程开头"):i==4?zh("录音工作流暂缓"):zh("设备播放尚未接入此编辑窗口"));}transport[0].onClick=[this]{timeline.cursor=0;viewport.setViewPosition(0,viewport.getViewPositionY());cursor.setText("0",false);updateCounter();timeline.repaint();ruler.repaint();};

  for(auto* field:{&start,&end,&cursor,&gain,&fadeIn,&fadeOut,&trackDb,&trackPan,&masterDb,&sendDb}){field->setInputRestrictions(18,"0123456789.-");field->setText("0");field->setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff181c1e));field->setColour(juce::TextEditor::textColourId,EditStyle::green);field->setColour(juce::TextEditor::focusedOutlineColourId,EditStyle::accent);addChildComponent(field);}end.setText("48000");cursor.onReturnKey=[this]{action([this]{timeline.cursor=editSampleValue(cursor);updateCounter();timeline.repaint();ruler.repaint();});};
  const char* fieldIds[]={"selection-start","selection-end","edit-cursor","clip-gain","clip-fade-in","clip-fade-out","track-volume","track-pan","master-volume","send-volume"};juce::TextEditor* fields[]={&start,&end,&cursor,&gain,&fadeIn,&fadeOut,&trackDb,&trackPan,&masterDb,&sendDb};for(int i=0;i<10;++i){fields[i]->setComponentID(fieldIds[i]);fields[i]->setTitle(fieldIds[i]);fields[i]->onTextChange=[this,i]{inputEvent("field-edit-"+juce::String(i));};}
  const char* captions[]={"起点 · 采样","终点 · 采样","光标 · 采样","增益 · dB","淡入 · 采样","淡出 · 采样","推子 · dB","声像 · -1..1","Master · dB","发送 · dB"};for(int i=0;i<10;++i){fields[i]->setTitle(zh(captions[i]));fieldLabels[size_t(i)].setText(zh(captions[i]),juce::dontSendNotification);fieldLabels[size_t(i)].setFont(juce::FontOptions(11));addChildComponent(fieldLabels[size_t(i)]);}
  zoom.setRange(5,1000,0.1);zoom.setSkewFactorFromMidPoint(60);zoom.setValue(timeline.scale);zoom.setSliderStyle(juce::Slider::LinearHorizontal);zoom.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);zoom.setTooltip(zh("时间线水平缩放"));zoom.onDragStart=[this]{captureZoomAnchor();zoomGesture=true;};zoom.onDragEnd=[this]{zoomGesture=false;};zoom.onValueChange=[this]{setTimelineZoom(zoom.getValue());};
  gridSize.addItem(zh("10 毫秒"),1);gridSize.addItem(zh("100 毫秒"),2);gridSize.addItem(zh("1 秒"),3);gridSize.setSelectedId(1,juce::dontSendNotification);gridSize.setTooltip(zh("绝对时间网格；音乐节拍网格后续接入"));gridSize.onChange=[this]{setGrid();};snap.setToggleState(true,juce::dontSendNotification);snap.onClick=[this]{setGrid();};setGrid();
  for(auto* c:std::initializer_list<juce::Component*>{&mute,&solo,&preSend,&outputTrack,&applyMix,&busButton,&routeButton,&sendButton,&exportDepth,&cancelExport})addChildComponent(c);
  applyMix.onClick=[this]{action([this]{if(timeline.selectedTrack>=0)session.setMix(timeline.selectedTrack,float(editNumberValue(trackDb)),float(editNumberValue(trackPan)),mute.getToggleState(),solo.getToggleState());session.setMaster(float(editNumberValue(masterDb)));});};busButton.onClick=[this]{action([this]{timeline.selectedTrack=session.addBus();});};routeButton.onClick=[this]{action([this]{session.route(timeline.selectedTrack,outputTrack.getSelectedId()-2);});};sendButton.onClick=[this]{action([this]{session.addSend(timeline.selectedTrack,outputTrack.getSelectedId()-2,float(editNumberValue(sendDb)),preSend.getToggleState());});};
  exportDepth.addItem(zh("PCM 24 位"),24);exportDepth.addItem(zh("浮点 32 位"),32);exportDepth.setSelectedId(24);cancelExport.onClick=[this]{action([this]{exportJob.cancel();});};properties.onClick=[this]{bottomMode=bottomMode==1?0:1;resized();};tasks.onClick=[this]{bottomMode=bottomMode==2?0:2;resized();};routing.onClick=[this]{bottomMode=bottomMode==3?0:3;syncMixer();resized();};
  timeline.changed=[this]{grabKeyboardFocus();syncInspector();};timeline.execute=[this](auto fn){action([this,fn]{fn();syncInspector();});};counter.setColour(juce::Label::backgroundColourId,juce::Colour(0xff090d0b));counter.setColour(juce::Label::textColourId,EditStyle::green);counter.setFont(juce::FontOptions(15));counter.setJustificationType(juce::Justification::centredLeft);counter.setBorderSize(juce::BorderSize<int>(4,10,4,6));counter.setMinimumHorizontalScale(1.0f);counter.setTooltip(zh("光标时间码 · 30fps显示基准；下行为分秒，不代表视频同步已接入"));status.setFont(juce::FontOptions(11));status.setText(zh("就绪 · 离线编辑 · 0.1.6"),juce::dontSendNotification);
  if(persist)if(auto xml=juce::XmlDocument::parse(layoutFile())){leftWidth=std::clamp(xml->getIntAttribute("left",165),130,260);rightWidth=std::clamp(xml->getIntAttribute("right",190),145,320);showLeft=xml->getBoolAttribute("showLeft",true);showRight=xml->getBoolAttribute("showRight",true);timeline.scale=std::clamp(xml->getDoubleAttribute("zoom",12),5.0,1000.0);zoom.setValue(timeline.scale,juce::dontSendNotification);EditStyle::rowHeight=std::clamp(xml->getIntAttribute("rowHeight",64),EditStyle::rowHeightMin,EditStyle::rowHeightMax);}
  setSize(1600,900);refresh();startTimer(200);
 }
 ~MainPanel()override{stopTimer();viewport.moved=nullptr;trackViewport.moved=nullptr;if(persist){juce::XmlElement xml("LAYOUT");xml.setAttribute("left",leftWidth);xml.setAttribute("right",rightWidth);xml.setAttribute("showLeft",showLeft);xml.setAttribute("showRight",showRight);xml.setAttribute("zoom",timeline.scale);xml.setAttribute("rowHeight",EditStyle::rowHeight);xml.writeTo(layoutFile());}inlineEditor.reset();pluginWindow.reset();setLookAndFeel(nullptr);}
 juce::File layoutFile()const{return juce::File(STUDIO_SOURCE_ROOT).getChildFile("build/user-gui-layout.xml");}
 void setRowHeight(int value){EditStyle::rowHeight=std::clamp(value,EditStyle::rowHeightMin,EditStyle::rowHeightMax);resized();timeline.refresh();repaint();}
 void setTool(int i){timeline.cancelDrag();timeline.tool=i;for(int j=0;j<3;++j)tools[size_t(j)].setToggleState(i==j,juce::dontSendNotification);timeline.setMouseCursor(i==2?juce::MouseCursor::DraggingHandCursor:i==1?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::IBeamCursor);}
 void setGrid(){timeline.grid=snap.getToggleState()?(gridSize.getSelectedId()==2?4800:gridSize.getSelectedId()==3?48000:480):1;modes[2].setToggleState(!snap.getToggleState(),juce::dontSendNotification);modes[3].setToggleState(snap.getToggleState(),juce::dontSendNotification);}
 void updateCounter(){counter.setText(EditStyle::timecode(timeline.cursor)+"\n"+EditStyle::clock(timeline.cursor),juce::dontSendNotification);ruler.repaint();}
 void selectTrack(int index,bool reveal){timeline.cancelDrag();timeline.selectedTrack=index;timeline.selected=-1;if(reveal)viewport.setViewPosition(viewport.getViewPositionX(),(index<0?te::getAudioTracks(*session.edit).size():index)*EditStyle::rowHeight);syncInspector();grabKeyboardFocus();}
 void showPlugins(int index){inlineEditor.reset();pluginWindow=std::make_unique<PluginsWindow>(session,index);}
 void slotClicked(int index,int slot){action([this,index,slot]{auto chain=PluginChain::external(session,index);if(slot<chain.size()){chain[slot]->initialiseFully();if(!chain[slot]->getAudioPluginInstance()){showPlugins(index);return;}pluginWindow.reset();inlineEditor=std::make_unique<HostedPluginWindow>(*chain[slot]);}else showPlugins(index);});}
 void action(std::function<void()> fn){try{fn();status.setText(zh("操作完成 · 离线编辑"),juce::dontSendNotification);refresh();}catch(const std::exception& e){status.setText(zh("未执行：")+juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
 void syncRows(){auto tracks=te::getAudioTracks(*session.edit);if(rowEdit!=session.edit.get()||rowCount!=tracks.size()){rows.clear();rowEdit=session.edit.get();rowCount=tracks.size();for(int i=0;i<=tracks.size();++i){auto row=std::make_unique<TrackStrip>(session,i==tracks.size()?-1:i);row->trace=[this](const auto& event){inputEvent(event);};row->act=[this](auto fn){action(fn);};row->select=[this](int index){selectTrack(index,false);};row->insert=[this](int index,int slot){slotClicked(index,slot);};strips.addAndMakeVisible(*row);rows.push_back(std::move(row));}strips.setSize(controlWidth,timeline.getHeight()+(viewport.getHorizontalScrollBar().isVisible()?viewport.getScrollBarThickness():0));for(int i=0;i<int(rows.size());++i)rows[size_t(i)]->setBounds(0,i*EditStyle::rowHeight,controlWidth,EditStyle::rowHeight);}for(auto& row:rows)row->sync();nav.selected=timeline.selectedTrack<0?tracks.size():timeline.selectedTrack;nav.repaint();clipList.selected=timeline.selected;clipList.repaint();}
 void refresh(){auto count=te::getAudioTracks(*session.edit).size();if(timeline.selectedTrack>=count)timeline.selectedTrack=count>0?count-1:-1;timeline.refresh();syncRows();syncMixer();nav.refresh();clipList.refresh();updateCounter();auto missing=session.missingMedia();if(!missing.isEmpty())status.setText(zh("缺失素材：")+juce::String(missing.size())+zh(" 个 · 文件菜单可重定位"),juce::dontSendNotification);repaint();}
 void syncInspector(){syncRows();syncMixer();cursor.setText(juce::String(timeline.cursor),false);if(auto* c=timeline.selection()){start.setText(juce::String(Session::samples(c->getPosition().getStart())),false);end.setText(juce::String(Session::samples(c->getPosition().getEnd())),false);if(auto* a=dynamic_cast<te::WaveAudioClip*>(c)){gain.setText(juce::String(a->getGainDB()),false);fadeIn.setText(juce::String(std::llround(a->getFadeIn().inSeconds()*48000)),false);fadeOut.setText(juce::String(std::llround(a->getFadeOut().inSeconds()*48000)),false);}}if(timeline.selectionEnd>timeline.selectionStart){start.setText(juce::String(timeline.selectionStart),false);end.setText(juce::String(timeline.selectionEnd),false);}updateCounter();timeline.repaint();}
 void syncMixer(){auto tracks=te::getAudioTracks(*session.edit);outputTrack.clear(juce::dontSendNotification);outputTrack.addItem("Master",1);for(int i=0;i<tracks.size();++i)outputTrack.addItem(tracks[i]->getName(),i+2);if(timeline.selectedTrack>=0&&timeline.selectedTrack<tracks.size()){auto* t=tracks[timeline.selectedTrack];auto* v=t->getVolumePlugin();trackDb.setText(juce::String(v->getVolumeDb(),2),false);trackPan.setText(juce::String(v->getPan(),3),false);mute.setToggleState(t->isMuted(false),juce::dontSendNotification);solo.setToggleState(t->isSolo(false),juce::dontSendNotification);auto* target=t->getOutput().getDestinationTrack();outputTrack.setSelectedId(target?tracks.indexOf(target)+2:1,juce::dontSendNotification);}else outputTrack.setSelectedId(1,juce::dontSendNotification);masterDb.setText(juce::String(session.edit->getMasterVolumePlugin()->getVolumeDb(),2),false);}
 void captureZoomAnchor(){int oldX=viewport.getViewPositionX(),cursorX=timeline.x(timeline.cursor);bool visible=cursorX>=oldX&&cursorX<oldX+viewport.getViewWidth();zoomAnchorSample=visible?double(timeline.cursor):double(oldX)*48000.0/timeline.scale;zoomAnchorPixel=visible?cursorX-oldX:0;}
 void setTimelineZoom(double value){timeline.cancelDrag();int oldY=viewport.getViewPositionY();if(!zoomGesture)captureZoomAnchor();timeline.scale=value;timeline.refresh();strips.setSize(controlWidth,timeline.getHeight()+(viewport.getHorizontalScrollBar().isVisible()?viewport.getScrollBarThickness():0));viewport.setViewPosition(std::max(0,int(std::llround(zoomAnchorSample*value/48000.0))-zoomAnchorPixel),oldY);trackViewport.setViewPosition(0,viewport.getViewPositionY());ruler.scrollX=viewport.getViewPositionX();ruler.repaint();}
 void resetEditView(){timeline.resetView();viewport.setViewPosition(0,0);start.setText("0",false);end.setText(juce::String(std::max<int64_t>(48000,Session::samples(tracktion::TimePosition::fromSeconds(session.edit->getLength().inSeconds())))),false);cursor.setText("0",false);for(auto* field:{&gain,&fadeIn,&fadeOut})field->setText("0",false);updateCounter();}
 void loadProjectForUi(const juce::File& file,bool isTemplate){inlineEditor.reset();pluginWindow.reset();require(!session.edit->hasChangedSinceSaved(),"Save the current project before opening another");if(isTemplate)session.loadTemplate(file);else session.openProject(file);resetEditView();}
    void choose(Command command){

        auto mode=(command==saveCopy||command==exportWave||command==saveUserTemplate)?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode;
        auto pattern=(command==importFile||command==relinkMedia)?"*.wav;*.bwf;*.aif;*.aiff;*.flac":command==exportWave?"*.wav":(command==saveUserTemplate||command==openUserTemplate)?"*.swtemplate":"*.tracktionedit";
        chooser=std::make_unique<juce::FileChooser>(zh("选择文件"),session.root,pattern);
        chooser->launchAsync(mode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<MainPanel>(this),command](const juce::FileChooser& f){if(!safe)return;auto file=f.getResult();if(file==juce::File())return;safe->action([safe,file,command]{
            if(command==importFile)safe->import(file);
            if(command==saveUserTemplate)safe->session.saveTemplate(file.withFileExtension("swtemplate"));
            if(command==openUserTemplate)safe->loadProjectForUi(file,true);
            if(command==relinkMedia){auto* audio=dynamic_cast<te::WaveAudioClip*>(safe->timeline.selection());require(audio!=nullptr,"Select an audio clip");safe->session.relink(*audio,file);}
            if(command==saveCopy)safe->session.saveAs(file.withFileExtension("tracktionedit"));
            if(command==openProject)safe->loadProjectForUi(file,false);
            if(command==exportWave)safe->exportJob.start(safe->session,file.withFileExtension("wav"),editSampleValue(safe->start),editSampleValue(safe->end),safe->exportDepth.getSelectedId());
        });});
    }
    void import(const juce::File& f){int track=std::clamp(timeline.selectedTrack,0,te::getAudioTracks(*session.edit).size());auto* imported=session.importAudio(f,track,timeline.cursor);timeline.cancelDrag();timeline.clearTimeSelection();timeline.selected=session.clips().indexOf(imported);timeline.selectedTrack=te::getAudioTracks(*session.edit).indexOf(dynamic_cast<te::AudioTrack*>(imported->getTrack()));syncInspector();}
    void perform(Command command){
        timeline.cancelDrag();
        if(command==plugins){action([this]{showPlugins(timeline.selectedTrack);});return;}
        if(command==importFile||command==openProject||command==saveCopy||command==exportWave||command==saveUserTemplate||command==openUserTemplate||command==relinkMedia){choose(command);return;}
        action([this,command]{
            if(command==addTrack){session.begin("Add track");session.addTrack();return;}
            if(command==saveProject){session.save();return;}
            if(command==undoEdit||command==redoEdit){juce::var selectedId;if(auto* clip=timeline.selection())selectedId=clip->state.getProperty("id");if(command==undoEdit)session.undo();else session.redo();timeline.selected=-1;if(!selectedId.isVoid()){auto clips=session.clips();for(int i=0;i<clips.size();++i)if(clips[i]->state.getProperty("id")==selectedId){timeline.selected=i;break;}}syncInspector();return;}
            if(command==recovery){session.checkpoint();return;}
            auto* c=timeline.selection();require(c!=nullptr,"Select a clip first");
            auto pos=editSampleValue(cursor);
            if(command==splitClip)session.split(*c,pos);
            if(command==copyClip)session.duplicate(*c,pos);
            if(command==removeClip){session.erase(*c);timeline.selected=-1;}
            if(command==moveClip)session.move(*c,editSampleValue(start));
            if(command==trimClip)session.trim(*c,editSampleValue(start),editSampleValue(end));
            if(command==envelope){auto* audio=dynamic_cast<te::WaveAudioClip*>(c);require(audio!=nullptr,"Select audio clip");session.setEnvelope(*audio,float(editNumberValue(gain)),editSampleValue(fadeIn),editSampleValue(fadeOut));}
            if(command==crossfadeClips){auto* left=dynamic_cast<te::WaveAudioClip*>(c);require(left!=nullptr,"Select left audio clip");te::WaveAudioClip* right=nullptr;for(auto* item:session.clips())if(item!=left&&item->getTrack()==left->getTrack()&&item->getPosition().getStart()>left->getPosition().getStart()&&item->getPosition().getStart()<left->getPosition().getEnd()){right=dynamic_cast<te::WaveAudioClip*>(item);break;}require(right!=nullptr,"No overlapping right clip");session.crossfade(*left,*right);}
            syncInspector();
        });
    }

 bool inputProbe=false;juce::String lastInputEvent;int inputEventCount=0;
 void inputEvent(const juce::String& event){if(!inputProbe)return;lastInputEvent=event;++inputEventCount;session.root.getChildFile("gui-input-events.log").appendText(event+"\n");writeInputProbe();}
 void writeInputProbe(){auto* out=new juce::DynamicObject;auto* focus=juce::Component::getCurrentlyFocusedComponent();juce::String focusId;for(auto* c=focus;c&&focusId.isEmpty();c=c->getParentComponent())focusId=c->getComponentID();out->setProperty("focus",focusId);out->setProperty("last_event",lastInputEvent);out->setProperty("event_count",inputEventCount);out->setProperty("start",start.getText());out->setProperty("end",end.getText());out->setProperty("cursor_text",cursor.getText());out->setProperty("cursor",double(timeline.cursor));out->setProperty("selected_track",timeline.selectedTrack);out->setProperty("scale",timeline.scale);out->setProperty("zoom_anchor_sample",zoomAnchorSample);out->setProperty("zoom_anchor_pixel",zoomAnchorPixel);out->setProperty("scroll_x",viewport.getViewPositionX());out->setProperty("scroll_y",viewport.getViewPositionY());out->setProperty("ruler_x",ruler.scrollX);out->setProperty("time_start",double(timeline.selectionStart));out->setProperty("time_end",double(timeline.selectionEnd));juce::Array<juce::var> names,editors,clips;for(auto* t:te::getAudioTracks(*session.edit))names.add(t->getName());for(auto& row:rows)editors.add(row->name.getText());for(auto* c:session.clips()){auto* item=new juce::DynamicObject;item->setProperty("start",double(Session::samples(c->getPosition().getStart())));item->setProperty("end",double(Session::samples(c->getPosition().getEnd())));clips.add(juce::var(item));}out->setProperty("track_names",names);out->setProperty("editor_names",editors);out->setProperty("clips",clips);session.root.getChildFile("gui-input-state.json").replaceWithText(juce::JSON::toString(juce::var(out)));}
 juce::StringArray getMenuBarNames()override{return {zh("文件"),zh("编辑"),zh("视图"),zh("轨道"),zh("片段"),zh("选项"),zh("窗口"),zh("帮助")};}
 juce::PopupMenu getMenuForIndex(int index,const juce::String&)override{juce::PopupMenu m;auto item=[&](Command c,const char* text){m.addItem(100+int(c),zh(text));};if(index==0){item(openProject,"打开工程…");item(importFile,"导入音频…");item(saveProject,"保存    Ctrl+S");item(saveCopy,"另存为…");m.addSeparator();item(recovery,"保存恢复点");item(saveUserTemplate,"保存用户模板…");item(openUserTemplate,"从用户模板新建…");item(relinkMedia,"重定位素材…");m.addSeparator();item(exportWave,"导出 WAV…");}if(index==1){item(undoEdit,"撤销    Ctrl+Z");item(redoEdit,"重做    Ctrl+Shift+Z");item(copyClip,"复制到光标");item(removeClip,"删除片段    Delete");}if(index==2){m.addItem(1,zh("轨道列表"),true,showLeft);m.addItem(2,zh("片段列表"),true,showRight);m.addItem(3,zh("片段属性"),true,bottomMode==1);m.addItem(4,zh("路由属性"),true,bottomMode==3);m.addSeparator();m.addItem(5,zh("恢复默认布局"));}if(index==3){item(addTrack,"新建音轨");m.addItem(6,zh("新建总线"));item(plugins,"插件链…");}if(index==4){item(splitClip,"在光标处切分");item(moveClip,"移动至属性起点");item(trimClip,"按属性范围裁剪");item(envelope,"应用增益与淡化");item(crossfadeClips,"交叉淡化");}if(index==5){m.addItem(7,zh("网格吸附"),true,snap.getToggleState());m.addItem(8,zh("时间码显示：30 fps"),false);}if(index==6){item(plugins,"当前轨道插件链…");m.addItem(9,zh("后台任务"));}if(index==7){m.addItem(10,"DAW Tools 0.1.6",false);m.addItem(11,zh("设备播放/录音尚未接入此窗口"),false);}return m;}
 void menuItemSelected(int id,int)override{if(id>=100){perform(Command(id-100));return;}if(id==1)showLeft=!showLeft;if(id==2)showRight=!showRight;if(id==3)bottomMode=bottomMode==1?0:1;if(id==4)bottomMode=bottomMode==3?0:3;if(id==5){showLeft=showRight=true;leftWidth=165;rightWidth=190;bottomMode=0;zoom.setValue(12);}if(id==6)busButton.onClick();if(id==7){snap.setToggleState(!snap.getToggleState(),juce::dontSendNotification);setGrid();}if(id==9)bottomMode=2;resized();}
 bool keyPressed(const juce::KeyPress& incoming)override{auto key=editInputKey(incoming);inputEvent("workspace-key-"+key.getTextDescription());if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='S'){perform(saveProject);return true;}auto* focus=juce::Component::getCurrentlyFocusedComponent();for(auto* c=focus;c&&c!=this;c=c->getParentComponent())if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;if(key.isKeyCode(juce::KeyPress::escapeKey)){timeline.cancelDrag();return true;}if(key.getModifiers().isCommandDown()){if(key.getKeyCode()=='Z'){perform(key.getModifiers().isShiftDown()?redoEdit:undoEdit);return true;}if(key.getKeyCode()=='Y'){perform(redoEdit);return true;}if(key.getKeyCode()=='S'){perform(saveProject);return true;}if(key.getKeyCode()=='O'){perform(openProject);return true;}if(key.getKeyCode()=='I'){perform(importFile);return true;}}if(key.getKeyCode()==juce::KeyPress::deleteKey){perform(removeClip);return true;}return false;}
 bool isInterestedInFileDrag(const juce::StringArray& files)override{for(auto f:files)if(juce::File(f).hasFileExtension("wav;bwf;aif;aiff;flac"))return true;return false;}
 void filesDropped(const juce::StringArray& files,int x,int y)override{action([this,files,x,y]{auto pos=timeline.getLocalPoint(this,juce::Point<int>{x,y});if(viewport.getBounds().contains(x,y)){timeline.cursor=timeline.at(float(pos.x));int row=pos.y/EditStyle::rowHeight;timeline.selectedTrack=std::min(row,te::getAudioTracks(*session.edit).size());}for(auto& f:files)import(juce::File(f));});}
 void timerCallback()override{syncRows();if(inputProbe)writeInputProbe();if(exportJob.active){try{juce::String result;if(exportJob.poll(result))status.setText(result=="cancelled"?zh("导出已取消；未生成目标文件"):zh("导出完成：")+result,juce::dontSendNotification);else status.setText(zh("后台导出：")+juce::String(int(exportJob.progress()*100))+"%",juce::dontSendNotification);}catch(const std::exception& e){status.setText(zh("导出失败：")+juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}cancelExport.setEnabled(exportJob.active);if(++snapshotTicks<300)return;snapshotTicks=0;auto changed=session.edit->getTimeOfLastChange().toMilliseconds();if(session.edit->hasChangedSinceSaved()&&changed>lastSnapshot){action([this,changed]{session.checkpoint();lastSnapshot=changed;});}}
 void resized()override{
  const int w=getWidth(),h=getHeight(),left=showLeft?leftWidth:0,right=showRight?rightWidth:0;
  controlWidth=w<1400?300:340;
  const int timelineX=left+controlWidth+4,clipX=w-right;
  const int top=EditStyle::topHeight,head=EditStyle::rulerHeight;
  const int bottom=bottomMode?146:EditStyle::statusHeight;
  const int workH=std::max(120,h-top-head-bottom);
  menu.setBounds(0,0,w,25);
  groupDividers.clear();
  // 第一行：走带 | 计数器 | 网格 | 缩放 | 编辑模式 | 工具 | 行高
  const int r1=30,rh=26;int x=6;
  for(int i=0;i<5;++i){auto& b=transport[size_t(i)];b.setComponentID("transport-"+juce::String(i));b.setBounds(x+i*36,r1,34,rh);}
  x+=5*36+2;groupDividers.push_back(x-2);x+=4;
  counter.setBounds(x,r1,168,rh+24);x+=170;groupDividers.push_back(x-2);x+=2;
  gridSize.setBounds(x,r1,86,rh);snap.setBounds(x+90,r1,62,rh);x+=156;groupDividers.push_back(x-2);x+=2;
  const int zoomW=std::clamp(w-8-966,60,132);
  zoom.setBounds(x,r1+2,zoomW,rh-4);x+=zoomW+6;groupDividers.push_back(x-2);x+=2;
  for(int i=0;i<4;++i)modes[size_t(i)].setBounds(x+i*56,r1,54,rh);
  x+=4*56+2;groupDividers.push_back(x-2);x+=4;
  for(int i=0;i<3;++i)tools[size_t(i)].setBounds(x+i*50,r1,48,rh);
  x+=3*50+2;groupDividers.push_back(x-2);x+=4;
  rowMinus.setBounds(x,r1,24,rh);rowPlus.setBounds(x+26,r1,24,rh);
  // 第二行：编辑命令 | 采样字段
  const int r2=60,rh2=24;const bool narrow=w<1180;
  const int fieldsW=w>=1440?414:(w>=1180?360:318);const int step=fieldsW/3,labelW=narrow?26:48,fieldW=step-labelW-2;
  const int fx=w-fieldsW;int bx=366;const int cmdW=std::clamp((fx-bx-8)/8-2,42,76);const int cmdIds[]={5,6,0,1,2,3,4,13};
  for(int id:cmdIds){auto& b=*buttons[size_t(id)];b.setVisible(true);b.setComponentID("cmd-"+juce::String(id));b.setBounds(bx,r2,cmdW,rh2);bx+=cmdW+2;}
  const char* shortLabels[]={"起点","终点","光标"};const char* fullLabels[]={"起点 · 采样","终点 · 采样","光标 · 采样"};
  int fieldX=fx;
  for(int i=0;i<3;++i){auto* field=i==0?&start:i==1?&end:&cursor;field->setVisible(true);auto& lab=fieldLabels[size_t(i)];lab.setVisible(true);
   lab.setText(zh(narrow?shortLabels[i]:fullLabels[i]),juce::dontSendNotification);lab.setBounds(fieldX,r2+1,labelW,22);
   field->setBounds(fieldX+labelW+2,r2,fieldW,rh2);fieldX+=step;}
  // 主区
  trackViewport.setBounds(left+2,top+head,controlWidth,workH);
  viewport.setBounds(timelineX,top+head,std::max(200,clipX-timelineX-4),workH);
  ruler.setBounds(timelineX,top,std::max(200,clipX-timelineX-4),head);
  strips.setSize(controlWidth,timeline.getHeight()+(viewport.getHorizontalScrollBar().isVisible()?viewport.getScrollBarThickness():0));
  for(int i=0;i<int(rows.size());++i)rows[size_t(i)]->setBounds(0,i*EditStyle::rowHeight,controlWidth,EditStyle::rowHeight);
  navViewport.setVisible(showLeft);navViewport.setBounds(3,top+28,std::max(0,left-7),std::max(120,workH-120));
  nav.setSize(std::max(100,left-22),nav.getHeight());
  clipViewport.setVisible(showRight);clipViewport.setBounds(clipX+5,top+28,std::max(0,right-10),head+workH-31);
  clipList.setSize(std::max(100,right-26),clipList.getHeight());
  // 底栏
  for(auto* c:std::initializer_list<juce::Component*>{&gain,&fadeIn,&fadeOut,&trackDb,&trackPan,&masterDb,&sendDb,&mute,&solo,&preSend,&outputTrack,&applyMix,&routeButton,&sendButton,&exportDepth,&cancelExport})c->setVisible(false);
  for(int i=3;i<10;++i)fieldLabels[size_t(i)].setVisible(false);
  for(int id:{10,11,12,17})buttons[size_t(id)]->setVisible(false);
  const int by=h-bottom;
  properties.setBounds(4,h-26,84,23);routing.setBounds(90,h-26,64,23);tasks.setBounds(156,h-26,54,23);
  status.setBounds(216,h-27,std::max(60,w-424),25);
  properties.setToggleState(bottomMode==1,juce::dontSendNotification);
  routing.setToggleState(bottomMode==3,juce::dontSendNotification);
  tasks.setToggleState(bottomMode==2,juce::dontSendNotification);
  if(bottomMode==1){
    juce::TextEditor* f[]={&gain,&fadeIn,&fadeOut};const int fw=132;int x2=12;
    for(int i=0;i<3;++i){auto& lab=fieldLabels[size_t(i+3)];lab.setVisible(true);lab.setBounds(x2,by+10,fw,20);f[i]->setVisible(true);f[i]->setBounds(x2,by+32,fw,27);x2+=fw+10;}
    const int ids[]={10,11,12,17};int bx2=x2+10;int bw=std::clamp((w-bx2-12)/4-6,84,140);
    for(int i=0;i<4;++i){auto& b=*buttons[size_t(ids[i])];b.setVisible(true);b.setBounds(bx2+i*(bw+6),by+32,bw,27);}
  }
  if(bottomMode==2){exportDepth.setVisible(true);cancelExport.setVisible(true);exportDepth.setBounds(16,by+34,155,29);cancelExport.setBounds(185,by+34,130,29);}
  if(bottomMode==3){
    juce::TextEditor* f[]={&trackDb,&trackPan,&masterDb,&sendDb};const int fw=104;int x2=12;
    for(int i=0;i<4;++i){auto& lab=fieldLabels[size_t(i+6)];lab.setVisible(true);lab.setBounds(x2,by+8,fw,20);f[i]->setVisible(true);f[i]->setBounds(x2,by+30,fw,26);x2+=fw+8;}
    int bx2=x2+16;int bw=std::clamp((w-bx2-12)/7-6,70,120);
    for(auto* c:std::initializer_list<juce::Component*>{&mute,&solo,&preSend,&outputTrack,&applyMix,&routeButton,&sendButton}){
      const int cw=(c==&outputTrack)?std::min(150,bw+46):bw;
      c->setVisible(true);c->setBounds(bx2,by+30,cw,27);bx2+=cw+6;}
  }
  ruler.scrollX=viewport.getViewPositionX();repaint();
 }

 void paint(juce::Graphics& g)override{
  const int w=getWidth(),h=getHeight(),left=showLeft?leftWidth:0,right=showRight?rightWidth:0;
  const int top=EditStyle::topHeight,head=EditStyle::rulerHeight;
  g.fillAll(EditStyle::bg);
  // 顶区：菜单条 + 工具区
  g.setColour(EditStyle::panel);g.fillRect(0,0,w,top);
  g.setColour(EditStyle::edge);g.drawHorizontalLine(25,0,float(w));g.drawHorizontalLine(top-1,0,float(w));
  for(int x:groupDividers)g.drawVerticalLine(x,29,float(top-4));
  // 轨道控制列表头
  g.setColour(EditStyle::ruler);g.fillRect(left+2,top,EditStyle::controlWidth,head);
  g.setColour(EditStyle::edge);g.drawHorizontalLine(top+head-1,float(left),float(left+EditStyle::controlWidth));
  g.setColour(EditStyle::muted);g.setFont(10);
  g.drawText(zh("轨道"),left+12,top,116,head,juce::Justification::left);
  g.drawText(zh("输入/输出"),left+142,top,72,head,juce::Justification::left);
  g.drawText(zh("插入 A–E"),left+220,top,EditStyle::controlWidth-224,head,juce::Justification::left);
  // 左侧面板标题
  if(showLeft){g.setColour(EditStyle::text);g.setFont(11);g.drawText(zh("轨道"),7,top,left-14,26,juce::Justification::left);
    const int gy=navViewport.getBottom()+6;g.setColour(EditStyle::panel);g.fillRect(3,gy,left-6,22);
    g.setColour(EditStyle::text);g.drawText(zh("编组"),9,gy,left-16,22,juce::Justification::left);
    g.setColour(EditStyle::muted);g.drawText(zh("无编组"),13,gy+26,left-22,22,juce::Justification::left);}
  // 右侧片段列表标题
  if(showRight){g.setColour(EditStyle::text);g.setFont(11);g.drawText(zh("片段"),w-right+8,top,right-16,26,juce::Justification::left);}
  // 面板分隔
  g.setColour(EditStyle::edge);
  if(showLeft)g.fillRect(left-1,top,3,h-top-EditStyle::statusHeight);
  if(showRight)g.fillRect(w-right-2,top,3,h-top-EditStyle::statusHeight);
  g.drawHorizontalLine(h-EditStyle::statusHeight,0,float(w));
  // 底栏右侧常驻信息
  g.setColour(EditStyle::muted);g.setFont(10);
  g.drawText(zh("48 kHz · 24 bit · 离线编辑"),w-190,h-EditStyle::statusHeight,182,EditStyle::statusHeight,juce::Justification::right);
  const int by=h-(bottomMode?146:EditStyle::statusHeight);
  g.setColour(EditStyle::edge);g.drawHorizontalLine(by,0,float(w));
  if(bottomMode==2){g.setColour(EditStyle::text);g.drawText(exportJob.active?zh("后台任务正在执行，进度见状态栏"):zh("当前没有运行中的导出任务"),335,by+35,600,27,juce::Justification::left);}
 }

 void mouseMove(const juce::MouseEvent& e)override{bool divider=(showLeft&&std::abs(e.x-leftWidth)<5)||(showRight&&std::abs(e.x-(getWidth()-rightWidth))<5);setMouseCursor(divider?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);}
 void mouseDown(const juce::MouseEvent& e)override{dragDivider=showLeft&&std::abs(e.x-leftWidth)<6?1:showRight&&std::abs(e.x-(getWidth()-rightWidth))<6?2:0;}
 void mouseDrag(const juce::MouseEvent& e)override{if(dragDivider==1)leftWidth=std::clamp(e.x,130,260);if(dragDivider==2)rightWidth=std::clamp(getWidth()-e.x,145,320);if(dragDivider)resized();}
};
