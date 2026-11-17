#include "native_menu.h"
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_dialog.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void NeedMenu(int32_t code,const char* action){if(code)throw std::runtime_error(std::string("ArkUI menu ")+action+" failed ("+std::to_string(code)+")");}
void Number(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,float value){ArkUI_NumberValue n{};n.f32=value;ArkUI_AttributeItem a{&n,1,nullptr,nullptr};NeedMenu(api.setAttribute(node,key,&a),"number");}
void Integer(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,uint32_t value){ArkUI_NumberValue n{};n.u32=value;ArkUI_AttributeItem a{&n,1,nullptr,nullptr};NeedMenu(api.setAttribute(node,key,&a),"integer");}
void Text(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,ArkUI_NodeAttributeType key,const std::string& value){if(value.find('\0')!=std::string::npos)throw std::invalid_argument("Menu text contains NUL");ArkUI_AttributeItem a{};a.string=value.c_str();NeedMenu(api.setAttribute(node,key,&a),"text");}
class ArkUiMenuPopup final:public MenuPopup {
  struct Item {ArkUiMenuPopup* owner;ArkUI_NodeHandle node=nullptr;int id;bool enabled;};
  ArkUI_NativeNodeAPI_1* api_=nullptr;
  ArkUI_NativeDialogAPI_1* dialogs_=nullptr;
  ArkUI_NativeDialogHandle dialog_=nullptr;
  ArkUI_NodeHandle root_=nullptr,scroll_=nullptr,column_=nullptr;
  std::vector<std::unique_ptr<Item>> rows_;
  std::function<void(int)> chosen_;
  void Disconnect()noexcept {if(!api_)return;for(auto& row:rows_){api_->unregisterNodeEvent(row->node,NODE_ON_CLICK);api_->removeNodeEventReceiver(row->node,Receive);}}
  void Clear()noexcept {
    Disconnect();for(auto& row:rows_){if(column_)api_->removeChild(column_,row->node);api_->disposeNode(row->node);}rows_.clear();
    if(scroll_&&column_)api_->removeChild(scroll_,column_);if(root_&&scroll_)api_->removeChild(root_,scroll_);
    if(column_)api_->disposeNode(column_);if(scroll_)api_->disposeNode(scroll_);if(root_)api_->disposeNode(root_);root_=scroll_=column_=nullptr;
  }
  static void Receive(ArkUI_NodeEvent* event)noexcept {
    try{if(!event||OH_ArkUI_NodeEvent_GetEventType(event)!=NODE_ON_CLICK)return;auto* row=static_cast<Item*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!row||!row->owner||!row->enabled)return;auto* owner=row->owner;const int id=row->id;auto callback=owner->chosen_;if(callback)callback(id);/* callback may destroy every row and its owner */}catch(...){/* never cross the C ABI */}
  }
public:
  ~ArkUiMenuPopup()override {try{Close();}catch(...){Disconnect();if(dialogs_&&dialog_)dialogs_->dispose(dialog_);/* retain attached nodes after a platform detach failure */}}
  void Close()override {
    if(dialog_){NeedMenu(dialogs_->close(dialog_),"close");NeedMenu(dialogs_->removeContent(dialog_),"detach");dialogs_->dispose(dialog_);dialog_=nullptr;}Clear();chosen_={};
  }
  void Open(const std::string& title,const std::vector<MenuRow>& rows,std::function<void(int)> chosen)override {
    if(rows.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Menu too large");
    if(title.find('\0')!=std::string::npos)throw std::invalid_argument("Menu title contains NUL");for(const auto& row:rows)if(row.label.find('\0')!=std::string::npos||row.help.find('\0')!=std::string::npos)throw std::invalid_argument("Menu text contains NUL");
    Close();if(!api_)OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE,ArkUI_NativeNodeAPI_1,api_);if(!dialogs_)OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_DIALOG,ArkUI_NativeDialogAPI_1,dialogs_);if(!api_||!dialogs_)throw std::runtime_error("Native menu requires the real ArkUI API12 interfaces");
    try {
      root_=api_->createNode(ARKUI_NODE_COLUMN);scroll_=api_->createNode(ARKUI_NODE_SCROLL);column_=api_->createNode(ARKUI_NODE_COLUMN);if(!root_||!scroll_||!column_)throw std::runtime_error("Menu nodes could not be created");
      Number(*api_,root_,NODE_WIDTH,360);Number(*api_,scroll_,NODE_WIDTH,360);Number(*api_,column_,NODE_WIDTH,360);Number(*api_,scroll_,NODE_HEIGHT,static_cast<float>(std::min<size_t>(12,rows.size()+1)*36));Text(*api_,root_,NODE_ACCESSIBILITY_TEXT,title.empty()?"菜单":title);
      NeedMenu(api_->addChild(root_,scroll_),"attach scroll");NeedMenu(api_->addChild(scroll_,column_),"attach list");rows_.reserve(rows.size()+1);
      for(size_t i=0;i<=rows.size();++i) {
        const bool dismiss=i==rows.size();const bool separator=!dismiss&&rows[i].separator;
        auto row=std::make_unique<Item>();row->owner=this;row->id=dismiss?std::numeric_limits<int>::min():rows[i].id;row->enabled=dismiss||rows[i].enabled;
        row->node=api_->createNode(separator?ARKUI_NODE_ROW:ARKUI_NODE_BUTTON);if(!row->node)throw std::runtime_error("Menu row creation failed");auto* stable=row.get();rows_.push_back(std::move(row));Number(*api_,stable->node,NODE_WIDTH,360);Number(*api_,stable->node,NODE_HEIGHT,separator?2:36);
        if(separator)Integer(*api_,stable->node,NODE_BACKGROUND_COLOR,0xffa0a0a0u);
        else {
          const auto label=dismiss?std::string("关闭菜单"):(rows[i].checkable?(rows[i].checked?"✓ ":"  "):"")+rows[i].label+(rows[i].submenu?"  ›":"");
          Text(*api_,stable->node,NODE_BUTTON_LABEL,label);Text(*api_,stable->node,NODE_ACCESSIBILITY_TEXT,dismiss?label:rows[i].label+(rows[i].checked?"，已选中":"")+(rows[i].submenu?"，子菜单":"")+(rows[i].help.empty()?"":"，"+rows[i].help));Integer(*api_,stable->node,NODE_ENABLED,stable->enabled);Integer(*api_,stable->node,NODE_FOCUSABLE,stable->enabled);
          NeedMenu(api_->addNodeEventReceiver(stable->node,Receive),"receiver");NeedMenu(api_->registerNodeEvent(stable->node,NODE_ON_CLICK,0,stable),"click");
        }
        NeedMenu(api_->addChild(column_,stable->node),"attach row");
      }
      chosen_=std::move(chosen);dialog_=dialogs_->create();if(!dialog_)throw std::runtime_error("Native menu dialog creation failed");NeedMenu(dialogs_->setContent(dialog_,root_),"content");NeedMenu(dialogs_->setAutoCancel(dialog_,true),"auto cancel");NeedMenu(dialogs_->show(dialog_,false),"show");
    }catch(...){try{Close();}catch(...){}throw;}
  }
};
}
std::shared_ptr<MenuPopup> CreateArkUiMenuPopup(){return std::make_shared<ArkUiMenuPopup>();}
}
