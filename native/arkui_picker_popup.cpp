#include "arkui_picker_popup.h"
#include <arkui/native_interface.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void Need(int32_t code,const char* name){if(code)throw std::runtime_error(std::string("ArkUI picker ")+name+" failed ("+std::to_string(code)+")");}
void Float(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,float value){ArkUI_NumberValue n{};n.f32=value;ArkUI_AttributeItem item{&n,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"numeric attribute");}
void Integer(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,int32_t value){ArkUI_NumberValue n{};n.i32=value;ArkUI_AttributeItem item{&n,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"integer attribute");}
void ColorAttribute(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,uint32_t value){ArkUI_NumberValue n{};n.u32=value;ArkUI_AttributeItem item{&n,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"color attribute");}
void Text(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,const std::string& text){ArkUI_AttributeItem item{};item.string=text.c_str();Need(api.setAttribute(node,key,&item),"text attribute");}
uint32_t Contrast(Color color){unsigned alpha=color.alpha;unsigned red=(color.red*alpha+250u*(255-alpha))/255,green=(color.green*alpha+250u*(255-alpha))/255,blue=(color.blue*alpha+250u*(255-alpha))/255;return 299*red+587*green+114*blue>=128000?0xff101510:0xfffafcfa;}
std::string TextEvent(ArkUI_NodeEvent* event){const auto* data=OH_ArkUI_NodeEvent_GetStringAsyncEvent(event);if(!data||!data->pStr)throw std::runtime_error("Native picker text event has no payload");return data->pStr;}
}
ArkUiPickerPopup::ArkUiPickerPopup(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle root,Kind kind,std::function<void(const NodeEvent&)> receiver)
 :api_(api),root_(root),kind_(kind),receiver_(std::move(receiver)){SetLabel();}
