#include "native_ui_scheduler.h"
#include <napi/native_api.h>
#include <array>
#include <chrono>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
namespace olex2::harmony::ui {
namespace {
void Need(napi_env env,napi_status status,const char* action){
  if(status==napi_ok)return;
  if(status==napi_pending_exception){napi_value ignored=nullptr;napi_get_and_clear_last_exception(env,&ignored);}
  throw std::runtime_error(std::string("Native UI timer ")+action+" failed ("+std::to_string(status)+")");
}
struct SchedulerState;
struct CallbackData{std::weak_ptr<SchedulerState> state;UiScheduler::Ticket ticket;};
struct CleanupData{std::weak_ptr<SchedulerState> state;};
struct Timer {
  napi_ref callback=nullptr,handle=nullptr;
  bool oneShot=false;
  std::function<void()> receiver;
  Timer()=default;
  Timer(const Timer&)=delete;
  Timer& operator=(const Timer&)=delete;
  // References have exactly one owner even when map insertion or the
  // subsequent platform call throws. A moved-from local owns no handles.
  Timer(Timer&& other)noexcept
    :callback(std::exchange(other.callback,nullptr)),
     handle(std::exchange(other.handle,nullptr)),oneShot(other.oneShot),
     receiver(std::move(other.receiver)){}
  Timer& operator=(Timer&&)=delete;
};
struct SchedulerState {
  napi_env env=nullptr;
  std::thread::id thread=std::this_thread::get_id();
  bool alive=true;
  UiScheduler::Ticket next=0;
  std::array<napi_ref,4> functions{};
  std::map<UiScheduler::Ticket,Timer> timers;
  CleanupData* cleanup=nullptr;
  std::string lastError;
  void Check()const{if(std::this_thread::get_id()!=thread)throw std::runtime_error("Native UI timers require their creating environment thread");}
  void RequireAlive()const{Check();if(!alive)throw std::runtime_error("Native UI timer environment has ended");}
  napi_value Call(std::size_t function,std::size_t count,napi_value* args){
    RequireAlive();napi_value fn=nullptr,global=nullptr,result=nullptr;
    Need(env,napi_get_reference_value(env,functions.at(function),&fn),"read timer function");
    Need(env,napi_get_global(env,&global),"read timer global");
    Need(env,napi_call_function(env,global,fn,count,args,&result),"call timer function");return result;
  }
  void DeleteReferences(Timer& timer)noexcept{
    if(timer.handle){napi_delete_reference(env,timer.handle);timer.handle=nullptr;}
    if(timer.callback){napi_delete_reference(env,timer.callback);timer.callback=nullptr;}
  }
  void Stop(UiScheduler::Ticket ticket){
    RequireAlive();auto found=timers.find(ticket);if(found==timers.end())return;
    napi_value holder=nullptr,handle=nullptr;
    Need(env,napi_get_reference_value(env,found->second.handle,&holder),"read timer handle holder");
    Need(env,napi_get_named_property(env,holder,"value",&handle),"read opaque timer handle");
    // Keep the ticket and callback if the platform rejects cancellation. A
    // caller can report/retry it rather than silently claiming it was stopped.
    Call(found->second.oneShot?2:3,1,&handle);
    DeleteReferences(found->second);timers.erase(found);
  }
  void Shutdown()noexcept{
    if(!alive)return;
    // Context owners and their cleanup hooks run on this creating thread.
    if(std::this_thread::get_id()!=thread)return;
    while(!timers.empty()){
      auto found=timers.begin();try{Stop(found->first);}catch(const std::exception& e){lastError=e.what();DeleteReferences(found->second);timers.erase(found);}catch(...){DeleteReferences(found->second);timers.erase(found);}
    }
    for(auto& reference:functions){if(reference)napi_delete_reference(env,reference);reference=nullptr;}
    if(cleanup){auto* data=cleanup;cleanup=nullptr;if(napi_remove_env_cleanup_hook(env,EnvironmentCleanup,data)==napi_ok)delete data;}
    alive=false;
  }
  static void EnvironmentCleanup(void* raw)noexcept{
    auto* data=static_cast<CleanupData*>(raw);auto state=data->state.lock();
    if(state){state->cleanup=nullptr;state->alive=false;state->timers.clear();state->functions.fill(nullptr);}
    // The VM reclaims references and invokes each callback-data finalizer.
    delete data;
  }
  static void FinalizeCallback(napi_env,void* raw,void*)noexcept{delete static_cast<CallbackData*>(raw);}
  static napi_value Fire(napi_env env,napi_callback_info info)noexcept{
    std::shared_ptr<SchedulerState> state;
    UiScheduler::Ticket ticket=0;
    try{
      void* raw=nullptr;std::size_t argc=0;Need(env,napi_get_cb_info(env,info,&argc,nullptr,nullptr,&raw),"read timer callback");
      if(raw){auto* data=static_cast<CallbackData*>(raw);ticket=data->ticket;state=data->state.lock();}
      if(state&&state->alive){
        state->Check();auto found=state->timers.find(ticket);
        if(found!=state->timers.end()){
          auto receiver=found->second.receiver;
          // Complete single-shot state before user callbacks, including a
          // reentrant Stop(), a new timer or release of the scheduler owner.
          if(found->second.oneShot){state->DeleteReferences(found->second);state->timers.erase(found);}
          receiver();
        }
      }
    }catch(const std::exception& e){if(state){state->lastError=e.what();try{if(state->alive)state->Stop(ticket);}catch(...){}}}
    catch(...){if(state){state->lastError="Native UI timer receiver failed";try{if(state->alive)state->Stop(ticket);}catch(...){}}}
    napi_value result=nullptr;if(env)napi_get_undefined(env,&result);return result;
  }
};
class NapiUiScheduler final:public UiScheduler {
  std::shared_ptr<SchedulerState> state_;
public:
  explicit NapiUiScheduler(napi_env env):state_(std::make_shared<SchedulerState>()){
    if(!env)throw std::invalid_argument("Native UI timer environment is absent");state_->env=env;
    try{
      napi_value global=nullptr;Need(env,napi_get_global(env,&global),"read timer global");
      const char* names[]={"setTimeout","setInterval","clearTimeout","clearInterval"};
      for(std::size_t i=0;i<4;++i){napi_value fn=nullptr;napi_valuetype type;
        Need(env,napi_get_named_property(env,global,names[i],&fn),"find standard UI timer");
        Need(env,napi_typeof(env,fn,&type),"check standard UI timer");if(type!=napi_function)throw std::runtime_error(std::string("This UI environment has no standard ")+names[i]);
        Need(env,napi_create_reference(env,fn,1,&state_->functions[i]),"retain standard UI timer");
      }
      auto cleanup=std::make_unique<CleanupData>();cleanup->state=state_;
      Need(env,napi_add_env_cleanup_hook(env,SchedulerState::EnvironmentCleanup,cleanup.get()),"register environment cleanup");
      state_->cleanup=cleanup.release();
    }catch(...){state_->Shutdown();throw;}
  }
  ~NapiUiScheduler()override{state_->Shutdown();}
  Ticket Start(uint64_t milliseconds,bool oneShot,std::function<void()> receiver)override{
    auto state=state_;state->RequireAlive();
    if(!receiver)throw std::invalid_argument("Native UI timer receiver is absent");
    if(milliseconds==0||milliseconds>static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))throw std::out_of_range("Native UI timer interval must be 1..2147483647 ms");
    if(state->next==std::numeric_limits<Ticket>::max())throw std::overflow_error("Native UI timer tickets exhausted");
    const auto ticket=++state->next;
    auto data=std::make_unique<CallbackData>();data->state=state;data->ticket=ticket;
    napi_value callback=nullptr,delay=nullptr,handle=nullptr,holder=nullptr;Timer timer;timer.oneShot=oneShot;timer.receiver=std::move(receiver);
    auto env=state->env;
    Need(env,napi_create_function(env,"olex2NativeUiTimer",NAPI_AUTO_LENGTH,SchedulerState::Fire,data.get(),&callback),"create timer callback");
    Need(env,napi_add_finalizer(env,callback,data.get(),SchedulerState::FinalizeCallback,nullptr,nullptr),"register callback cleanup");data.release();
    try{
      Need(env,napi_create_reference(env,callback,1,&timer.callback),"retain timer callback");
      Need(env,napi_create_double(env,static_cast<double>(milliseconds),&delay),"create timer interval");
      // Keep the opaque returned value in a referenced holder. Harmony timer
      // numbers and other standard runtime handle objects remain lossless.
      Need(env,napi_create_object(env,&holder),"create timer handle holder");
      Need(env,napi_create_reference(env,holder,1,&timer.handle),"retain timer handle holder");
      auto inserted=state->timers.emplace(ticket,std::move(timer));
      if(!inserted.second)throw std::logic_error("Native UI timer ticket collision");
      napi_value args[]={callback,delay};
      try{
        handle=state->Call(oneShot?0:1,2,args);
        Need(env,napi_set_named_property(env,holder,"value",handle),"retain opaque timer handle");
      }catch(...){
        // If scheduling succeeded but recording its handle failed, cancel the
        // real timer while that handle is still valid in this callback scope.
        if(handle){try{state->Call(oneShot?2:3,1,&handle);}catch(...){}}
        auto found=state->timers.find(ticket);if(found!=state->timers.end()){state->DeleteReferences(found->second);state->timers.erase(found);}throw;
      }
      return ticket;
    }catch(...){state->DeleteReferences(timer);throw;}
  }
  void Stop(Ticket ticket)override{auto state=state_;state->Stop(ticket);}
  bool IsActive(Ticket ticket)const override{state_->Check();return state_->alive&&state_->timers.find(ticket)!=state_->timers.end();}
  uint64_t NowMs()const override{state_->Check();return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
  std::string LastError()const override{state_->Check();return state_->lastError;}
};
}
std::unique_ptr<UiScheduler> CreateNapiUiScheduler(void* environment){return std::make_unique<NapiUiScheduler>(static_cast<napi_env>(environment));}
}
