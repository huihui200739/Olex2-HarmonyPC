// Actual ArkUI API12 custom dialog controller. OnDisappear, not close()'s
// return code or an elapsed animation delay, gates detachment and completion.
#include "native_dialog.h"
#include <arkui/native_interface.h>
#include <arkui/native_node.h>
#include <arkui/native_dialog.h>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void NeedDialog(int32_t code,const char* action){if(code)throw std::runtime_error(std::string("ArkUI dialog ")+action+" failed ("+std::to_string(code)+")");}
class ArkUiDialogSession final:public DialogSession,public std::enable_shared_from_this<ArkUiDialogSession> {
  std::thread::id thread_=std::this_thread::get_id();
  ArkUI_NativeNodeAPI_1* nodes_=nullptr;ArkUI_NativeDialogAPI_1* dialogs_=nullptr;
  ArkUI_NativeDialogHandle dialog_=nullptr;ArkUI_NodeHandle wrapper_=nullptr;
  std::shared_ptr<UiScheduler> scheduler_;std::shared_ptr<DialogView> view_;
  std::shared_ptr<ArkUiDialogSession> nativeHold_;DialogOptions options_;DialogCallbacks callbacks_;
  bool contentAttached_=false,childAttached_=false,receiver_=false,appearRegistered_=false,disappearRegistered_=false;
  bool shown_=false,appeared_=false,closePending_=false,disappeared_=false,closed_=false;
  UiScheduler::Ticket cleanupTicket_=0;std::string error_;unsigned lifecycleDepth_=0;
  struct LifecycleScope {ArkUiDialogSession& owner;explicit LifecycleScope(ArkUiDialogSession& value):owner(value){++owner.lifecycleDepth_;}~LifecycleScope(){--owner.lifecycleDepth_;}};
  void Check()const{if(std::this_thread::get_id()!=thread_)throw std::runtime_error("Native dialog controller requires its creating UI thread");}
  void Error(const std::string& message)noexcept{try{error_=message;auto callback=callbacks_.error;if(callback)callback(message);}catch(...) {}}
  void FinishDetach(){
    Check();if(closed_)return;auto self=shared_from_this();
    if(shown_&&!disappeared_)return;
    // Each completed operation is recorded independently. A failed detach keeps
    // the view owner, wrapper, receiver and controller available for retry.
    if(contentAttached_){NeedDialog(dialogs_->removeContent(dialog_),"detach actual content");contentAttached_=false;}
    if(childAttached_){NeedDialog(nodes_->removeChild(wrapper_,static_cast<ArkUI_NodeHandle>(view_->root->Handle())),"detach owned view");childAttached_=false;}
    if(appearRegistered_){nodes_->unregisterNodeEvent(wrapper_,NODE_EVENT_ON_APPEAR);appearRegistered_=false;}
    if(disappearRegistered_){nodes_->unregisterNodeEvent(wrapper_,NODE_EVENT_ON_DISAPPEAR);disappearRegistered_=false;}
    if(receiver_){NeedDialog(nodes_->removeNodeEventReceiver(wrapper_,Receive),"remove lifecycle receiver");receiver_=false;}
    if(dialog_){dialogs_->dispose(dialog_);dialog_=nullptr;}
    if(wrapper_){nodes_->disposeNode(wrapper_);wrapper_=nullptr;}
    closed_=true;closePending_=false;shown_=false;auto callback=std::move(callbacks_.closed);callbacks_={};
    auto heldView=std::move(view_);auto heldNative=std::move(nativeHold_);
    if(callback)callback();
  }
  void QueueDetach()noexcept{
    if(closed_||cleanupTicket_)return;
    try{std::weak_ptr<ArkUiDialogSession> weak=shared_from_this();cleanupTicket_=scheduler_->Start(1,true,[weak]{if(auto self=weak.lock()){self->cleanupTicket_=0;try{self->FinishDetach();}catch(const std::exception& e){self->Error(e.what());}catch(...){self->Error("Native dialog detach failed");}}});}
    catch(const std::exception& e){Error(e.what());}catch(...){Error("Native dialog completion could not be queued");}
  }
  static void Receive(ArkUI_NodeEvent* event)noexcept{
    try{
      if(!event)return;auto* raw=static_cast<ArkUiDialogSession*>(OH_ArkUI_NodeEvent_GetUserData(event));if(!raw)return;auto self=raw->shared_from_this();self->Check();LifecycleScope scope(*self);
      const auto type=OH_ArkUI_NodeEvent_GetEventType(event);
      if(type==NODE_EVENT_ON_APPEAR){self->appeared_=true;return;}
      if(type!=NODE_EVENT_ON_DISAPPEAR||self->closed_)return;
      self->disappeared_=true;self->QueueDetach();
      if(!self->closePending_){self->closePending_=true;auto dismiss=self->callbacks_.dismiss;if(dismiss)dismiss(-1);}
      self->QueueDetach();
    }catch(...){/* no exceptions cross the platform event ABI */}
  }
  static void Dismiss(ArkUI_DialogDismissEvent* event)noexcept{
    if(!event)return;
    // Block automatic dismissal. The C++ owner closes explicitly and keeps
    // content and modal leases until the real unmount event has completed.
    OH_ArkUI_DialogDismissEvent_SetShouldBlockDismiss(event,true);
    auto* raw=static_cast<ArkUiDialogSession*>(OH_ArkUI_DialogDismissEvent_GetUserData(event));if(!raw)return;
    try{auto self=raw->shared_from_this();self->Check();if(self->closed_||self->closePending_)return;auto callback=self->callbacks_.dismiss;if(callback)callback(OH_ArkUI_DialogDismissEvent_GetDismissReason(event));}
    catch(const std::exception& e){raw->Error(e.what());}catch(...){raw->Error("Native dialog dismissal failed");}
  }
public:
  ArkUiDialogSession(std::shared_ptr<UiScheduler> scheduler,std::shared_ptr<DialogView> view,const DialogOptions& options,DialogCallbacks callbacks):scheduler_(std::move(scheduler)),view_(std::move(view)),options_(options),callbacks_(std::move(callbacks)){}
  void Configure(){
    Check();nativeHold_=shared_from_this();
    OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE,ArkUI_NativeNodeAPI_1,nodes_);OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_DIALOG,ArkUI_NativeDialogAPI_1,dialogs_);
    if(!nodes_||!dialogs_)throw std::runtime_error("Native dialogs require the real ArkUI API12 node and dialog interfaces");
    wrapper_=nodes_->createNode(ARKUI_NODE_COLUMN);if(!wrapper_)throw std::runtime_error("Native dialog wrapper could not be created");
    NeedDialog(nodes_->addNodeEventReceiver(wrapper_,Receive),"bind lifecycle receiver");receiver_=true;
    NeedDialog(nodes_->registerNodeEvent(wrapper_,NODE_EVENT_ON_APPEAR,0,this),"register actual appearance");appearRegistered_=true;
    NeedDialog(nodes_->registerNodeEvent(wrapper_,NODE_EVENT_ON_DISAPPEAR,0,this),"register actual disappearance");disappearRegistered_=true;
    NeedDialog(nodes_->addChild(wrapper_,static_cast<ArkUI_NodeHandle>(view_->root->Handle())),"attach retained view");childAttached_=true;
    dialog_=dialogs_->create();if(!dialog_)throw std::runtime_error("Native dialog controller could not be created");
    NeedDialog(dialogs_->setContent(dialog_,wrapper_),"set retained content");contentAttached_=true;
    const auto& g=options_.geometry;NeedDialog(dialogs_->setContentAlignment(dialog_,g.centered?ARKUI_ALIGNMENT_CENTER:ARKUI_ALIGNMENT_TOP_START,static_cast<float>(g.offset.x),static_cast<float>(g.offset.y)),"set initial position");
    NeedDialog(dialogs_->setModalMode(dialog_,options_.modal),"set actual modal mode");NeedDialog(dialogs_->setAutoCancel(dialog_,true),"route dismiss requests");NeedDialog(dialogs_->enableCustomStyle(dialog_,true),"use owned dialog chrome");
    NeedDialog(dialogs_->registerOnWillDismissWithUserData(dialog_,this,Dismiss),"register owned dismissal");
  }
  void AbandonUnshown()noexcept{
    // A creation error has never shown a popup. Retain native ownership if even
    // cleanup fails, rather than destroying content still attached to ArkUI.
    callbacks_={};try{FinishDetach();}catch(const std::exception& e){Error(e.what());}catch(...){Error("Native dialog creation cleanup failed");}
  }
  void Show()override{
    Check();if(closed_||!dialog_)throw std::logic_error("Native dialog is not ready");if(shown_)throw std::logic_error("Native dialog is already shown");shown_=true;
    try{NeedDialog(dialogs_->show(dialog_,false),"show");}catch(...){if(!appeared_)shown_=false;throw;}
  }
  void Close()override{
    Check();if(closed_)return;auto self=shared_from_this();
    if(!shown_||disappeared_){closePending_=true;if(lifecycleDepth_){QueueDetach();return;}if(cleanupTicket_){scheduler_->Stop(cleanupTicket_);cleanupTicket_=0;}FinishDetach();return;}
    if(closePending_)return;closePending_=true;
    try{NeedDialog(dialogs_->close(dialog_),"request close");}catch(const std::exception& e){if(!disappeared_)closePending_=false;Error(e.what());throw;}
    // Closing is asynchronous. No detach, parent restoration or completion is
    // inferred here; NODE_EVENT_ON_DISAPPEAR queues the next event-turn cleanup.
  }
  bool IsClosed()const override{Check();return closed_;}
  bool IsClosePending()const override{Check();return closePending_;}
  std::string LastError()const override{Check();return error_;}
};
class ArkUiDialogs final:public DialogBackend {
  std::shared_ptr<UiScheduler> scheduler_;
public:
  explicit ArkUiDialogs(std::shared_ptr<UiScheduler> scheduler):scheduler_(std::move(scheduler)){if(!scheduler_)throw std::invalid_argument("Native dialog backend requires a UI scheduler");}
  std::shared_ptr<DialogSession> Create(std::shared_ptr<DialogView> view,const DialogOptions& options,DialogCallbacks callbacks)override{
    if(!view||!view->root)throw std::invalid_argument("Native dialog requires owned content");auto session=std::make_shared<ArkUiDialogSession>(scheduler_,std::move(view),options,std::move(callbacks));
    try{session->Configure();}catch(...){session->AbandonUnshown();throw;}return session;
  }
};
}
std::shared_ptr<DialogBackend> CreateArkUiDialogBackend(std::shared_ptr<UiScheduler> scheduler){return std::make_shared<ArkUiDialogs>(std::move(scheduler));}
}
