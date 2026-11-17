#include "native_viewport_zoom.h"
#include <arkui/native_node_napi.h>
#include <napi/native_api.h>
#include <hilog/log.h>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
namespace {
struct Owner {
  napi_env env=nullptr;
  napi_ref callback=nullptr,content=nullptr;
  void* native=nullptr;
  bool active=false;
  ~Owner(){if(callback)napi_delete_reference(env,callback);if(content)napi_delete_reference(env,content);}
};
std::map<uint32_t,std::shared_ptr<Owner>>& Owners(){static thread_local std::map<uint32_t,std::shared_ptr<Owner>> owners;return owners;}
uint32_t& Sequence(){static thread_local uint32_t value=0;return value;}
void Check(napi_status code,const char* message){if(code!=napi_ok)throw std::runtime_error(message);}
int Value(napi_env env,napi_value input){double value=0;Check(napi_get_value_double(env,input,&value),"Zoom needs a number");if(!std::isfinite(value)||value<22||value>100||std::floor(value)!=value)throw std::invalid_argument("Zoom needs integral 22..100 tenths");return static_cast<int>(value);}
bool Enabled(napi_env env,napi_value input){bool value=false;Check(napi_get_value_bool(env,input,&value),"Zoom enabled must be boolean");return value;}
uint32_t Token(napi_env env,napi_value input){double value=0;Check(napi_get_value_double(env,input,&value),"Zoom owner must be a number");if(value<1||value>UINT32_MAX||std::floor(value)!=value)throw std::invalid_argument("Zoom owner is invalid");return static_cast<uint32_t>(value);}
std::shared_ptr<Owner> Find(napi_env env,uint32_t token){auto p=Owners().find(token);if(p==Owners().end()||p->second->env!=env||!p->second->active)throw std::invalid_argument("Zoom owner is inactive");return p->second;}
napi_value Undefined(napi_env env){napi_value value=nullptr;Check(napi_get_undefined(env,&value),"Cannot create zoom result");return value;}
int Change(void* context,int value){
  const auto token=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context));auto p=Owners().find(token);if(p==Owners().end()||!p->second->active)return 0;
  auto owner=p->second; // Pin callback/content if a normal UI callback closes it.
  napi_handle_scope scope=nullptr;if(napi_open_handle_scope(owner->env,&scope)!=napi_ok)return -1;
  napi_value function=nullptr,receiver=nullptr,arg=nullptr,result=nullptr;int code=-1;
  if(napi_get_reference_value(owner->env,owner->callback,&function)==napi_ok&&napi_get_undefined(owner->env,&receiver)==napi_ok&&napi_create_double(owner->env,value/10.0,&arg)==napi_ok&&napi_call_function(owner->env,receiver,function,1,&arg,&result)==napi_ok)code=0;
  if(code){bool pending=false;napi_is_exception_pending(owner->env,&pending);if(pending){napi_value error;napi_get_and_clear_last_exception(owner->env,&error);}OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","Original zoom callback failed; recorded in control status");}
  napi_close_handle_scope(owner->env,scope);return code;
}
void Cleanup(void* data){
  auto env=static_cast<napi_env>(data);
  for(auto i=Owners().begin();i!=Owners().end();){if(i->second->env!=env){++i;continue;}auto owner=i->second;owner->active=false;char error[512]{};if(owner->native&&olex2_native_zoom_unmount(owner->native,error,sizeof(error))!=0)OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","Native zoom retained after cleanup detach failure");i=Owners().erase(i);}
}
napi_value Mount(napi_env env,napi_callback_info info){
  uint32_t token=0;std::shared_ptr<Owner> owner;
  try{
    size_t count=5;napi_value args[5]{};Check(napi_get_cb_info(env,info,&count,args,nullptr,nullptr),"Cannot read zoom arguments");if(count!=4)throw std::invalid_argument("Expected NodeContent, tenths, enabled and callback");
    int value=Value(env,args[1]);bool enabled=Enabled(env,args[2]);napi_valuetype type;Check(napi_typeof(env,args[3],&type),"Cannot inspect zoom callback");if(type!=napi_function)throw std::invalid_argument("Zoom callback is not a function");
    ArkUI_NodeContentHandle content=nullptr;if(OH_ArkUI_GetNodeContentFromNapiValue(env,args[0],&content)!=0||!content)throw std::invalid_argument("Expected a native NodeContent");
    if(Sequence()==UINT32_MAX)throw std::runtime_error("Native zoom owner sequence exhausted");token=++Sequence();owner=std::make_shared<Owner>();owner->env=env;
    Check(napi_create_reference(env,args[0],1,&owner->content),"Cannot retain zoom content");Check(napi_create_reference(env,args[3],1,&owner->callback),"Cannot retain zoom callback");Owners().emplace(token,owner);
    char error[512]{};owner->native=olex2_native_zoom_mount(content,value,enabled,Change,reinterpret_cast<void*>(static_cast<uintptr_t>(token)),error,sizeof(error));if(!owner->native)throw std::runtime_error(error);
    owner->active=true;napi_value result;Check(napi_create_uint32(env,token,&result),"Cannot create zoom owner token");return result;
  }catch(const std::exception& error){if(owner&&owner->native){char message[512]{};if(olex2_native_zoom_unmount(owner->native,message,sizeof(message))!=0){owner->active=false;OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","Native zoom retained after failed initialization cleanup");}}if(token)Owners().erase(token);napi_throw_error(env,nullptr,error.what());}
  return nullptr;
}
napi_value Update(napi_env env,napi_callback_info info){
  try{size_t count=4;napi_value args[4]{};Check(napi_get_cb_info(env,info,&count,args,nullptr,nullptr),"Cannot read zoom update");if(count!=3)throw std::invalid_argument("Expected zoom owner, tenths and enabled");auto owner=Find(env,Token(env,args[0]));int value=Value(env,args[1]);bool enabled=Enabled(env,args[2]);char error[512]{};if(olex2_native_zoom_update(owner->native,value,enabled,error,sizeof(error))!=0)throw std::runtime_error(error);return Undefined(env);}catch(const std::exception& error){napi_throw_error(env,nullptr,error.what());}return nullptr;
}
napi_value Unmount(napi_env env,napi_callback_info info){
  try{size_t count=2;napi_value args[2]{};Check(napi_get_cb_info(env,info,&count,args,nullptr,nullptr),"Cannot read zoom detach");if(count!=1)throw std::invalid_argument("Expected one zoom owner");auto token=Token(env,args[0]);auto owner=Find(env,token);char error[512]{};if(olex2_native_zoom_unmount(owner->native,error,sizeof(error))!=0)throw std::runtime_error(error);owner->active=false;owner->native=nullptr;Owners().erase(token);return Undefined(env);}catch(const std::exception& error){napi_throw_error(env,nullptr,error.what());}return nullptr;
}
}
__attribute__((visibility("hidden"))) napi_status RegisterNativeViewportZoom(napi_env env,napi_value exports){
  napi_property_descriptor methods[]={{"mountNativeViewportZoom",nullptr,Mount,nullptr,nullptr,nullptr,napi_default,nullptr},{"updateNativeViewportZoom",nullptr,Update,nullptr,nullptr,nullptr,napi_default,nullptr},{"unmountNativeViewportZoom",nullptr,Unmount,nullptr,nullptr,nullptr,napi_default,nullptr}};
  auto result=napi_define_properties(env,exports,3,methods);if(result==napi_ok)result=napi_add_env_cleanup_hook(env,Cleanup,env);return result;
}
