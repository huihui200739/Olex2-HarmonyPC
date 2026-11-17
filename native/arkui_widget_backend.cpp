// Genuine native node implementation; host checks never substitute this backend.
#include "native_widget.h"
#include "native_text_positions.h"
#include "eutf8.h"
#include "arkui_choice_popup.h"
#include "arkui_input_events.h"
#include "arkui_list_box.h"
#include "arkui_picker_popup.h"
#include "arkui_button_image.h"
#include "arkui_tree.h"
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
namespace olex2::harmony::ui {
namespace {
void Require(int32_t result,const char* action) {
  if(result)throw std::runtime_error(std::string("ArkUI ")+action+" failed ("+std::to_string(result)+")");
}
bool IsInput(NodeKind kind){return kind==NodeKind::TextInput||kind==NodeKind::TextArea;}
ArkUI_NodeAttributeType NumberAttribute(Property property) {
  switch(property) {
    case Property::Width:return NODE_WIDTH;case Property::Height:return NODE_HEIGHT;
    case Property::Left:case Property::Top:return NODE_POSITION;
    case Property::TranslateX:case Property::TranslateY:return NODE_TRANSLATE;
    case Property::Background:return NODE_BACKGROUND_COLOR;case Property::Enabled:return NODE_ENABLED;
    case Property::Visible:return NODE_VISIBILITY;case Property::Focusable:return NODE_FOCUSABLE;
    case Property::FontColor:return NODE_FONT_COLOR;case Property::FontSize:return NODE_FONT_SIZE;
    case Property::SliderValue:return NODE_SLIDER_VALUE;case Property::SliderMinimum:return NODE_SLIDER_MIN_VALUE;
    case Property::SliderMaximum:return NODE_SLIDER_MAX_VALUE;case Property::SliderStep:return NODE_SLIDER_STEP;
    case Property::SliderVertical:return NODE_SLIDER_DIRECTION;case Property::SliderTrackColor:return NODE_SLIDER_TRACK_COLOR;
    case Property::SliderSelectedColor:return NODE_SLIDER_SELECTED_COLOR;
    case Property::Checked:return NODE_CHECKBOX_SELECT;case Property::TextAlign:return NODE_TEXT_ALIGN;
    case Property::BorderWidth:return NODE_BORDER_WIDTH;case Property::BorderColor:return NODE_BORDER_COLOR;case Property::BorderRadius:return NODE_BORDER_RADIUS;
    case Property::LayoutWeight:return NODE_LAYOUT_WEIGHT;case Property::Focused:return NODE_FOCUS_STATUS;case Property::Padding:return NODE_PADDING;case Property::ButtonRectangular:return NODE_BUTTON_TYPE;case Property::ImageStretch:return NODE_IMAGE_OBJECT_FIT;
    case Property::TreeScrollX:case Property::TreeScrollY:case Property::ChoiceSelection:case Property::RowSpacing:case Property::Editable:case Property::CompactOptions:case Property::PickerReadOnly:case Property::PickerAllowEmpty:break;
  }
  throw std::invalid_argument("Attribute is managed by its native control implementation");
}
ArkUI_NodeAttributeType StringAttribute(NodeKind kind,TextProperty property) {
  switch(property) {
    case TextProperty::Content:
      if(kind==NodeKind::TextInput)return NODE_TEXT_INPUT_TEXT;
      if(kind==NodeKind::TextArea)return NODE_TEXT_AREA_TEXT;
      if(kind==NodeKind::Button||kind==NodeKind::Choice)return NODE_BUTTON_LABEL;
      return NODE_TEXT_CONTENT;
    case TextProperty::Accessibility:return NODE_ACCESSIBILITY_TEXT;
    case TextProperty::Id:return NODE_ID;
  }
  throw std::invalid_argument("Unknown text attribute");
}
ArkUI_NodeEventType NativeEvent(NodeKind node,EventKind kind) {
  switch(kind) {
    case EventKind::Click:return NODE_ON_CLICK;case EventKind::Pointer:return NODE_TOUCH_EVENT;
    case EventKind::SliderChange:return NODE_SLIDER_EVENT_ON_CHANGE;
    case EventKind::CheckChange:return NODE_CHECKBOX_EVENT_ON_CHANGE;
    case EventKind::Focus:return NODE_ON_FOCUS;case EventKind::Blur:return NODE_ON_BLUR;case EventKind::Hover:return NODE_ON_HOVER;
    case EventKind::TextChange:
      if(IsInput(node))return node==NodeKind::TextArea?NODE_TEXT_AREA_ON_CHANGE:NODE_TEXT_INPUT_ON_CHANGE;
      break;
    case EventKind::Submit:
      if(IsInput(node))return node==NodeKind::TextArea?NODE_TEXT_AREA_ON_SUBMIT:NODE_TEXT_INPUT_ON_SUBMIT;
      break;
    case EventKind::Key:return NODE_ON_KEY_EVENT;
    case EventKind::TreeSelect:case EventKind::TreeToggle:case EventKind::TreeActivate:case EventKind::TreeContext:case EventKind::TreeEdit:case EventKind::ChoiceChange:case EventKind::DoubleClick:case EventKind::SpinStep:case EventKind::BackendError:case EventKind::DateChange:case EventKind::ColorChange:break;
  }
  throw std::invalid_argument("Event has no raw native-node equivalent for this control");
}
struct Record {
  ArkUI_NodeHandle handle=nullptr;
  ArkUI_NativeNodeAPI_1* api=nullptr;
  NodeKind kind=NodeKind::Row;
  std::unique_ptr<ArkUiChoicePopup> choice;
  std::unique_ptr<ArkUiListBox> list;
  std::unique_ptr<ArkUiTree> tree;
  std::unique_ptr<ArkUiPickerPopup> picker;
  std::unique_ptr<ArkUiDoubleClick> doubleClick;
  std::map<Property,double> numbers;
  std::map<TextProperty,std::string> strings;
  std::vector<ArkUI_NodeEventType> events;
  std::vector<ArkUI_NodeHandle> children;
  std::function<void(const NodeEvent&)> receiver;
  ImageRef image;
  bool writingText=false;
  TextSelection selection;bool selectionKnown=false,programmaticCaret=false;
};
void WriteInput(Record& record,const std::string& text) {
  const bool wasWriting=record.writingText;record.writingText=true;
  try {ArkUI_AttributeItem item{};item.string=text.c_str();Require(record.api->setAttribute(record.handle,StringAttribute(record.kind,TextProperty::Content),&item),"write native input");record.writingText=wasWriting;}
  catch(...){record.writingText=wasWriting;throw;}
}
class ArkUiNodes final:public NodeBackend {
  ArkUI_NativeNodeAPI_1* api_=nullptr;
  std::thread::id uiThread_;
  std::map<NodeHandle,std::unique_ptr<Record>> records_;
  ArkUI_NativeNodeAPI_1& Api() {
    if(uiThread_==std::thread::id())uiThread_=std::this_thread::get_id();
    if(uiThread_!=std::this_thread::get_id())throw std::runtime_error("ArkUI controls require their creating UI thread");
    if(!api_)OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE,ArkUI_NativeNodeAPI_1,api_);
    if(!api_)throw std::runtime_error("ArkUI native node interface is unavailable");return *api_;
  }
  Record& Find(NodeHandle node)const {auto i=records_.find(node);if(i==records_.end())throw std::invalid_argument("Unknown owned native control");return *i->second;}
  static void Report(const std::function<void(const NodeEvent&)>& receiver,const char* error)noexcept {
    try{if(receiver){NodeEvent converted{EventKind::BackendError};converted.text=error;receiver(converted);}}catch(...){}
  }
  static void Receive(ArkUI_NodeEvent* event)noexcept {
    std::function<void(const NodeEvent&)> receiver;
    try {
      if(!event)return;auto* record=static_cast<Record*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!record)return;
      receiver=record->receiver;const auto type=OH_ArkUI_NodeEvent_GetEventType(event);
      if(type==NODE_TEXT_INPUT_ON_WILL_INSERT||type==NODE_TEXT_INPUT_ON_WILL_DELETE||type==NODE_TEXT_AREA_ON_WILL_INSERT||type==NODE_TEXT_AREA_ON_WILL_DELETE) {
        // Real synchronous platform veto. Programmatic SetValue remains allowed.
        ArkUI_NumberValue result{};result.i32=record->writingText||record->numbers.at(Property::Editable)!=0;
        Require(OH_ArkUI_NodeEvent_SetReturnNumberValue(event,&result,1),"native readonly insert/delete veto");return;
      }
      if(type==NODE_TEXT_INPUT_ON_CHANGE||type==NODE_TEXT_AREA_ON_CHANGE) {
        if(record->writingText)return;const auto* payload=OH_ArkUI_NodeEvent_GetStringAsyncEvent(event);
        if(!payload||!payload->pStr)throw std::runtime_error("Native input change has no string payload");
        NodeEvent converted{EventKind::TextChange};converted.text=payload->pStr;
        if(record->numbers.at(Property::Editable)==0) {
          // Backstop for native edits not routed through the IME will-insert /
          // will-delete hooks. Restore the actual node, not just its cached model.
          WriteInput(*record,record->strings[TextProperty::Content]);return;
        }
        auto& previous=record->strings[TextProperty::Content];if(previous==converted.text)return;previous=converted.text;
        if(receiver)receiver(converted);return;
      }
      if(type==NODE_TEXT_INPUT_ON_TEXT_SELECTION_CHANGE||type==NODE_TEXT_AREA_ON_TEXT_SELECTION_CHANGE){
        const auto* payload=OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);
        if(!payload||payload->data[0].i32<0||payload->data[1].i32<0)throw std::runtime_error("Native text selection event is invalid");
        record->selection={payload->data[0].i32,payload->data[1].i32};record->selectionKnown=true;record->programmaticCaret=false;return;
      }
      if(!receiver)return;
      if(type==NODE_ON_KEY_EVENT){DispatchNativeKeyboard(event,receiver);return;}
      if(type==NODE_TOUCH_EVENT){DispatchNativeButtonPointer(event,record->handle,receiver);return;}
      if(type==NODE_ON_CLICK){receiver(NodeEvent{EventKind::Click});return;}
      if(type==NODE_ON_FOCUS){receiver(NodeEvent{EventKind::Focus});return;}
      if(type==NODE_ON_BLUR){receiver(NodeEvent{EventKind::Blur});return;}
      if(type==NODE_TEXT_INPUT_ON_SUBMIT||type==NODE_TEXT_AREA_ON_SUBMIT){receiver(NodeEvent{EventKind::Submit});return;}
      if(type==NODE_CHECKBOX_EVENT_ON_CHANGE||type==NODE_ON_HOVER) {
        const auto* data=OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);if(!data)return;
        receiver(NodeEvent{type==NODE_ON_HOVER?EventKind::Hover:EventKind::CheckChange,static_cast<double>(data->data[0].i32)});return;
      }
      if(type==NODE_SLIDER_EVENT_ON_CHANGE) {
        const auto* data=OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);if(!data)return;
        const auto phase=data->data[1].i32;if(phase<0||phase>3)return;
        receiver(NodeEvent{EventKind::SliderChange,data->data[0].f32,static_cast<SliderPhase>(phase)});return;
      }
    }catch(const std::exception& e){Report(receiver,e.what());}catch(...){Report(receiver,"Native control event conversion failed");}
    // The original callback may release its owner. No Record access follows it.
  }
  void SetGap(ArkUI_NodeHandle child,NodeKind parent,double gap) {
    ArkUI_NumberValue values[4]{};values[parent==NodeKind::Column?2:1].f32=static_cast<float>(gap);
    ArkUI_AttributeItem item{values,4,nullptr,nullptr};Require(Api().setAttribute(child,NODE_MARGIN,&item),"set native row/column gap");
  }
