#include "native_button_widgets.h"
#include "btnext.h"
#include "eutf8.h"
#include <arkui/native_node.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace olex2::harmony::ui;
namespace {
void Error(char* target,std::size_t capacity,const char* message)noexcept{if(target&&capacity){auto length=std::min(capacity-1,std::strlen(message));std::memcpy(target,message,length);target[length]='\0';}}
olxstr Text(const char* text){return text?TUtf8::Decode(text,std::strlen(text)):EmptyString();}
void Validate(const Olex2NativeButtonValues* values){if(!values||!values->caption||!values->bitmapSource||!*values->bitmapSource||!values->imageSources||!*values->imageSources)throw std::invalid_argument("Native buttons need caption and genuine bitmap/image sources");if(values->imageWidth==0||values->imageHeight==0||values->imageWidth< -1||values->imageHeight< -1||values->imageWidth>8192||values->imageHeight>8192)throw std::invalid_argument("Native image button dimensions are unsupported");}
struct ButtonView;
struct Forward final:AActionHandler {ButtonView* owner;explicit Forward(ButtonView* v):owner(v){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;};
struct ButtonView {
 Forward forward;Widget column;ctrl_ext::TButton text;ctrl_ext::TBmpButton bitmap;ctrl_ext::TImgButton image;
 ArkUI_NodeContentHandle content=nullptr;Olex2NativeButtonAction callback=nullptr;void* context=nullptr;bool attached=false;std::thread::id thread=std::this_thread::get_id();
 ButtonView(const Olex2NativeButtonValues& values,Olex2NativeButtonAction cb,void* data):forward(this),column(ArkUiBackend(),NodeKind::Column),text(ArkUiBackend(),&column,-1,Text(values.caption),{},Size{320,36}),bitmap(ArkUiBackend(),&column,-1,ctrl_ext::TBmpButton::LoadBitmap(ArkUiBackend(),Text(values.bitmapSource))),image(ArkUiBackend(),&column),callback(cb),context(data){
  image.SetImages(Text(values.imageSources),values.imageWidth,values.imageHeight);text.SetCustomDrawParams(Text(values.drawParameters));text.SetHint(Text(values.hint));bitmap.SetHint(Text(values.hint));image.SetHint(Text(values.hint));
  column.SetWidth(std::max({320,bitmap.GetWidth(),int(image.GetWidth())}));column.SetHeight(36+bitmap.GetHeight()+image.GetHeight()+16);column.SetNumber(Property::RowSpacing,8);
  for(ctrl_ext::AButtonBase* button:{static_cast<ctrl_ext::AButtonBase*>(&text),static_cast<ctrl_ext::AButtonBase*>(&bitmap),static_cast<ctrl_ext::AButtonBase*>(&image)}){button->OnClick.Add(&forward);button->OnDown.Add(&forward);button->OnUp.Add(&forward);}SetEnabled(values.enabled!=0);
 }
 void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native buttons require their creating UI thread");}
 void Pin(const std::shared_ptr<ButtonView>& owner){column.SetEventLifetime(owner);}
 void CheckErrors()const{for(auto* node:{static_cast<const Widget*>(&text),static_cast<const Widget*>(&bitmap),static_cast<const Widget*>(&image)})if(!node->LastEventError().empty())throw std::runtime_error(node->LastEventError());}
 void SetEnabled(bool enabled){const bool old=column.IsEnabled();try{image.SetEnabled(enabled);column.Enable(enabled);}catch(...){try{image.SetEnabled(old);column.Enable(old);}catch(...){}throw;}}
};
bool Forward::Execute(const IOlxObject* sender,const IOlxObject*,TActionQueue* queue){if(!owner->attached||!owner->callback)return true;int kind=0;ctrl_ext::AButtonBase* button=nullptr;
 if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->text)){kind=1;button=&owner->text;}else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->bitmap)){kind=2;button=&owner->bitmap;}else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->image)||sender==static_cast<IOlxObject*>(&owner->image)){kind=3;button=&owner->image;}else throw std::runtime_error("Unknown original button action sender");
 const int action=queue==&button->OnClick?1:queue==&button->OnDown?2:queue==&button->OnUp?3:0;if(!action)throw std::runtime_error("Unknown original button action queue");auto callback=owner->callback;auto context=owner->context;if(callback(context,kind,action,button->IsDown()?1:0)!=0)throw std::runtime_error("Native button callback failed");return true;
}
struct Registry{std::mutex mutex;uintptr_t next=0;std::map<void*,std::shared_ptr<ButtonView>> owners;};
Registry& Views(){static Registry value;return value;}
std::shared_ptr<ButtonView> Find(void* key){auto& registry=Views();std::lock_guard<std::mutex> lock(registry.mutex);auto p=registry.owners.find(key);if(p==registry.owners.end())throw std::invalid_argument("Native button owner is inactive");return p->second;}
}
extern "C" void* olex2_native_buttons_mount(void* content,const Olex2NativeButtonValues* values,Olex2NativeButtonAction callback,void* context,char* error,std::size_t capacity){
 try{Error(error,capacity,"");Validate(values);if(!content||!callback)throw std::invalid_argument("Native buttons need NodeContent and a callback");auto view=std::make_shared<ButtonView>(*values,callback,context);view->Pin(view);view->content=static_cast<ArkUI_NodeContentHandle>(content);auto& registry=Views();void* key=nullptr;
  {std::lock_guard<std::mutex> lock(registry.mutex);if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::runtime_error("Native button owner sequence exhausted");key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,view);}
  if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0){std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);throw std::runtime_error("ArkUI could not attach original button controls");}view->attached=true;return key;
 }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native button construction failed");}return nullptr;
}
extern "C" int olex2_native_buttons_set_enabled(void* key,int enabled,char* error,std::size_t capacity){try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();view->CheckErrors();view->SetEnabled(enabled!=0);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native button update failed");}return -1;}
extern "C" int olex2_native_buttons_unmount(void* key,char* error,std::size_t capacity){try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0)throw std::runtime_error("Native button detach failed; owner retained");view->attached=false;view->content=nullptr;auto& registry=Views();{std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);}return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native button detach failed; owner retained");}return -1;}
