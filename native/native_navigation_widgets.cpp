#include "native_navigation_widgets.h"
#include "treeviewext.h"
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
void Error(char* out,size_t capacity,const char* text)noexcept{if(out&&capacity){const auto n=std::min(capacity-1,std::strlen(text));std::memcpy(out,text,n);out[n]=0;}}
olxstr Decode(const char* text){return text?TUtf8::Decode(text,std::strlen(text)):EmptyString();}
std::string Encode(const olxstr& text){auto value=TUtf8::Encode(text);return {value.c_str(),value.Length()};}
struct Navigation;
struct Forward:public AActionHandler {Navigation* owner;explicit Forward(Navigation* v):owner(v){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;};
struct Navigation {
 Forward forward;ctrl_ext::TTreeView tree;ArkUI_NodeContentHandle content=nullptr;Olex2NativeNavigationAction action;void* context;bool attached=false;std::thread::id thread=std::this_thread::get_id();
 Navigation(const Olex2NativeNavigationValues& values,Olex2NativeNavigationAction callback,void* data):forward(this),tree(ArkUiBackend(),nullptr,-1,{},Size{values.width,values.height},values.flags),action(callback),context(data){TStrList strings;for(size_t i=0;i<values.lineCount;++i){if(!values.lines[i])throw std::invalid_argument("Tree text line is null");strings.Add(Decode(values.lines[i]));}if(!tree.LoadFromStrings(strings))throw std::invalid_argument("Tree input must contain label/data pairs");tree.ExpandAllChildren(tree.GetRootItem());tree.SetContextMenu(values.contextDefinition&&*values.contextDefinition?ctrl_ext::TTreeView::CreateContextMenu(Decode(values.contextDefinition)):ctrl_ext::TTreeView::CreateDefaultContextMenu());tree.OnSelect.Add(&forward);tree.OnDblClick.Add(&forward);tree.OnEdit.Add(&forward);tree.Enable(values.enabled!=0);}
 void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native navigation requires its creating UI thread");}
 void Check()const{CheckThread();if(!tree.LastEventError().empty())throw std::runtime_error(tree.LastEventError());}
};
bool Forward::Execute(const IOlxObject*,const IOlxObject* data,TActionQueue* queue){if(!owner->attached||!owner->action)return true;const int kind=queue==&owner->tree.OnSelect?1:queue==&owner->tree.OnDblClick?2:queue==&owner->tree.OnEdit?3:0;if(!kind)throw std::runtime_error("Unknown tree action");auto selection=owner->tree.GetSelection();auto label=selection.IsOk()?Encode(owner->tree.GetItemText(selection)):std::string();std::string value;if(kind==3&&data)value=Encode(data->ToString());else if(selection.IsOk()){auto* node=owner->tree.GetItemData(selection);if(node&&node->GetData())value=Encode(node->GetData()->ToString());}auto callback=owner->action;auto context=owner->context;if(callback(context,kind,label.c_str(),value.c_str()))throw std::runtime_error("Native navigation callback failed");return true;}
struct Registry{std::mutex mutex;uintptr_t next=0;std::map<void*,std::shared_ptr<Navigation>> owners;};Registry& Views(){static Registry value;return value;}
std::shared_ptr<Navigation> Find(void* key){auto& r=Views();std::lock_guard<std::mutex> lock(r.mutex);auto it=r.owners.find(key);if(it==r.owners.end())throw std::invalid_argument("Inactive navigation owner");return it->second;}
}
extern "C" void* olex2_native_navigation_mount(void* content,const Olex2NativeNavigationValues* values,Olex2NativeNavigationAction callback,void* context,char* error,size_t capacity){
 try{Error(error,capacity,"");if(!content||!values||!callback||values->lineCount&&!values->lines||values->lineCount>100000||values->width<1||values->height<1||values->width>32767||values->height>32767)throw std::invalid_argument("Invalid native navigation inputs");auto view=std::make_shared<Navigation>(*values,callback,context);view->tree.SetEventLifetime(view);view->content=static_cast<ArkUI_NodeContentHandle>(content);auto& r=Views();void* key;{std::lock_guard<std::mutex> lock(r.mutex);if(r.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Navigation owner sequence");key=reinterpret_cast<void*>(++r.next);r.owners.emplace(key,view);}if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->tree.Handle()))){std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(key);throw std::runtime_error("Could not attach native navigation");}view->attached=true;return key;
 }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native navigation creation failed");}return nullptr;
}
extern "C" int olex2_native_navigation_set_enabled(void* key,int enabled,char* error,size_t capacity){try{Error(error,capacity,"");auto view=Find(key);view->Check();view->tree.Enable(enabled!=0);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native navigation update failed");}return -1;}
extern "C" int olex2_native_navigation_unmount(void* key,char* error,size_t capacity){try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->tree.Handle())))throw std::runtime_error("Navigation detach failed; owner retained");view->attached=false;view->content=nullptr;auto& r=Views();std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(key);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Navigation detach failed; owner retained");}return -1;}