public:
  NodeHandle Create(NodeKind kind)override {
    auto& api=Api();ArkUI_NodeType type=ARKUI_NODE_ROW;
    switch(kind) {
      case NodeKind::Row:type=ARKUI_NODE_ROW;break;case NodeKind::Column:type=ARKUI_NODE_COLUMN;break;
      case NodeKind::Image:type=ARKUI_NODE_IMAGE;break;case NodeKind::Text:type=ARKUI_NODE_TEXT;break;case NodeKind::Slider:type=ARKUI_NODE_SLIDER;break;
      case NodeKind::CheckBox:type=ARKUI_NODE_CHECKBOX;break;case NodeKind::Choice:case NodeKind::Button:case NodeKind::DateField:case NodeKind::ColorField:type=ARKUI_NODE_BUTTON;break;
      case NodeKind::TextInput:type=ARKUI_NODE_TEXT_INPUT;break;case NodeKind::TextArea:type=ARKUI_NODE_TEXT_AREA;break;
      case NodeKind::Scroll:case NodeKind::Tree:case NodeKind::ListBox:type=ARKUI_NODE_SCROLL;break;
      default:throw std::invalid_argument("Unknown native node kind");
    }
    auto record=std::make_unique<Record>();record->handle=api.createNode(type);if(!record->handle)throw std::runtime_error("ArkUI createNode returned no node");
    record->api=&api;record->kind=kind;auto handle=static_cast<NodeHandle>(record->handle);
    record->numbers={{Property::Width,0},{Property::Height,0},{Property::Left,0},{Property::Top,0},{Property::Enabled,1},{Property::Visible,1},{Property::Editable,1},{Property::Background,0},{Property::SliderValue,0},{Property::SliderMinimum,0},{Property::SliderMaximum,100},{Property::SliderStep,1},{Property::SliderVertical,0},{Property::Checked,0},{Property::ChoiceSelection,-1},{Property::RowSpacing,0}};
    try {
      auto* stable=record.get();
      if(kind==NodeKind::Choice)record->choice=std::make_unique<ArkUiChoicePopup>(api,record->handle,[stable](int selected){auto receiver=stable->receiver;if(receiver)receiver(NodeEvent{EventKind::ChoiceChange,static_cast<double>(selected)});});
      if(kind==NodeKind::ListBox)record->list=std::make_unique<ArkUiListBox>(api,record->handle,[stable](EventKind kind,int selected){auto receiver=stable->receiver;if(receiver)receiver(NodeEvent{kind,static_cast<double>(selected)});});
      if(kind==NodeKind::DateField||kind==NodeKind::ColorField)record->picker=std::make_unique<ArkUiPickerPopup>(api,record->handle,kind==NodeKind::DateField?ArkUiPickerPopup::Kind::Date:ArkUiPickerPopup::Kind::Color,[stable](const NodeEvent& event){auto receiver=stable->receiver;if(receiver)receiver(event);});
      if(kind==NodeKind::Tree)record->tree=std::make_unique<ArkUiTree>(api,record->handle,[stable](const NodeEvent& event){auto receiver=stable->receiver;if(receiver)receiver(event);});
      records_.emplace(handle,std::move(record));
    }catch(...){record.reset();api.disposeNode(static_cast<ArkUI_NodeHandle>(handle));throw;}
    return handle;
  }
  void Destroy(NodeHandle node)noexcept override {
    auto i=records_.find(node);if(i==records_.end())return;Unsubscribe(node);i->second->choice.reset();i->second->list.reset();i->second->picker.reset();i->second->tree.reset();api_->disposeNode(i->second->handle);records_.erase(i);
  }
  void AddChild(NodeHandle parent,NodeHandle child)override {
    auto& p=Find(parent);auto c=Find(child).handle;p.children.reserve(p.children.size()+1);Require(Api().addChild(p.handle,c),"addChild");p.children.push_back(c);
    try{if(p.children.size()>1)SetGap(p.children[p.children.size()-2],p.kind,p.numbers.at(Property::RowSpacing));}
    catch(...){p.children.pop_back();Api().removeChild(p.handle,c);throw;}
  }
  void RemoveChild(NodeHandle parent,NodeHandle child)override {
    auto& p=Find(parent);auto c=Find(child).handle;Require(Api().removeChild(p.handle,c),"removeChild");p.children.erase(std::remove(p.children.begin(),p.children.end(),c),p.children.end());if(!p.children.empty())SetGap(p.children.back(),p.kind,0);
  }
  void SetNumber(NodeHandle node,Property property,double number)override {
    if(!std::isfinite(number))throw std::invalid_argument("Native number is not finite");auto& record=Find(node);Api();
    if(property==Property::TreeScrollX||property==Property::TreeScrollY){if(!record.tree)throw std::invalid_argument("Tree scroll property requires a real tree");record.tree->SetScroll(property==Property::TreeScrollX,number);return;}
    if(property==Property::Width&&record.tree){const double old=record.numbers.at(Property::Width);record.tree->SetViewportWidth(static_cast<float>(number));ArkUI_NumberValue n{};n.f32=static_cast<float>(number);ArkUI_AttributeItem a{&n,1,nullptr,nullptr};try{Require(Api().setAttribute(record.handle,NODE_WIDTH,&a),"tree width");}catch(...){try{record.tree->SetViewportWidth(old>0?old:480);}catch(...){}throw;}record.numbers[property]=number;return;}
    if(property==Property::PickerReadOnly||property==Property::PickerAllowEmpty){if(!record.picker)throw std::invalid_argument("Picker flags require a real native calendar/color control");if(property==Property::PickerReadOnly)record.picker->SetReadOnly(number!=0);else record.picker->SetAllowEmpty(number!=0);record.numbers[property]=number!=0;return;}
    if(property==Property::ChoiceSelection) {
      if((!record.choice&&!record.list)||std::floor(number)!=number||number < -1||number>std::numeric_limits<int>::max())throw std::invalid_argument("Invalid native option selection");
      if(record.choice)record.choice->SetSelected(static_cast<int>(number));else record.list->SetSelected(static_cast<int>(number));record.numbers[property]=number;return;
    }
    if(property==Property::CompactOptions) {if(!record.choice)throw std::invalid_argument("Compact options require a real native choice");record.choice->SetCompact(number!=0);record.numbers[property]=number;return;}
    if(property==Property::Editable) {if(!IsInput(record.kind))throw std::invalid_argument("Editable requires a native text node");record.numbers[property]=number!=0;return;}
    if(property==Property::RowSpacing) {for(size_t i=0;i<record.children.size();++i)SetGap(record.children[i],record.kind,i+1<record.children.size()?number:0);record.numbers[property]=number;return;}
    ArkUI_NumberValue values[3]{};ArkUI_AttributeItem item{values,1,nullptr,nullptr};
    switch(property) {
      case Property::Left:case Property::Top:
        values[0].f32=static_cast<float>(property==Property::Left?number:record.numbers[Property::Left]);values[1].f32=static_cast<float>(property==Property::Top?number:record.numbers[Property::Top]);item.size=2;break;
      case Property::TranslateX:case Property::TranslateY:
        values[0].f32=static_cast<float>(property==Property::TranslateX?number:record.numbers[Property::TranslateX]);values[1].f32=static_cast<float>(property==Property::TranslateY?number:record.numbers[Property::TranslateY]);values[2].f32=0;item.size=3;break;
      case Property::ButtonRectangular:values[0].i32=number!=0?ARKUI_BUTTON_TYPE_NORMAL:ARKUI_BUTTON_TYPE_CAPSULE;break;
      case Property::ImageStretch:values[0].i32=number!=0?ARKUI_OBJECT_FIT_FILL:ARKUI_OBJECT_FIT_CONTAIN;break;
      case Property::Enabled:case Property::Focusable:case Property::Focused:case Property::Checked:values[0].i32=number!=0;break;
      case Property::Visible:values[0].i32=number!=0?ARKUI_VISIBILITY_VISIBLE:ARKUI_VISIBILITY_NONE;break;
      case Property::TextAlign:values[0].i32=number!=0?ARKUI_TEXT_ALIGNMENT_END:ARKUI_TEXT_ALIGNMENT_START;break;
      case Property::SliderVertical:values[0].i32=number!=0?ARKUI_SLIDER_DIRECTION_VERTICAL:ARKUI_SLIDER_DIRECTION_HORIZONTAL;break;
      case Property::Background:case Property::FontColor:case Property::SliderTrackColor:case Property::SliderSelectedColor:case Property::BorderColor:values[0].u32=static_cast<uint32_t>(number);break;
      default:values[0].f32=static_cast<float>(number);break;
    }
    Require(Api().setAttribute(record.handle,NumberAttribute(property),&item),"setAttribute");record.numbers[property]=number;
  }
  Size Measure(NodeHandle node)const override {const auto& record=Find(node);if(uiThread_!=std::this_thread::get_id())throw std::runtime_error("Measured native geometry requires the UI thread");const auto size=api_->getMeasuredSize(record.handle);return {size.width,size.height};}
  double GetNumber(NodeHandle node,Property property)const override {
    const auto& record=Find(node);
    if(property==Property::TreeScrollX||property==Property::TreeScrollY){if(!record.tree)throw std::invalid_argument("Tree scroll property requires a real tree");return record.tree->Scroll(property==Property::TreeScrollX);}
    if(property==Property::ChoiceSelection){if(record.choice)return record.choice->Selected();if(record.list)return record.list->Selected();throw std::invalid_argument("Node is not a native option control");}
    if(property==Property::Checked){const auto* value=api_->getAttribute(record.handle,NODE_CHECKBOX_SELECT);if(value&&value->value&&value->size>0)return value->value[0].i32;}
    if(property==Property::SliderValue){const auto* value=api_->getAttribute(record.handle,NODE_SLIDER_VALUE);if(value&&value->value&&value->size>0)return value->value[0].f32;}
    auto value=record.numbers.find(property);return value==record.numbers.end()?0:value->second;
  }
  void SetText(NodeHandle node,TextProperty property,const std::string& text)override {
    auto& record=Find(node);Api();if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Native text contains an embedded NUL");
    if(property==TextProperty::Content&&record.picker){record.picker->SetValue(text);record.strings[property]=text;return;}
    if(property==TextProperty::Content&&IsInput(record.kind)) {
      auto old=record.strings[property];const auto oldRange=record.selection;const bool oldKnown=record.selectionKnown;record.strings[property]=text;try{WriteInput(record,text);record.selectionKnown=false;}catch(...){record.strings[property]=std::move(old);record.selection=oldRange;record.selectionKnown=oldKnown;throw;}return;
    }
    ArkUI_AttributeItem item{};item.string=text.c_str();Require(Api().setAttribute(record.handle,StringAttribute(record.kind,property),&item),"set text");record.strings[property]=text;
  }
  std::string GetText(NodeHandle node,TextProperty property)const override {
    const auto& record=Find(node);if(property==TextProperty::Content&&record.picker)return record.picker->Value();const auto* value=api_->getAttribute(record.handle,StringAttribute(record.kind,property));if(value&&value->string)return value->string;auto p=record.strings.find(property);return p==record.strings.end()?std::string():p->second;
  }
  void Subscribe(NodeHandle node,const std::vector<EventKind>& kinds,std::function<void(const NodeEvent&)> receiver)override {
    auto& record=Find(node);Unsubscribe(node);Require(Api().addNodeEventReceiver(record.handle,Receive),"add event receiver");
    try {
      record.events.reserve(kinds.size()+3);record.receiver=std::move(receiver);
      auto subscribe=[&](ArkUI_NodeEventType type){if(std::find(record.events.begin(),record.events.end(),type)!=record.events.end())return;Require(Api().registerNodeEvent(record.handle,type,static_cast<int32_t>(type),&record),"register node event");record.events.push_back(type);};
      if(IsInput(record.kind)){subscribe(record.kind==NodeKind::TextArea?NODE_TEXT_AREA_ON_TEXT_SELECTION_CHANGE:NODE_TEXT_INPUT_ON_TEXT_SELECTION_CHANGE);subscribe(record.kind==NodeKind::TextArea?NODE_TEXT_AREA_ON_WILL_INSERT:NODE_TEXT_INPUT_ON_WILL_INSERT);subscribe(record.kind==NodeKind::TextArea?NODE_TEXT_AREA_ON_WILL_DELETE:NODE_TEXT_INPUT_ON_WILL_DELETE);}
      for(const auto kind:kinds) {
        if(kind==EventKind::ChoiceChange){if(!record.choice&&!record.list)throw std::invalid_argument("Choice change requires its real native implementation");continue;}
        if(kind==EventKind::DateChange||kind==EventKind::ColorChange){if(!record.picker||(kind==EventKind::DateChange)!=(record.kind==NodeKind::DateField))throw std::invalid_argument("Picker event requires its corresponding real native backend");continue;}
        if(kind==EventKind::TreeSelect||kind==EventKind::TreeToggle||kind==EventKind::TreeActivate||kind==EventKind::TreeContext||kind==EventKind::TreeEdit){if(!record.tree)throw std::invalid_argument("Tree events require their real renderer");continue;}
        if(kind==EventKind::BackendError)continue;
        if(kind==EventKind::DoubleClick) {
          if(!record.list&&!record.doubleClick){auto* stable=&record;record.doubleClick=std::make_unique<ArkUiDoubleClick>(record.handle,[stable]{auto receiver=stable->receiver;if(receiver)receiver(NodeEvent{EventKind::DoubleClick});});}continue;
        }
        if(kind==EventKind::Key) {
          // Hardware-key conversion is an optional platform extension. When the
          // API14 interface is absent, keep the base text widget usable and skip
          // key events instead of failing the whole dialog.
          if(!NativeKeyboardAvailable()) continue;
        }
        subscribe(NativeEvent(record.kind,kind));
      }
    }catch(...){Unsubscribe(node);throw;}
  }
  void SetTextSelection(NodeHandle node,TextSelection range)override{
    auto& record=Find(node);Api();if(!IsInput(record.kind))throw std::invalid_argument("Text selection requires a real native text node");
    const auto text=TUtf8::Decode(record.strings[TextProperty::Content].c_str());const auto maximum=TextUtf16Offset(text,text.Length());
    if(range.start<0||range.end<range.start||static_cast<std::size_t>(range.end)>maximum)throw std::out_of_range("Native selection exceeds its UTF-16 string");
    ArkUI_NumberValue values[2]{};values[0].i32=range.start;values[1].i32=range.end;ArkUI_AttributeItem item{values,2,nullptr,nullptr};
    Require(Api().setAttribute(record.handle,record.kind==NodeKind::TextArea?NODE_TEXT_AREA_TEXT_SELECTION:NODE_TEXT_INPUT_TEXT_SELECTION,&item),"set native text selection");
    record.selection=range;record.selectionKnown=true;record.programmaticCaret=true;
  }
  int32_t GetTextCaret(NodeHandle node)const override{
    const auto& record=Find(node);if(!IsInput(record.kind))throw std::invalid_argument("Caret requires a real native text node");
    // Immediate programmatic queries use the requested endpoint: the SDK
    // explicitly warns that its layout-based caret getter can lag one frame.
    if(record.selectionKnown&&(record.programmaticCaret||record.selection.start==record.selection.end))return record.selection.end;
    const auto* item=api_->getAttribute(record.handle,record.kind==NodeKind::TextArea?NODE_TEXT_AREA_CARET_OFFSET:NODE_TEXT_INPUT_CARET_OFFSET);
    if(!item||!item->value||item->size<1||item->value[0].i32<0)throw std::runtime_error("Native SDK active caret is unavailable");
    return item->value[0].i32;
  }
  TextSelection GetTextSelection(NodeHandle node)const override{
    const auto& record=Find(node);if(!IsInput(record.kind))throw std::invalid_argument("Text selection requires a real native text node");
    if(record.selectionKnown)return record.selection;
    const auto* item=api_->getAttribute(record.handle,record.kind==NodeKind::TextArea?NODE_TEXT_AREA_TEXT_SELECTION:NODE_TEXT_INPUT_TEXT_SELECTION);
    if(!item||!item->value||item->size<2||item->value[0].i32<0||item->value[1].i32<0)throw std::runtime_error("Native SDK text selection is unavailable");
    return {item->value[0].i32,item->value[1].i32};
  }
  void SetTreeRows(NodeHandle node,const std::vector<TreeRow>& rows)override{auto& record=Find(node);Api();if(!record.tree)throw std::invalid_argument("Node is not a native tree");record.tree->SetRows(rows);}
  ImageRef DecodeImage(const std::vector<uint8_t>& encoded)override{Api();return DecodeArkUiButtonImage(encoded);}
  void SetImage(NodeHandle node,const ImageRef& image)override{auto& record=Find(node);Api();if(record.kind!=NodeKind::Image)throw std::invalid_argument("Image pixels require a real ArkUI Image node");WriteArkUiButtonImage(*api_,record.handle,image);record.image=image;}
  void SetOptions(NodeHandle node,const std::vector<std::string>& values)override {auto& record=Find(node);Api();if(record.choice)record.choice->SetOptions(values);else if(record.list)record.list->SetOptions(values);else throw std::invalid_argument("Node is not a native option control");}
  void OpenOptions(NodeHandle node)override {auto& record=Find(node);Api();if(!record.choice)throw std::invalid_argument("Node has no native option popup");record.choice->Open();}
  void CloseOptions(NodeHandle node)override {auto& record=Find(node);Api();if(record.choice)record.choice->Close();else if(!record.list)throw std::invalid_argument("Node is not a native option control");}
  void OpenPicker(NodeHandle node)override{auto& record=Find(node);Api();if(!record.picker)throw std::invalid_argument("Node has no native calendar/color popup");record.picker->Open();}
  void ClosePicker(NodeHandle node)override{auto& record=Find(node);Api();if(!record.picker)throw std::invalid_argument("Node has no native calendar/color popup");record.picker->Close();}
  void Unsubscribe(NodeHandle node)noexcept override {auto i=records_.find(node);if(i==records_.end()||!api_)return;auto& record=*i->second;record.doubleClick.reset();for(auto type:record.events)api_->unregisterNodeEvent(record.handle,type);record.events.clear();record.receiver={};api_->removeNodeEventReceiver(record.handle,Receive);}
};
}
NodeBackend& ArkUiBackend(){static ArkUiNodes backend;return backend;}
}
