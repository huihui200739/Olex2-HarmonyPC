#include "native_dialog.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
void DialogPositionStore::Check()const{if(std::this_thread::get_id()!=thread_)throw std::runtime_error("Dialog positions require their UI thread");}
bool DialogPositionStore::Restore(const std::string& name,DialogGeometry& result)const{Check();auto i=positions_.find(name);if(i==positions_.end())return false;result=i->second;return true;}
void DialogPositionStore::Save(const std::string& name,const DialogGeometry& value){Check();if(!name.empty())positions_[name]=value;}
DialogView::~DialogView(){while(!descendants.empty())descendants.pop_back();root.reset();}
void DialogView::Retain(const std::shared_ptr<Widget>& widget){if(!widget||!root||&widget->Backend()!=&root->Backend())throw std::invalid_argument("Dialog content has a different backend");descendants.push_back(widget);}
namespace {
struct ParentLeaseState {
  Widget* parent=nullptr;std::weak_ptr<void> life;bool wasEnabled=false,held=false;
  void Release(){if(!held)return;if(!life.expired())parent->Enable(wasEnabled);held=false;}
};
// The UI-thread registry keys ownership, not a reusable Widget address. Leases
// share the original enabled state and restore it only after the last modal.
using ParentKey=std::weak_ptr<void>;
using ParentMap=std::map<ParentKey,std::weak_ptr<ParentLeaseState>,std::owner_less<ParentKey>>;
ParentMap& Leases(){static thread_local ParentMap leases;return leases;}
std::shared_ptr<ParentLeaseState> AcquireParent(Widget* parent,const std::weak_ptr<void>& life){
  if(!parent||life.expired())return {};
  auto& leases=Leases();for(auto i=leases.begin();i!=leases.end();)if(i->first.expired()||i->second.expired())i=leases.erase(i);else ++i;
  auto i=leases.find(life);if(i!=leases.end())if(auto existing=i->second.lock())return existing;
  auto lease=std::make_shared<ParentLeaseState>();lease->parent=parent;lease->life=life;lease->wasEnabled=parent->IsEnabled();parent->Enable(false);lease->held=true;leases[life]=lease;return lease;
}
void ReleaseParent(std::shared_ptr<ParentLeaseState>& lease){if(!lease)return;if(lease.use_count()==1)lease->Release();lease.reset();}
void Validate(const DialogGeometry& geometry){if(geometry.size.width<240||geometry.size.height<140||geometry.size.width>16384||geometry.size.height>16384)throw std::out_of_range("Dialog layout must be 240..16384 by 140..16384");}
int Coordinate(double value){if(!std::isfinite(value)||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max())throw std::out_of_range("Dialog coordinate is out of range");return static_cast<int>(std::lround(value));}
}
struct DialogWindow::State:std::enable_shared_from_this<DialogWindow::State> {
  std::thread::id thread=std::this_thread::get_id();NodeBackend* nodes=nullptr;
  std::shared_ptr<DialogBackend> backend;std::shared_ptr<UiScheduler> scheduler;
  std::shared_ptr<DialogPositionStore> positions;std::shared_ptr<DialogView> view;
  std::shared_ptr<DialogSession> session;std::shared_ptr<ParentLeaseState> lease;std::shared_ptr<State> finishHold;
  Widget* parent=nullptr;std::weak_ptr<void> parentLife;
  std::shared_ptr<void> eventLife=std::make_shared<int>(0);
  Widget* header=nullptr;Widget* titleNode=nullptr;Widget* closeButton=nullptr;
  Widget* body=nullptr;Widget* footer=nullptr;Widget* spacer=nullptr;Widget* resize=nullptr;
  DialogGeometry geometry,openedGeometry;Size measured{0,0};
  std::string title,name,error;bool resizable=true,shown=false,closing=false,modal=false,ownerAlive=true;
  int pendingResult=DialogCancelled;std::function<void(int)> completion;std::function<void()> resized;
  UiScheduler::Ticket observer=0;
  struct Drag {bool active=false,resize=false;int id=-1;double x=0,y=0,scaleX=1,scaleY=1;DialogGeometry before;} drag;
  void Check()const{if(std::this_thread::get_id()!=thread)throw std::runtime_error("Dialogs require their creating UI thread");}
  void Error(const std::string& value){error=value;}
  void Layout(){Validate(geometry);const int w=geometry.size.width,h=geometry.size.height;view->root->SetWidth(w);view->root->SetHeight(h);header->SetWidth(w);header->SetHeight(40);titleNode->SetWidth(w-48);titleNode->SetHeight(40);closeButton->SetWidth(40);closeButton->SetHeight(36);body->SetWidth(w);body->SetHeight(h-(resizable?64:40));footer->SetWidth(w);footer->SetHeight(resizable?24:0);resize->Show(resizable);footer->Show(resizable);if(resizable){resize->SetWidth(32);resize->SetHeight(24);spacer->SetNumber(Property::LayoutWeight,1);}}
  void Translate(){view->root->SetNumber(Property::TranslateX,shown?double(geometry.offset.x)-openedGeometry.offset.x:0);view->root->SetNumber(Property::TranslateY,shown?double(geometry.offset.y)-openedGeometry.offset.y:0);}
  void Save(){if(positions)positions->Save(name,geometry);}
  void Resize(Size size){Check();auto next=geometry;next.size=size;Validate(next);geometry=next;Layout();Save();}
  void Move(Point point,bool centered){Check();if(shown&&centered!=openedGeometry.centered)throw std::logic_error("Dialog alignment changes apply after closing");geometry.offset=point;geometry.centered=centered;Translate();Save();}
  void Pointer(const NodeEvent& event,bool resizeHandle){
    Check();if(!ownerAlive||closing||!event.pointer||(!resizable&&resizeHandle))return;const auto p=*event.pointer;
    if(p.phase==PointerPhase::Cancel){drag.active=false;return;}
    if(p.phase==PointerPhase::Down){if(!p.inside)return;auto actual=nodes->Measure(view->root->Handle());if(actual.width<=0||actual.height<=0)throw std::runtime_error("Dialog drag requires an actual measured layout");drag={true,resizeHandle,p.id,p.windowX,p.windowY,double(actual.width)/geometry.size.width,double(actual.height)/geometry.size.height,geometry};return;}
    if(!drag.active||drag.id!=p.id||drag.resize!=resizeHandle)return;
    if(p.phase==PointerPhase::Move||p.phase==PointerPhase::Up){const auto dx=(p.windowX-drag.x)/drag.scaleX,dy=(p.windowY-drag.y)/drag.scaleY;if(resizeHandle){Resize({std::clamp(Coordinate(drag.before.size.width+dx),240,16384),std::clamp(Coordinate(drag.before.size.height+dy),140,16384)});}else Move({Coordinate(drag.before.offset.x+dx),Coordinate(drag.before.offset.y+dy)},drag.before.centered);}
    if(p.phase==PointerPhase::Up)drag.active=false;
  }
  void Key(const NodeEvent& event,bool resizeHandle){if(!event.key||!event.key->down||closing||!ownerAlive)return;auto& key=*event.key;if(key.logical==LogicalKey::Escape){RequestClose(DialogCancelled);key.handled=true;return;}if(resizeHandle&&resizable){auto size=geometry.size;switch(key.logical){case LogicalKey::Left:size.width-=10;break;case LogicalKey::Right:size.width+=10;break;case LogicalKey::Up:size.height-=10;break;case LogicalKey::Down:size.height+=10;break;default:return;}size.width=std::clamp(size.width,240,16384);size.height=std::clamp(size.height,140,16384);Resize(size);key.handled=true;}}
  void StartObserver(){
    if(observer||!session)return;std::weak_ptr<State> weak=shared_from_this();
    observer=scheduler->Start(40,false,[weak]{if(auto state=weak.lock())state->Observe();});
  }
  void Observe(){
    Check();if(!session)return;if(session->IsClosed()){Finish();return;}if(closing||!ownerAlive){closing=true;try{session->Close();}catch(const std::exception& e){Error(e.what());}return;}
    auto actual=nodes->Measure(view->root->Handle());if(actual.width<=0||actual.height<=0)return;if(actual.width==measured.width&&actual.height==measured.height)return;measured=actual;auto callback=resized;if(ownerAlive&&callback)callback();
  }
  void Finish(){
    Check();auto self=shared_from_this();shown=false;closing=true;drag.active=false;
    try{
      ReleaseParent(lease);Save();
      if(observer){auto ticket=observer;scheduler->Stop(ticket);observer=0;}
    }catch(const std::exception& e){
      Error(e.what());finishHold=self;
      if(!observer){try{std::weak_ptr<State> weak=self;observer=scheduler->Start(40,false,[weak]{if(auto state=weak.lock())state->Finish();});}catch(const std::exception& retryError){Error(std::string(e.what())+"; completion retry: "+retryError.what());}}
      return;
    }
    closing=false;modal=false;session.reset();finishHold.reset();
    auto callback=std::move(completion);completion={};if(ownerAlive&&callback)callback(pendingResult);
  }
  void RequestClose(int result){
    Check();if(!session||closing)return;const int old=pendingResult;pendingResult=result;closing=true;drag.active=false;
    try{auto active=session;active->Close();}catch(const std::exception& e){if(session&&!session->IsClosePending()){closing=false;pendingResult=old;}Error(e.what());throw;}
  }
  void Open(bool asModal,bool manageParent,std::function<void(int)> done){
    Check();if(session||shown||closing)throw std::logic_error("Dialog is already visible or closing");if(!ownerAlive)throw std::logic_error("Dialog owner has ended");
    error.clear();modal=asModal;pendingResult=DialogCancelled;completion=std::move(done);openedGeometry=geometry;measured={0,0};Translate();auto self=shared_from_this();
    DialogCallbacks callbacks;callbacks.closed=[self]{self->Finish();};callbacks.dismiss=[weak=std::weak_ptr<State>(self)](int){if(auto state=weak.lock()){if(!state->closing)state->RequestClose(DialogCancelled);}};callbacks.error=[weak=std::weak_ptr<State>(self)](const std::string& text){if(auto state=weak.lock())state->Error(text);};
    try{
      session=backend->Create(view,{geometry,asModal},std::move(callbacks));if(!session)throw std::runtime_error("Native dialog backend returned no session");
      if(asModal&&manageParent)lease=AcquireParent(parent,parentLife);
      shown=true;auto active=session;active->Show();if(!session||session->IsClosed())return;StartObserver();
    }catch(const std::exception& e){Error(e.what());completion={};if(session){closing=true;try{session->Close();}catch(const std::exception& closeError){Error(std::string(e.what())+"; close: "+closeError.what());}}else{shown=false;modal=false;completion={};ReleaseParent(lease);}if(session&&!observer){try{StartObserver();}catch(const std::exception& retry){Error(std::string(e.what())+"; cleanup retry: "+retry.what());}}throw;}
  }
  void Dispose()noexcept{
    if(!ownerAlive)return;ownerAlive=false;eventLife.reset();resized={};completion={};drag.active=false;try{Save();if(session){if(!closing)RequestClose(DialogCancelled);}else ReleaseParent(lease);}catch(const std::exception& e){Error(e.what());}catch(...){Error("Dialog owner cleanup failed");}
    if(session&&!observer){try{StartObserver();}catch(const std::exception& e){Error(e.what());}catch(...){Error("Dialog owner retry could not be scheduled");}}
  }
};
DialogWindow::DialogWindow(NodeBackend& nodes,std::shared_ptr<DialogBackend> backend,std::shared_ptr<UiScheduler> scheduler,Widget* parent,const std::string& title,const std::string& name,DialogGeometry geometry,std::shared_ptr<DialogPositionStore> positions,bool resizable):state_(std::make_shared<State>()){
  if(!backend||!scheduler)throw std::invalid_argument("Dialog requires a backend and UI scheduler");if(parent && &parent->Backend()!=&nodes)throw std::invalid_argument("Dialog parent uses a different backend");
  auto& state=*state_;state.nodes=&nodes;state.backend=std::move(backend);state.scheduler=std::move(scheduler);state.parent=parent;if(parent)state.parentLife=parent->ObjectLifetime();state.positions=std::move(positions);state.name=name;state.title=title;state.resizable=resizable;state.geometry=geometry;if(state.positions)state.positions->Restore(name,state.geometry);Validate(state.geometry);
  state.view=std::make_shared<DialogView>();state.view->root=std::make_shared<Widget>(nodes,NodeKind::Column);state.view->root->SetEventLifetime(state.eventLife);state.view->root->SetBackgroundColor(0xfff5f5f5u);state.view->root->SetText(TextProperty::Accessibility,title);
  std::weak_ptr<State> weak=state_;
  auto retain=[&](const std::shared_ptr<Widget>& widget){state.view->Retain(widget);return widget.get();};
  state.header=retain(std::make_shared<EventWidget>(nodes,NodeKind::Row,state.view->root.get(),std::vector<EventKind>{EventKind::Pointer,EventKind::Key},[weak](const NodeEvent& e){if(auto s=weak.lock()){if(e.kind==EventKind::Pointer)s->Pointer(e,false);else s->Key(e,false);}}));
  state.titleNode=retain(std::make_shared<Widget>(nodes,NodeKind::Text,state.header));state.titleNode->SetText(TextProperty::Content,title);state.titleNode->SetNumber(Property::FontSize,16);
  state.closeButton=retain(std::make_shared<EventWidget>(nodes,NodeKind::Button,state.header,std::vector<EventKind>{EventKind::Click,EventKind::Key},[weak](const NodeEvent& e){if(auto s=weak.lock()){if(e.kind==EventKind::Click)s->RequestClose(DialogCancelled);else s->Key(e,false);}}));state.closeButton->SetText(TextProperty::Content,"×");state.closeButton->SetText(TextProperty::Accessibility,"关闭对话框");state.closeButton->SetNumber(Property::Focusable,1);
  state.body=retain(std::make_shared<Widget>(nodes,NodeKind::Column,state.view->root.get()));state.body->SetNumber(Property::Padding,12);
  state.footer=retain(std::make_shared<Widget>(nodes,NodeKind::Row,state.view->root.get()));state.spacer=retain(std::make_shared<Widget>(nodes,NodeKind::Text,state.footer));
  state.resize=retain(std::make_shared<EventWidget>(nodes,NodeKind::Button,state.footer,std::vector<EventKind>{EventKind::Pointer,EventKind::Key},[weak](const NodeEvent& e){if(auto s=weak.lock()){if(e.kind==EventKind::Pointer)s->Pointer(e,true);else s->Key(e,true);}}));state.resize->SetText(TextProperty::Content,"◢");state.resize->SetText(TextProperty::Accessibility,"调整对话框大小；方向键微调");state.resize->SetNumber(Property::Focusable,resizable?1:0);state.Layout();
}
DialogWindow::~DialogWindow(){state_->Dispose();}
void DialogWindow::SetResizeReceiver(std::function<void()> receiver){state_->Check();state_->resized=std::move(receiver);}
Widget& DialogWindow::Window()const{state_->Check();return *state_->view->root;}
Widget& DialogWindow::Body()const{state_->Check();return *state_->body;}
Widget* DialogWindow::GetParent()const{state_->Check();return state_->parentLife.expired()?nullptr:state_->parent;}
std::shared_ptr<DialogView> DialogWindow::SharedView()const{state_->Check();return state_->view;}
void DialogWindow::Retain(const std::shared_ptr<Widget>& widget){state_->Check();state_->view->Retain(widget);}
bool DialogWindow::Show(bool show){state_->Check();if(show){if(state_->shown||state_->closing)return false;state_->Open(false,false,{});return true;}if(!state_->session)return false;state_->RequestClose(DialogCancelled);return true;}
void DialogWindow::ShowModalEx(bool manageParent,std::function<void(int)> done){state_->Open(true,manageParent,std::move(done));}
void DialogWindow::EndModal(int result){state_->RequestClose(result);}
void DialogWindow::RetryClose(){state_->Check();if(state_->session){if(!state_->closing)state_->RequestClose(state_->pendingResult);else state_->session->Close();}else if(state_->lease||state_->completion)state_->Finish();}
bool DialogWindow::IsShown()const{state_->Check();return state_->shown;}
bool DialogWindow::IsClosing()const{state_->Check();return state_->closing;}
bool DialogWindow::IsModal()const{state_->Check();return state_->modal;}
DialogGeometry DialogWindow::Geometry()const{state_->Check();return state_->geometry;}
void DialogWindow::SetPosition(Point point,bool centered){state_->Move(point,centered);}
void DialogWindow::SetSize(Size size){state_->Resize(size);}
void DialogWindow::SetTitle(const std::string& title){state_->Check();state_->titleNode->SetText(TextProperty::Content,title);state_->view->root->SetText(TextProperty::Accessibility,title);state_->title=title;}
std::string DialogWindow::GetTitle()const{state_->Check();return state_->title;}
std::string DialogWindow::LastDialogError()const{state_->Check();if(!state_->error.empty())return state_->error;for(auto* widget:{state_->header,state_->closeButton,state_->resize})if(!widget->LastEventError().empty())return widget->LastEventError();return {};}
}
