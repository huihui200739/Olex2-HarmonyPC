#include "native_option_widgets.h"
#include "native_widget.h"
#include "choiceext.h"
#include "checkboxext.h"
#include "eutf8.h"
#include <arkui/native_node.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {
void Error(char* out,std::size_t capacity,const char* text){if(out&&capacity){auto n=std::min(capacity-1,std::strlen(text));std::memcpy(out,text,n);out[n]=0;}}
struct OptionsView;
struct Forward:AActionHandler{OptionsView* owner;explicit Forward(OptionsView* p):owner(p){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;};
struct OptionsView {
  Forward forward;Widget row;ctrl_ext::TChoice choice;ctrl_ext::TCheckBox check;
  ArkUI_NodeContentHandle content=nullptr;Olex2NativeOptionsChange callback=nullptr;void* context=nullptr;
  bool attached=false;std::thread::id thread=std::this_thread::get_id();
  OptionsView(const char* const* items,std::size_t count,int selected,bool checked,bool enabled,Olex2NativeOptionsChange cb,void* data)
   :forward(this),row(ArkUiBackend(),NodeKind::Row),choice(ArkUiBackend(),&row,-1,{},Size{210,32}),check(ArkUiBackend(),&row,-1,L"选中",{},Size{100,30}),callback(cb),context(data){
    row.SetWidth(320);row.SetHeight(40);row.SetNumber(Property::RowSpacing,8);
    for(std::size_t i=0;i<count;++i){if(!items[i])throw std::invalid_argument("A native option label is absent");choice.AddObject(TUtf8::Decode(items[i],std::strlen(items[i])));}
    choice.SetSelection(selected);check.SetChecked(checked);row.Enable(enabled);choice.Enable(enabled);check.Enable(enabled);
    choice.Widget::SetText(TextProperty::Accessibility,"原生选项");choice.Widget::SetText(TextProperty::Id,"olex2-original-options-choice");check.SetText(TextProperty::Id,"olex2-original-options-check");
    choice.OnChange.Add(&forward);check.OnClick.Add(&forward);
  }
  void Pin(const std::shared_ptr<OptionsView>& self){row.SetEventLifetime(self);choice.SetEventLifetime(self);check.SetEventLifetime(self);}
  void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native option owner requires its creating UI thread");}
};
bool Forward::Execute(const IOlxObject*,const IOlxObject*,TActionQueue*){if(owner->attached&&owner->callback&&owner->callback(owner->context,owner->choice.GetSelection(),owner->check.IsChecked())!=0)throw std::runtime_error("Native option callback failed");return true;}
struct Registry{std::mutex mutex;uintptr_t next=0;std::map<void*,std::shared_ptr<OptionsView>> owners;};
Registry& Views(){static Registry value;return value;}
std::shared_ptr<OptionsView> Find(void* handle){auto& r=Views();std::lock_guard<std::mutex> lock(r.mutex);auto p=r.owners.find(handle);if(p==r.owners.end())throw std::invalid_argument("Native options owner is inactive");return p->second;}
void Selection(int value,std::size_t count){if(value < -1||(value>=0&&static_cast<std::size_t>(value)>=count))throw std::out_of_range("Native options selection is outside its items");}
}
extern "C" void* olex2_native_options_mount(void* content,const char* const* items,std::size_t count,int selected,bool checked,bool enabled,Olex2NativeOptionsChange callback,void* context,char* error,std::size_t capacity){
  try{Error(error,capacity,"");if(!content||!callback||(count&&!items))throw std::invalid_argument("Native options need content, item storage and callback");if(count>=static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Too many native options");Selection(selected,count);
    auto view=std::make_shared<OptionsView>(items,count,selected,checked,enabled,callback,context);view->Pin(view);view->content=static_cast<ArkUI_NodeContentHandle>(content);void* key=nullptr;auto& r=Views();{std::lock_guard<std::mutex> lock(r.mutex);if(r.next==std::numeric_limits<uintptr_t>::max())throw std::runtime_error("Native option owner sequence exhausted");key=reinterpret_cast<void*>(++r.next);r.owners.emplace(key,view);}
    if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->row.Handle()))!=0){std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(key);throw std::runtime_error("ArkUI could not attach original option controls");}view->attached=true;return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native options construction failed");}return nullptr;
}
extern "C" int olex2_native_options_update(void* handle,int selected,bool checked,bool enabled,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(handle);view->CheckThread();Selection(selected,view->choice.GetCount());if(!view->choice.LastEventError().empty()||!view->check.LastEventError().empty())throw std::runtime_error("Original option callback reported an error");view->choice.SetSelection(selected);view->check.SetChecked(checked);view->row.Enable(enabled);view->choice.Enable(enabled);view->check.Enable(enabled);return 0;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native options update failed");}return -1;
}
extern "C" int olex2_native_options_unmount(void* handle,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(handle);view->CheckThread();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->row.Handle()))!=0)throw std::runtime_error("Native options detach failed; owned nodes retained");view->attached=false;view->content=nullptr;auto& r=Views();{std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(handle);}return 0;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native options detach failed; owner retained");}return -1;
}
