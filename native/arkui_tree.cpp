#include "arkui_tree.h"
#include <arkui/ui_input_event.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void NeedTree(int32_t code,const char* action){if(code)throw std::runtime_error(std::string("ArkUI tree ")+action+" failed ("+std::to_string(code)+")");}
void Float(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,float value){ArkUI_NumberValue n{};n.f32=value;ArkUI_AttributeItem a{&n,1,nullptr,nullptr};NeedTree(api.setAttribute(node,key,&a),"dimension");}
void Integer(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,uint32_t value){ArkUI_NumberValue n{};n.u32=value;ArkUI_AttributeItem a{&n,1,nullptr,nullptr};NeedTree(api.setAttribute(node,key,&a),"integer attribute");}
void Text(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,const std::string& value){ArkUI_AttributeItem a{};a.string=value.c_str();NeedTree(api.setAttribute(node,key,&a),"text");}
}
ArkUiTree::ArkUiTree(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle root,std::function<void(const NodeEvent&)> receiver):api_(api),root_(root),receiver_(std::move(receiver)){Integer(api_,root_,NODE_FOCUS_ON_TOUCH,1);}
ArkUiTree::~ArkUiTree(){if(graph_){const bool detached=!graph_->attached||api_.removeChild(root_,graph_->horizontal)==0;Clear(*graph_,detached);}}
void ArkUiTree::Clear(Graph& graph,bool dispose)noexcept {
  for(auto& item:graph.rows){item->doubleClick.reset();for(auto node:{item->label,item->toggle})if(node){for(auto event:{NODE_ON_CLICK,NODE_ON_MOUSE,NODE_TEXT_INPUT_ON_CHANGE,NODE_TEXT_INPUT_ON_SUBMIT,NODE_ON_BLUR})api_.unregisterNodeEvent(node,event);api_.removeNodeEventReceiver(node,Receive);}if(dispose){if(item->label){api_.removeChild(item->row,item->label);api_.disposeNode(item->label);}if(item->toggle){api_.removeChild(item->row,item->toggle);api_.disposeNode(item->toggle);}if(item->row){api_.removeChild(graph.column,item->row);api_.disposeNode(item->row);}}}
  graph.rows.clear();if(dispose){if(graph.horizontal&&graph.column)api_.removeChild(graph.horizontal,graph.column);if(graph.column)api_.disposeNode(graph.column);if(graph.horizontal)api_.disposeNode(graph.horizontal);}graph.column=graph.horizontal=nullptr;graph.attached=false;
}
std::unique_ptr<ArkUiTree::Graph> ArkUiTree::Build(const std::vector<TreeRow>& rows){
  if(rows.size()>1000000)throw std::length_error("Native tree row limit");float content=std::max(width_,480.0f);for(const auto& row:rows){if(row.depth>32767||row.label.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid native tree row");content=std::max(content,std::min(32767.0f,static_cast<float>(row.depth*18+row.label.size()*10+72)));}
  auto graph=std::make_unique<Graph>();
  try {
    graph->horizontal=api_.createNode(ARKUI_NODE_SCROLL);graph->column=api_.createNode(ARKUI_NODE_COLUMN);if(!graph->horizontal||!graph->column)throw std::runtime_error("Native tree scroll nodes could not be created");
    Float(api_,graph->horizontal,NODE_WIDTH,width_);Float(api_,graph->horizontal,NODE_HEIGHT,std::max(1.0f,static_cast<float>(rows.size())*36));Integer(api_,graph->horizontal,NODE_SCROLL_SCROLL_DIRECTION,ARKUI_SCROLL_DIRECTION_HORIZONTAL);Float(api_,graph->column,NODE_WIDTH,content);NeedTree(api_.addChild(graph->horizontal,graph->column),"attach horizontal tree content");graph->rows.reserve(rows.size());
    for(size_t i=0;i<rows.size();++i){const auto& model=rows[i];auto item=std::make_unique<Item>();item->owner=this;item->index=static_cast<int>(i);item->editing=model.editing;item->draft=model.label;item->row=api_.createNode(ARKUI_NODE_ROW);if(!item->row)throw std::runtime_error("Native tree row creation failed");auto* stable=item.get();graph->rows.push_back(std::move(item));Float(api_,stable->row,NODE_WIDTH,content);Float(api_,stable->row,NODE_HEIGHT,36);
      stable->toggle=api_.createNode(ARKUI_NODE_BUTTON);stable->label=api_.createNode(model.editing?ARKUI_NODE_TEXT_INPUT:ARKUI_NODE_BUTTON);if(!stable->toggle||!stable->label)throw std::runtime_error("Native tree child creation failed");Float(api_,stable->toggle,NODE_WIDTH,28);Float(api_,stable->toggle,NODE_HEIGHT,32);ArkUI_NumberValue margins[4]{};margins[3].f32=static_cast<float>(model.depth)*18;ArkUI_AttributeItem margin{margins,4,nullptr,nullptr};NeedTree(api_.setAttribute(stable->toggle,NODE_MARGIN,&margin),"tree indentation");Text(api_,stable->toggle,NODE_BUTTON_LABEL,model.expandable?(model.expanded?"▾":"▸"):" ");Integer(api_,stable->toggle,NODE_ENABLED,model.expandable);Text(api_,stable->toggle,NODE_ACCESSIBILITY_TEXT,model.expandable?(model.expanded?"收起 ":"展开 ")+model.label:model.label);
      Float(api_,stable->label,NODE_WIDTH,std::max(40.0f,content-static_cast<float>(model.depth)*18-28));Float(api_,stable->label,NODE_HEIGHT,32);Text(api_,stable->label,model.editing?NODE_TEXT_INPUT_TEXT:NODE_BUTTON_LABEL,model.label);Text(api_,stable->label,NODE_ACCESSIBILITY_TEXT,model.label+(model.selected?"，已选中":""));Integer(api_,stable->label,NODE_BACKGROUND_COLOR,model.selected?0xffdcecdfu:0xfff8faf8u);Integer(api_,stable->label,NODE_FOCUSABLE,1);
      NeedTree(api_.addChild(stable->row,stable->toggle),"attach toggle");NeedTree(api_.addChild(stable->row,stable->label),"attach label");NeedTree(api_.addChild(graph->column,stable->row),"attach row");
      for(auto node:{stable->toggle,stable->label}){NeedTree(api_.addNodeEventReceiver(node,Receive),"row receiver");if(node==stable->toggle||!model.editing){NeedTree(api_.registerNodeEvent(node,NODE_ON_CLICK,0,stable),"row click");NeedTree(api_.registerNodeEvent(node,NODE_ON_MOUSE,0,stable),"row context");}}
      if(model.editing){NeedTree(api_.registerNodeEvent(stable->label,NODE_TEXT_INPUT_ON_CHANGE,0,stable),"editing change");NeedTree(api_.registerNodeEvent(stable->label,NODE_TEXT_INPUT_ON_SUBMIT,0,stable),"editing submit");NeedTree(api_.registerNodeEvent(stable->label,NODE_ON_BLUR,0,stable),"editing blur");}
      else stable->doubleClick=std::make_unique<ArkUiDoubleClick>(stable->label,[this,index=static_cast<int>(i)]{Emit(EventKind::TreeActivate,index);});
    }return graph;
  }catch(...){Clear(*graph,true);throw;}
}
void ArkUiTree::SetRows(const std::vector<TreeRow>& rows){
  if(updating_)throw std::logic_error("Native tree update is already in progress");struct Guard{bool& flag;explicit Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(updating_);
  const auto x=Scroll(true),y=Scroll(false);auto next=Build(rows);if(graph_&&graph_->attached){if(api_.removeChild(root_,graph_->horizontal)!=0){Clear(*next,true);throw std::runtime_error("Native tree old graph detach failed");}graph_->attached=false;}
  if(api_.addChild(root_,next->horizontal)!=0){if(graph_){if(api_.addChild(root_,graph_->horizontal)!=0){Clear(*next,true);throw std::runtime_error("Native tree replacement and restoration both failed");}graph_->attached=true;}Clear(*next,true);throw std::runtime_error("Native tree graph attach failed");}next->attached=true;
  auto previous=std::move(graph_);graph_=std::move(next);
  try{SetScroll(true,x);SetScroll(false,y);for(const auto& row:graph_->rows)if(row->editing)Integer(api_,row->label,NODE_FOCUS_STATUS,1);}catch(...){api_.removeChild(root_,graph_->horizontal);graph_->attached=false;Clear(*graph_,true);graph_=std::move(previous);if(graph_){NeedTree(api_.addChild(root_,graph_->horizontal),"restore previous tree");graph_->attached=true;}throw;}
  if(previous)Clear(*previous,true);
}
void ArkUiTree::SetViewportWidth(float width){if(!std::isfinite(width)||width<=0||width>32767)throw std::invalid_argument("Native tree viewport width");if(graph_)Float(api_,graph_->horizontal,NODE_WIDTH,width);width_=width;}
double ArkUiTree::Scroll(bool horizontal)const {auto node=horizontal?(graph_?graph_->horizontal:nullptr):root_;if(!node)return 0;const auto* a=api_.getAttribute(node,NODE_SCROLL_OFFSET);if(!a||!a->value||a->size<2)return 0;return a->value[horizontal?0:1].f32;}
void ArkUiTree::SetScroll(bool horizontal,double offset){if(!std::isfinite(offset)||offset<0||offset>std::numeric_limits<int>::max())throw std::invalid_argument("Native tree scroll offset");auto node=horizontal?(graph_?graph_->horizontal:nullptr):root_;if(!node){if(offset)throw std::logic_error("Empty tree has no horizontal content");return;}ArkUI_NumberValue values[3]{};values[horizontal?0:1].f32=static_cast<float>(offset);values[2].i32=0;ArkUI_AttributeItem a{values,3,nullptr,nullptr};NeedTree(api_.setAttribute(node,NODE_SCROLL_OFFSET,&a),"scroll offset");}
void ArkUiTree::Emit(EventKind kind,int index,const std::string& text){auto callback=receiver_;NodeEvent event{kind,static_cast<double>(index)};event.text=text;if(callback)callback(event);}
void ArkUiTree::Receive(ArkUI_NodeEvent* event)noexcept {
  std::function<void(const NodeEvent&)> callback;
  try{if(!event)return;auto* row=static_cast<Item*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!row||!row->owner)return;auto* owner=row->owner;if(owner->updating_)return;callback=owner->receiver_;const int index=row->index;const auto type=OH_ArkUI_NodeEvent_GetEventType(event);
    if(type==NODE_TEXT_INPUT_ON_CHANGE){auto* text=OH_ArkUI_NodeEvent_GetStringAsyncEvent(event);if(text&&text->pStr)row->draft=text->pStr;return;}
    if(type==NODE_TEXT_INPUT_ON_SUBMIT||type==NODE_ON_BLUR){const auto draft=row->draft;owner->Emit(EventKind::TreeEdit,index,draft);return;}
    if(type==NODE_ON_MOUSE){auto* input=OH_ArkUI_NodeEvent_GetInputEvent(event);if(input&&OH_ArkUI_MouseEvent_GetMouseButton(input)==UI_MOUSE_EVENT_BUTTON_RIGHT&&OH_ArkUI_MouseEvent_GetMouseAction(input)==UI_MOUSE_EVENT_ACTION_RELEASE)owner->Emit(EventKind::TreeContext,index);return;}
    if(type==NODE_ON_CLICK){const bool toggle=OH_ArkUI_NodeEvent_GetNodeHandle(event)==row->toggle;owner->Emit(toggle?EventKind::TreeToggle:EventKind::TreeSelect,index);}
  }catch(const std::exception& error){try{if(callback){NodeEvent failure{EventKind::BackendError};failure.text=error.what();callback(failure);}}catch(...){}}catch(...){/* native C callback boundary */}
}
}
