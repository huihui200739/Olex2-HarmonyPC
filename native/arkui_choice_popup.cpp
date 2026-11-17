#include "arkui_choice_popup.h"
#include <arkui/native_interface.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void Need(int32_t value,const char* text){if(value)throw std::runtime_error(std::string("ArkUI choice ")+text+" failed ("+std::to_string(value)+")");}
void Number(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,float value){ArkUI_NumberValue v{};v.f32=value;ArkUI_AttributeItem item{&v,1,nullptr,nullptr};Need(api.setAttribute(node,key,&item),"layout attribute");}
void Text(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,const std::string& value){ArkUI_AttributeItem item{};item.string=value.c_str();Need(api.setAttribute(node,key,&item),"text attribute");}
}
ArkUiChoicePopup::ArkUiChoicePopup(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle root,std::function<void(int)> changed):api_(api),root_(root),changed_(std::move(changed)){Label();}
ArkUiChoicePopup::~ArkUiChoicePopup(){
  try{Close();}catch(...){
    // A failed platform close must not leave callbacks into a destroyed owner.
    // Detach every callback but retain still-attached native nodes, not free them.
    for(auto& row:rows_){api_.unregisterNodeEvent(row->node,NODE_ON_CLICK);api_.removeNodeEventReceiver(row->node,Receive);}
    if(dialogs_&&dialog_)dialogs_->dispose(dialog_);
  }
}
void ArkUiChoicePopup::Label(){Text(api_,root_,NODE_BUTTON_LABEL,compact_?std::string("▾"):(selected_>=0?options_.at(static_cast<size_t>(selected_)):std::string("请选择"))+" ▾");}
void ArkUiChoicePopup::SetOptions(const std::vector<std::string>& options){
  if(options.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Native option list is too large");
  for(const auto& value:options)if(value.find('\0')!=std::string::npos)throw std::invalid_argument("Native option labels cannot contain an embedded NUL");
  auto next=options;Close();options_.swap(next);if(selected_>=0&&static_cast<size_t>(selected_)>=options_.size())selected_=-1;Label();
}
void ArkUiChoicePopup::SetSelected(int value){if(value < -1||(value>=0&&static_cast<size_t>(value)>=options_.size()))throw std::out_of_range("Native popup selection is outside its options");const int old=selected_;selected_=value;try{Label();}catch(...){selected_=old;throw;}}
void ArkUiChoicePopup::ClearNodes()noexcept{
  for(auto& row:rows_){api_.unregisterNodeEvent(row->node,NODE_ON_CLICK);api_.removeNodeEventReceiver(row->node,Receive);if(list_)api_.removeChild(list_,row->node);api_.disposeNode(row->node);}rows_.clear();
  if(scroll_&&list_)api_.removeChild(scroll_,list_);if(column_&&scroll_)api_.removeChild(column_,scroll_);
  if(list_)api_.disposeNode(list_);if(scroll_)api_.disposeNode(scroll_);if(column_)api_.disposeNode(column_);list_=scroll_=column_=nullptr;
}
void ArkUiChoicePopup::Close(){
  if(dialog_){Need(dialogs_->close(dialog_),"close");Need(dialogs_->removeContent(dialog_),"remove content");dialogs_->dispose(dialog_);dialog_=nullptr;}
  ClearNodes();
}
void ArkUiChoicePopup::Open(){
  Close();if(!dialogs_)OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_DIALOG,ArkUI_NativeDialogAPI_1,dialogs_);if(!dialogs_)throw std::runtime_error("ArkUI native choice dialog API12 is unavailable");
  try{
    column_=api_.createNode(ARKUI_NODE_COLUMN);scroll_=api_.createNode(ARKUI_NODE_SCROLL);list_=api_.createNode(ARKUI_NODE_COLUMN);if(!column_||!scroll_||!list_)throw std::runtime_error("Native choice list creation failed");
    Number(api_,column_,NODE_WIDTH,260);Number(api_,scroll_,NODE_WIDTH,260);Number(api_,list_,NODE_WIDTH,260);
    Number(api_,scroll_,NODE_HEIGHT,static_cast<float>(std::min<size_t>(10,options_.size()+1)*36));Need(api_.addChild(column_,scroll_),"attach scroll");Need(api_.addChild(scroll_,list_),"attach option list");
    rows_.reserve(options_.size()+1);
    for(size_t i=0;i<=options_.size();++i){auto row=std::make_unique<Item>();row->owner=this;row->index=static_cast<int>(i)-1;row->node=api_.createNode(ARKUI_NODE_BUTTON);if(!row->node)throw std::runtime_error("Native choice option creation failed");auto* stable=row.get();rows_.push_back(std::move(row));Number(api_,stable->node,NODE_WIDTH,260);Number(api_,stable->node,NODE_HEIGHT,36);Text(api_,stable->node,NODE_BUTTON_LABEL,i?options_[i-1]:std::string("（不选择）"));Need(api_.addChild(list_,stable->node),"attach choice row");Need(api_.addNodeEventReceiver(stable->node,Receive),"option receiver");Need(api_.registerNodeEvent(stable->node,NODE_ON_CLICK,0,stable),"option click");}
    dialog_=dialogs_->create();if(!dialog_)throw std::runtime_error("Native choice dialog creation failed");Need(dialogs_->setContent(dialog_,column_),"set content");Need(dialogs_->setAutoCancel(dialog_,true),"auto cancel");Need(dialogs_->show(dialog_,false),"show");
  }catch(...){try{Close();}catch(...){}throw;}
}
void ArkUiChoicePopup::Receive(ArkUI_NodeEvent* event)noexcept{
  try{if(!event||OH_ArkUI_NodeEvent_GetEventType(event)!=NODE_ON_CLICK)return;auto* row=static_cast<Item*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!row||!row->owner)return;auto callback=row->owner->changed_;int index=row->index;if(callback)callback(index);/* callback may close and destroy this popup; no later owner access */}catch(...){/* C callback boundary */}
}
}
