#include "arkui_list_box.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void Need(int32_t code,const char* name){if(code)throw std::runtime_error(std::string("ArkUI visible list ")+name+" failed ("+std::to_string(code)+")");}
void Float(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,float value){ArkUI_NumberValue number{};number.f32=value;ArkUI_AttributeItem item{&number,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"dimension");}
void Integer(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,int32_t value){ArkUI_NumberValue number{};number.i32=value;ArkUI_AttributeItem item{&number,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"integer attribute");}
}
ArkUiListBox::ArkUiListBox(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle root,std::function<void(EventKind,int)> receiver):api_(api),root_(root),receiver_(std::move(receiver)){Integer(api_,root_,NODE_FOCUS_ON_TOUCH,1);}
ArkUiListBox::~ArkUiListBox(){if(graph_){bool detached=!graph_->attached||api_.removeChild(root_,graph_->column)==0;Clear(*graph_,detached);}}
void ArkUiListBox::Clear(Graph& graph,bool dispose)noexcept{
  for(auto& row:graph.rows){row->doubleClick.reset();api_.unregisterNodeEvent(row->node,NODE_ON_CLICK);api_.removeNodeEventReceiver(row->node,Receive);if(dispose){api_.removeChild(graph.column,row->node);api_.disposeNode(row->node);}}
  graph.rows.clear();if(dispose&&graph.column)api_.disposeNode(graph.column);graph.column=nullptr;graph.attached=false;
}
std::unique_ptr<ArkUiListBox::Graph> ArkUiListBox::Build(const std::vector<std::string>& values){
  auto graph=std::make_unique<Graph>();
  try{
    graph->column=api_.createNode(ARKUI_NODE_COLUMN);if(!graph->column)throw std::runtime_error("Native visible list column creation failed");Float(api_,graph->column,NODE_WIDTH_PERCENT,1);graph->rows.reserve(values.size());
    for(size_t i=0;i<values.size();++i){
      auto row=std::make_unique<Item>();row->owner=this;row->index=static_cast<int>(i);row->node=api_.createNode(ARKUI_NODE_BUTTON);if(!row->node)throw std::runtime_error("Native visible list item creation failed");auto* stable=row.get();graph->rows.push_back(std::move(row));
      Float(api_,stable->node,NODE_WIDTH_PERCENT,1);Float(api_,stable->node,NODE_HEIGHT,32);Integer(api_,stable->node,NODE_FOCUSABLE,0);
      ArkUI_AttributeItem label{};label.string=values[i].c_str();Need(api_.setAttribute(stable->node,NODE_BUTTON_LABEL,&label),"label");Need(api_.addChild(graph->column,stable->node),"stage item");
      Need(api_.addNodeEventReceiver(stable->node,Receive),"item receiver");Need(api_.registerNodeEvent(stable->node,NODE_ON_CLICK,0,stable),"item click");
      stable->doubleClick=std::make_unique<ArkUiDoubleClick>(stable->node,[stable]{auto* owner=stable->owner;const int index=stable->index;owner->Emit(EventKind::DoubleClick,index);});
    }
    return graph;
  }catch(...){Clear(*graph,true);throw;}
}
void ArkUiListBox::SetOptions(const std::vector<std::string>& values){
  if(values.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Too many visible list items");for(const auto& text:values)if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Visible list label contains embedded NUL");
  auto next=Build(values);
  try{
    if(graph_&&graph_->attached){Need(api_.removeChild(root_,graph_->column),"replace old content");graph_->attached=false;}
    int32_t added=api_.addChild(root_,next->column);
    if(added){if(graph_){int32_t restored=api_.addChild(root_,graph_->column);graph_->attached=restored==0;if(restored)throw std::runtime_error("Native visible list replacement and old-content restoration both failed; owned nodes retained");}Need(added,"attach new content");}
    next->attached=true;
  }catch(...){Clear(*next,true);throw;}
  if(graph_)Clear(*graph_,true);graph_=std::move(next);if(selected_>=0&&static_cast<size_t>(selected_)>=values.size())selected_=-1;SetSelected(selected_);
}
void ArkUiListBox::SetSelected(int value){
  if(value < -1||(value>=0&&(!graph_||static_cast<size_t>(value)>=graph_->rows.size())))throw std::out_of_range("Visible list selection is outside its items");
  if(graph_)for(const auto& row:graph_->rows){ArkUI_NumberValue number{};number.u32=row->index==value?0xffdcecdf:0xfff8faf8;ArkUI_AttributeItem item{&number,1,nullptr,nullptr};Need(api_.setAttribute(row->node,NODE_BACKGROUND_COLOR,&item),"selected-item highlight");}
  if(value>=0){ArkUI_NumberValue offset[3]{};offset[1].f32=static_cast<float>(value)*32;offset[2].i32=0;ArkUI_AttributeItem item{offset,3,nullptr,nullptr};Need(api_.setAttribute(root_,NODE_SCROLL_OFFSET,&item),"selected-item visibility");}
  selected_=value;
}
void ArkUiListBox::Emit(EventKind kind,int index){auto callback=receiver_;if(callback)callback(kind,index);}
void ArkUiListBox::Receive(ArkUI_NodeEvent* event)noexcept{try{if(!event||OH_ArkUI_NodeEvent_GetEventType(event)!=NODE_ON_CLICK)return;auto* row=static_cast<Item*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!row||!row->owner)return;auto* owner=row->owner;int index=row->index;owner->Emit(EventKind::ChoiceChange,index);}catch(...){} }
}
