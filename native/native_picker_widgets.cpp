#include "native_picker_widgets.h"
#include "datectrlext.h"
#include "colorext.h"
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
struct Parsed{CalendarDate date;Color color;};
Parsed Validate(const Olex2NativePickerValues* values){if(!values||!values->date||!values->color)throw std::invalid_argument("Native picker values are absent");Parsed parsed;if(!CalendarDate::TryParse(values->date,parsed.date)||(!parsed.date.IsValid()&&!(values->allowNone&&parsed.date.IsEmpty())))throw std::invalid_argument("Native calendar requires a valid civil date or explicit allowed none");if(!TryColorHex(values->color,parsed.color))throw std::invalid_argument("Native color requires RGB/RGBA hexadecimal text");return parsed;}
struct PickerView;
struct Forward final:AActionHandler{PickerView* owner;explicit Forward(PickerView* v):owner(v){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;};
struct PickerView{
  Forward forward;Widget column;ctrl_ext::TDateCtrl date;ctrl_ext::TColorCtrl color;
  ArkUI_NodeContentHandle content=nullptr;Olex2NativePickerChange callback=nullptr;void* context=nullptr;bool attached=false;std::thread::id thread=std::this_thread::get_id();
  PickerView(const Olex2NativePickerValues& values,const Parsed& parsed,Olex2NativePickerChange cb,void* data)
    :forward(this),column(ArkUiBackend(),NodeKind::Column),date(ArkUiBackend(),&column,-1,parsed.date,{},Size{320,36},values.allowNone?DateAllowNone:0),color(ArkUiBackend(),&column,-1,parsed.color,{},Size{320,36}),callback(cb),context(data){
    column.SetWidth(340);column.SetHeight(88);column.SetNumber(Property::RowSpacing,8);
    date.Widget::SetText(TextProperty::Id,"olex2-original-picker-date");color.Widget::SetText(TextProperty::Id,"olex2-original-picker-color");date.Widget::SetText(TextProperty::Accessibility,"日期选择");color.Widget::SetText(TextProperty::Accessibility,"颜色选择");Apply(values,parsed);date.OnChange.Add(&forward);color.OnChange.Add(&forward);
  }
  void Apply(const Olex2NativePickerValues& values,const Parsed& parsed){
    // A valid incoming date is set before removing an old allow-none flag.
    if(parsed.date.IsValid())date.SetValue(parsed.date);date.SetWindowStyle((values.allowNone?DateAllowNone:0)|(values.readOnly?PickerReadOnly:0));if(parsed.date.IsEmpty())date.SetValue(parsed.date);
    color.SetReadOnly(values.readOnly);color.SetValue(parsed.color);column.Enable(values.enabled);date.Enable(values.enabled);color.Enable(values.enabled);
  }
  void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native picker owner requires its creating UI thread");}
  void CheckErrors()const{if(!date.LastEventError().empty()||!color.LastEventError().empty())throw std::runtime_error("An original picker callback reported an error");}
  void Pin(const std::shared_ptr<PickerView>& self){column.SetEventLifetime(self);}
  void ClosePopups(){date.ClosePopup();color.ClosePopup();}
};
bool Forward::Execute(const IOlxObject* sender,const IOlxObject*,TActionQueue*){
  if(!owner->attached||!owner->callback)return true;int kind=0;std::string value;
  if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->date)){kind=1;value=owner->date.GetValue().Iso();}else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->color)){kind=2;value=ColorHex(owner->color.GetValue());}else throw std::runtime_error("Unknown original picker action sender");
  auto callback=owner->callback;auto context=owner->context;if(callback(context,kind,value.c_str())!=0)throw std::runtime_error("Native picker callback failed");return true;
}
struct Registry{std::mutex mutex;uintptr_t next=0;std::map<void*,std::shared_ptr<PickerView>> owners;};
Registry& Views(){static Registry value;return value;}
std::shared_ptr<PickerView> Find(void* key){auto& registry=Views();std::lock_guard<std::mutex> lock(registry.mutex);auto p=registry.owners.find(key);if(p==registry.owners.end())throw std::invalid_argument("Native picker owner is inactive");return p->second;}
}
extern "C" void* olex2_native_pickers_mount(void* content,const Olex2NativePickerValues* values,Olex2NativePickerChange callback,void* context,char* error,std::size_t capacity){
  try{Error(error,capacity,"");if(!content||!callback)throw std::invalid_argument("Native pickers need NodeContent and callback");auto parsed=Validate(values);auto view=std::make_shared<PickerView>(*values,parsed,callback,context);view->Pin(view);view->content=static_cast<ArkUI_NodeContentHandle>(content);auto& registry=Views();void* key=nullptr;
    {std::lock_guard<std::mutex> lock(registry.mutex);if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::runtime_error("Native picker owner sequence exhausted");key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,view);}
    if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0){std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);throw std::runtime_error("ArkUI could not attach original picker controls");}view->attached=true;return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native picker construction failed");}return nullptr;
}
extern "C" int olex2_native_pickers_update(void* key,const Olex2NativePickerValues* values,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();auto parsed=Validate(values);view->CheckErrors();view->Apply(*values,parsed);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native picker update failed");}return -1;
}
extern "C" int olex2_native_pickers_unmount(void* key,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();view->ClosePopups();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0)throw std::runtime_error("Native picker detach failed; owner retained");view->attached=false;view->content=nullptr;auto& registry=Views();{std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);}return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native picker detach failed; owner retained");}return -1;
}