ArkUiPickerPopup::~ArkUiPickerPopup(){
  try{Close();}catch(...){
    // A failed detach retains platform nodes. Inactive binding records must also
    // remain alive if the platform could still hold their userdata addresses.
    for(auto& binding:bindings_){binding->owner=nullptr;for(auto type:binding->events)api_.unregisterNodeEvent(binding->node,type);if(binding->receiverAttached)api_.removeNodeEventReceiver(binding->node,Receive);binding.release();}
  }
}
ArkUI_NodeHandle ArkUiPickerPopup::Node(ArkUI_NodeType type,float width,float height){auto node=api_.createNode(type);if(!node)throw std::runtime_error("Native picker node creation failed");try{nodes_.push_back(node);}catch(...){api_.disposeNode(node);throw;}Float(api_,node,NODE_WIDTH,width);Float(api_,node,NODE_HEIGHT,height);return node;}
void ArkUiPickerPopup::Child(ArkUI_NodeHandle parent,ArkUI_NodeHandle child){edges_.emplace_back(parent,child);auto code=api_.addChild(parent,child);if(code)edges_.pop_back();Need(code,"attach dialog child");}
void ArkUiPickerPopup::Bind(ArkUI_NodeHandle node,Action action,int index,std::initializer_list<ArkUI_NodeEventType> events){
  auto binding=std::make_unique<Binding>();binding->owner=this;binding->node=node;binding->action=action;binding->index=index;auto* stable=binding.get();bindings_.push_back(std::move(binding));stable->events.reserve(events.size());
  Need(api_.addNodeEventReceiver(node,Receive),"add dialog receiver");stable->receiverAttached=true;
  for(auto type:events){Need(api_.registerNodeEvent(node,type,static_cast<int32_t>(type),stable),"register dialog event");stable->events.push_back(type);}
}
ArkUI_NodeHandle ArkUiPickerPopup::Button(ArkUI_NodeHandle parent,const std::string& label,float width,Action action,int index){auto node=Node(ARKUI_NODE_BUTTON,width,36);Text(api_,node,NODE_BUTTON_LABEL,label);Text(api_,node,NODE_ACCESSIBILITY_TEXT,label);Child(parent,node);Bind(node,action,index,{NODE_ON_CLICK});return node;}
ArkUI_NodeHandle ArkUiPickerPopup::Label(ArkUI_NodeHandle parent,const std::string& text,float width,float height){auto node=Node(ARKUI_NODE_TEXT,width,height);Text(api_,node,NODE_TEXT_CONTENT,text);Child(parent,node);return node;}
void ArkUiPickerPopup::SetLabel(){
  if(kind_==Kind::Date){Text(api_,root_,NODE_BUTTON_LABEL,(date_.IsEmpty()?std::string("请选择日期"):date_.Iso())+" ▾");return;}
  auto value=ColorHex(color_);Text(api_,root_,NODE_BUTTON_LABEL,value+" ▾");ColorAttribute(api_,root_,NODE_BACKGROUND_COLOR,PackColor(color_));
  ColorAttribute(api_,root_,NODE_FONT_COLOR,Contrast(color_));
}
std::string ArkUiPickerPopup::Value()const{return kind_==Kind::Date?date_.Iso():ColorHex(color_);}
void ArkUiPickerPopup::SetValue(const std::string& text){
  if(kind_==Kind::Date){CalendarDate next;if(!CalendarDate::TryParse(text,next)||(!next.IsValid()&&!(next.IsEmpty()&&allowEmpty_)))throw std::invalid_argument("Invalid native calendar value");Close();auto old=date_;date_=next;try{SetLabel();}catch(...){date_=old;try{SetLabel();}catch(...){}throw;}return;}
  Color next;if(!TryColorHex(text,next))throw std::invalid_argument("Invalid native RGBA value");Close();auto old=color_;color_=next;try{SetLabel();}catch(...){color_=old;try{SetLabel();}catch(...){}throw;}
}
void ArkUiPickerPopup::SetReadOnly(bool value){if(value)Close();readOnly_=value;}
void ArkUiPickerPopup::SetAllowEmpty(bool value){if(kind_!=Kind::Date)throw std::invalid_argument("Only a native calendar can allow an empty value");if(value!=allowEmpty_)Close();if(!value&&allowEmpty_&&date_.IsEmpty())throw std::invalid_argument("Select a valid date before disabling empty dates");allowEmpty_=value;}
void ArkUiPickerPopup::Close(){
  if(closing_)return;closing_=true;
  try{
    if(dialog_){if(shown_){Need(dialogs_->close(dialog_),"close dialog");shown_=false;}if(contentAttached_){Need(dialogs_->removeContent(dialog_),"detach dialog content");contentAttached_=false;}dialogs_->dispose(dialog_);dialog_=nullptr;}
    for(auto& binding:bindings_){for(auto type:binding->events)api_.unregisterNodeEvent(binding->node,type);binding->events.clear();if(binding->receiverAttached){Need(api_.removeNodeEventReceiver(binding->node,Receive),"remove dialog receiver");binding->receiverAttached=false;}}
    while(!edges_.empty()){auto edge=edges_.back();Need(api_.removeChild(edge.first,edge.second),"detach owned dialog child");edges_.pop_back();}
    bindings_.clear();for(auto node=nodes_.rbegin();node!=nodes_.rend();++node)api_.disposeNode(*node);nodes_.clear();panel_=year_=month_=hex_=preview_=nullptr;days_.fill(nullptr);sliders_.fill(nullptr);channelValues_.fill(nullptr);closing_=false;
  }catch(...){closing_=false;throw;}
}
void ArkUiPickerPopup::Open(){
  Close();if(readOnly_)return;if(!dialogs_)OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_DIALOG,ArkUI_NativeDialogAPI_1,dialogs_);if(!dialogs_)throw std::runtime_error("Native picker dialog API12 is unavailable");
  try{
    nodes_.reserve(150);edges_.reserve(150);bindings_.reserve(64);panel_=Node(ARKUI_NODE_COLUMN,360,kind_==Kind::Date?360:340);ColorAttribute(api_,panel_,NODE_BACKGROUND_COLOR,0xfffafcfa);
    if(kind_==Kind::Date){view_=date_.IsValid()?date_:CalendarDate::Today();BuildCalendar();RefreshCalendar();}else{candidate_=color_;BuildColor();RefreshColor();}
    dialog_=dialogs_->create();if(!dialog_)throw std::runtime_error("Native picker dialog creation failed");Need(dialogs_->setContent(dialog_,panel_),"set dialog content");contentAttached_=true;Need(dialogs_->setAutoCancel(dialog_,true),"dialog cancellation");Need(dialogs_->show(dialog_,false),"show dialog");shown_=true;
  }catch(...){try{Close();}catch(...){}throw;}
}
void ArkUiPickerPopup::BuildCalendar(){
  Label(panel_,"选择日期",336,28);auto navigation=Node(ARKUI_NODE_ROW,336,36);Child(panel_,navigation);
  Button(navigation,"≪",34,Action::PreviousYear);year_=Node(ARKUI_NODE_TEXT_INPUT,72,36);Child(navigation,year_);Text(api_,year_,NODE_ACCESSIBILITY_TEXT,"公历年份，1至9999");Bind(year_,Action::Year,0,{NODE_TEXT_INPUT_ON_CHANGE,NODE_TEXT_INPUT_ON_SUBMIT,NODE_ON_BLUR});Button(navigation,"≫",34,Action::NextYear);
  Button(navigation,"‹",34,Action::PreviousMonth);month_=Label(navigation,"",80,36);Button(navigation,"›",34,Action::NextMonth);
  auto week=Node(ARKUI_NODE_ROW,336,26);Child(panel_,week);for(const char* value:{"一","二","三","四","五","六","日"})Label(week,value,48,26);
  for(int row=0;row<6;++row){auto line=Node(ARKUI_NODE_ROW,336,36);Child(panel_,line);for(int column=0;column<7;++column){int index=row*7+column;days_[static_cast<size_t>(index)]=Button(line,"",48,Action::Day,index);}}
  auto footer=Node(ARKUI_NODE_ROW,336,40);Child(panel_,footer);Button(footer,"今天",100,Action::Today);if(allowEmpty_)Button(footer,"不选择日期",120,Action::None);Button(footer,"取消",100,Action::Cancel);
}
void ArkUiPickerPopup::RefreshCalendar(){
  bool old=updating_;updating_=true;
  try{
    yearDraft_=std::to_string(view_.year);Text(api_,year_,NODE_TEXT_INPUT_TEXT,yearDraft_);Text(api_,month_,NODE_TEXT_CONTENT,std::to_string(view_.month)+" 月");CalendarDate first{view_.year,view_.month,1};int start=first.Weekday(),count=CalendarDate::DaysInMonth(view_.year,view_.month);
    for(size_t i=0;i<days_.size();++i){int day=static_cast<int>(i)-start+1;if(day<1||day>count)day=0;dayNumbers_[i]=day;Text(api_,days_[i],NODE_BUTTON_LABEL,day?std::to_string(day):std::string());Text(api_,days_[i],NODE_ACCESSIBILITY_TEXT,day?CalendarDate{view_.year,view_.month,day}.Iso():std::string());Integer(api_,days_[i],NODE_ENABLED,day!=0);Integer(api_,days_[i],NODE_FOCUSABLE,day!=0);bool selected=day&&date_.IsValid()&&date_.year==view_.year&&date_.month==view_.month&&date_.day==day;ColorAttribute(api_,days_[i],NODE_BACKGROUND_COLOR,selected?0xffb8d6bd:0xffeef3ee);}
    updating_=old;
  }catch(...){updating_=old;throw;}
}
void ArkUiPickerPopup::CommitYear(){int year=0;if(CalendarDate::TryYear(yearDraft_,year)){view_.year=year;view_.day=std::min(view_.day,CalendarDate::DaysInMonth(view_.year,view_.month));}RefreshCalendar();}
void ArkUiPickerPopup::BuildColor(){
  Label(panel_,"选择颜色与透明度",336,28);preview_=Label(panel_,"",320,44);
  constexpr const char* labels[]={"红","绿","蓝","透明"};
  for(int channel=0;channel<4;++channel){auto row=Node(ARKUI_NODE_ROW,336,44);Child(panel_,row);Label(row,labels[channel],44,36);auto slider=Node(ARKUI_NODE_SLIDER,224,36);sliders_[static_cast<size_t>(channel)]=slider;Float(api_,slider,NODE_SLIDER_MIN_VALUE,0);Float(api_,slider,NODE_SLIDER_MAX_VALUE,255);Float(api_,slider,NODE_SLIDER_STEP,1);Text(api_,slider,NODE_ACCESSIBILITY_TEXT,labels[channel]);Child(row,slider);Bind(slider,Action::ColorChannel,channel,{NODE_SLIDER_EVENT_ON_CHANGE});channelValues_[static_cast<size_t>(channel)]=Label(row,"",52,36);}
  hex_=Node(ARKUI_NODE_TEXT_INPUT,320,40);Text(api_,hex_,NODE_ACCESSIBILITY_TEXT,"十六进制颜色，RGB或RGBA");Child(panel_,hex_);Bind(hex_,Action::Hex,0,{NODE_TEXT_INPUT_ON_CHANGE,NODE_TEXT_INPUT_ON_SUBMIT,NODE_ON_BLUR});auto footer=Node(ARKUI_NODE_ROW,320,40);Child(panel_,footer);Button(footer,"确定",150,Action::ConfirmColor);Button(footer,"取消",150,Action::Cancel);
}
void ArkUiPickerPopup::RefreshColor(){
  bool old=updating_;updating_=true;
  try{uint8_t channels[]={candidate_.red,candidate_.green,candidate_.blue,candidate_.alpha};for(size_t i=0;i<4;++i){Float(api_,sliders_[i],NODE_SLIDER_VALUE,channels[i]);Text(api_,channelValues_[i],NODE_TEXT_CONTENT,std::to_string(channels[i]));}hexDraft_=ColorHex(candidate_);Text(api_,hex_,NODE_TEXT_INPUT_TEXT,hexDraft_);Text(api_,preview_,NODE_TEXT_CONTENT,"预览 "+hexDraft_);ColorAttribute(api_,preview_,NODE_BACKGROUND_COLOR,PackColor(candidate_));ColorAttribute(api_,preview_,NODE_FONT_COLOR,Contrast(candidate_));updating_=old;}catch(...){updating_=old;throw;}
}
void ArkUiPickerPopup::CommitHex(){Color next;if(TryColorHex(hexDraft_,next))candidate_=next;RefreshColor();}
void ArkUiPickerPopup::AcceptDate(CalendarDate value){auto receiver=receiver_;NodeEvent event{EventKind::DateChange};event.text=value.Iso();Close();if(receiver)receiver(event);}
void ArkUiPickerPopup::AcceptColor(){auto receiver=receiver_;NodeEvent event{EventKind::ColorChange,static_cast<double>(PackColor(candidate_))};Close();if(receiver)receiver(event);}
void ArkUiPickerPopup::Handle(Action action,int index,ArkUI_NodeEvent* event){
  if(closing_||updating_||readOnly_)return;auto type=OH_ArkUI_NodeEvent_GetEventType(event);
  switch(action){
    case Action::Cancel:Close();return;
    case Action::Day:{int day=index>=0&&static_cast<size_t>(index)<dayNumbers_.size()?dayNumbers_[static_cast<size_t>(index)]:0;if(!day)return;CommitYear();day=std::min(day,CalendarDate::DaysInMonth(view_.year,view_.month));AcceptDate(CalendarDate{view_.year,view_.month,day});return;}
    case Action::Today:AcceptDate(CalendarDate::Today());return;
    case Action::None:if(allowEmpty_)AcceptDate({});return;
    case Action::PreviousMonth:case Action::NextMonth:CommitYear();view_=view_.ShiftMonths(action==Action::PreviousMonth?-1:1);RefreshCalendar();return;
    case Action::PreviousYear:case Action::NextYear:CommitYear();view_=view_.ShiftYears(action==Action::PreviousYear?-1:1);RefreshCalendar();return;
    case Action::Year:if(type==NODE_TEXT_INPUT_ON_CHANGE)yearDraft_=TextEvent(event);else CommitYear();return;
    case Action::Hex:if(type==NODE_TEXT_INPUT_ON_CHANGE)hexDraft_=TextEvent(event);else CommitHex();return;
    case Action::ColorChannel:{const auto* data=OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);if(!data||index<0||index>3)return;float value=data->data[0].f32;if(!std::isfinite(value)||value<0||value>255)throw std::invalid_argument("Native color channel is outside 0..255");if(hexDraft_!=ColorHex(candidate_))CommitHex();uint8_t channels[]={candidate_.red,candidate_.green,candidate_.blue,candidate_.alpha};channels[index]=static_cast<uint8_t>(std::lround(value));candidate_={channels[0],channels[1],channels[2],channels[3]};RefreshColor();return;}
    case Action::ConfirmColor:CommitHex();AcceptColor();return;
  }
}
void ArkUiPickerPopup::Receive(ArkUI_NodeEvent* event)noexcept{
  std::function<void(const NodeEvent&)> receiver;
  try{if(!event)return;auto* binding=static_cast<Binding*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!binding||!binding->owner)return;auto* owner=binding->owner;auto action=binding->action;int index=binding->index;receiver=owner->receiver_;owner->Handle(action,index,event);}
  catch(const std::exception& e){try{if(receiver){NodeEvent error{EventKind::BackendError};error.text=e.what();receiver(error);}}catch(...){} }
  catch(...){try{if(receiver){NodeEvent error{EventKind::BackendError};error.text="Native picker event failed";receiver(error);}}catch(...){} }
}
}
