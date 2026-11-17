#include "native_command_line.h"
#include "cmdline.h"
#include "eutf8.h"
#include "native_text_positions.h"
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
void Error(char* out,std::size_t size,const char* text)noexcept{if(out&&size){auto n=std::min(size-1,std::strlen(text));std::memcpy(out,text,n);out[n]='\0';}}
olxstr Decode(const char* text){if(!text)throw std::invalid_argument("Command-line text is absent");auto result=TUtf8::Decode(text,std::strlen(text));TextUtf16Offset(result,result.Length());return result;}
void Validate(const Olex2NativeCommandLineValues* value){if(!value)throw std::invalid_argument("Command-line values are absent");if(value->width<80||value->width>16384)throw std::out_of_range("Command-line width is outside 80..16384 vp");Decode(value->prompt);Decode(value->command);}
struct View;
struct Forward final:AActionHandler {
  View* owner;explicit Forward(View* value):owner(value){SetToDelete(false);}
  bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;
};
struct View {
  Forward forward;
  TCmdLine line;
  ArkUI_NodeContentHandle content=nullptr;
  Olex2NativeCommandSubmit callback=nullptr;void* context=nullptr;
  bool attached=false;std::thread::id thread=std::this_thread::get_id();
  View(const Olex2NativeCommandLineValues& value,Olex2NativeCommandSubmit cb,void* data)
   :forward(this),line(ArkUiBackend(),nullptr),callback(cb),context(data){Apply(value);line.OnCommand.Add(&forward);}
  void Apply(const Olex2NativeCommandLineValues& value){line.SetPromptStr(Decode(value.prompt));line.SetCommand(Decode(value.command));line.SetWidth(value.width);line.SetReadOnly(value.readOnly);line.Enable(value.enabled);}
  void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Command-line owner requires its creating UI thread");}
  void Check()const{CheckThread();if(!line.LastEventError().empty())throw std::runtime_error(line.LastEventError());}
};
bool Forward::Execute(const IOlxObject*,const IOlxObject*,TActionQueue*){
  if(!owner->attached||!owner->callback)return true;
  auto bytes=TUtf8::Encode(owner->line.GetCommand());
  if(owner->callback(owner->context,bytes.c_str())!=0)throw std::runtime_error("Command-line submit receiver rejected the ordinary text");
  return true;
}
struct Registry {std::mutex mutex;std::map<void*,std::shared_ptr<View>> owners;uintptr_t next=0;};
Registry& Views(){static auto* value=new Registry;return *value;}
std::shared_ptr<View> Find(void* key){auto& r=Views();std::lock_guard<std::mutex> lock(r.mutex);auto i=r.owners.find(key);if(i==r.owners.end())throw std::invalid_argument("Unknown native command-line owner");return i->second;}
}
extern "C" void* olex2_native_commandline_mount(void* content,const Olex2NativeCommandLineValues* value,Olex2NativeCommandSubmit callback,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!content)throw std::invalid_argument("Command-line NodeContent is absent");Validate(value);
    auto view=std::make_shared<View>(*value,callback,context);view->content=static_cast<ArkUI_NodeContentHandle>(content);view->line.SetEventLifetime(view);
    auto& r=Views();void* key=nullptr;{std::lock_guard<std::mutex> lock(r.mutex);if(r.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Command-line handles exhausted");key=reinterpret_cast<void*>(++r.next);r.owners.emplace(key,view);}
    if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->line.Handle()))!=0){std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(key);throw std::runtime_error("ArkUI could not attach the original command line");}
    view->attached=true;return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native command-line construction failed");}return nullptr;
}
extern "C" int olex2_native_commandline_update(void* key,const Olex2NativeCommandLineValues* value,char* error,std::size_t capacity){
  try{Error(error,capacity,"");Validate(value);auto view=Find(key);view->Check();view->Apply(*value);return 0;}
  catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native command-line update failed");}return -1;
}
extern "C" int olex2_native_commandline_unmount(void* key,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->line.Handle()))!=0)throw std::runtime_error("Command-line detach failed; owner retained");view->attached=false;view->content=nullptr;auto& r=Views();std::lock_guard<std::mutex> lock(r.mutex);r.owners.erase(key);return 0;}
  catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Command-line detach failed; owner retained");}return -1;
}
