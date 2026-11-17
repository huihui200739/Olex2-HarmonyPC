/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "ctrls.h"
#include "eutf8.h"
using namespace olex2::harmony::ui;
using namespace ctrl_ext;
namespace {
std::string DialogText(const olxstr& text){auto bytes=TUtf8::Encode(text);std::string result(bytes.c_str(),bytes.Length());if(result.find('\0')!=std::string::npos)throw std::invalid_argument("Dialog text contains an embedded NUL");return result;}
}
TDialog::TDialog(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,const olxstr& title,const olxstr& className,DialogGeometry geometry,std::shared_ptr<DialogPositionStore> positions,bool resizable)
 :DialogWindow(nodes,std::move(dialogs),std::move(scheduler),parent,DialogText(title),DialogText(className),geometry,std::move(positions),resizable),AOlxCtrl(&Window()),OnResize(DialogActions.New("OnResize")){
  SetResizeReceiver([this]{OnResize.Execute(this,nullptr);});
}
TDialog::~TDialog(){SetResizeReceiver({});}
struct TTimer::State {
  std::shared_ptr<UiScheduler> scheduler;UiScheduler::Ticket ticket=0;
  int interval=-1;bool oneShot=false;std::function<void()> notify;
};
TTimer::TTimer(std::shared_ptr<UiScheduler> scheduler):state_(std::make_shared<State>()),OnTimer(Actions.New("ONTIMER")){
  if(!scheduler)throw std::invalid_argument("Original timer requires its UI scheduler");state_->scheduler=std::move(scheduler);state_->notify=[this]{OnTimer.Execute(this,nullptr);};
}
TTimer::~TTimer(){state_->notify={};try{Stop();}catch(...){/* weak callback never dispatches to the ended original owner */}}
bool TTimer::Start(int milliseconds,bool oneShot){
  auto state=state_;if(milliseconds==-1)milliseconds=state->interval;if(milliseconds<=0)throw std::out_of_range("Original UI timer interval must be positive");Stop();std::weak_ptr<State> weak=state;
  const auto ticket=state->scheduler->Start(static_cast<uint64_t>(milliseconds),oneShot,[weak]{if(auto current=weak.lock()){if(current->oneShot)current->ticket=0;auto notify=current->notify;if(notify)notify();}});
  state->interval=milliseconds;state->oneShot=oneShot;state->ticket=ticket;return true;
}
void TTimer::Stop(){auto state=state_;if(state->ticket){state->scheduler->Stop(state->ticket);state->ticket=0;}}
bool TTimer::IsRunning()const{return state_->ticket&&state_->scheduler->IsActive(state_->ticket);}
bool TTimer::IsOneShot()const{return state_->oneShot;}
int TTimer::GetInterval()const{return state_->interval;}

#else
#include "ctrls.h"
#include "integration.h"

wxDEFINE_EVENT(OLX_COMMAND_EVT, olxCommandEvent);
//..............................................................................
//..............................................................................
IMPLEMENT_CLASS(TDialog, wxDialog)
IMPLEMENT_CLASS(TTimer, wxTimer)
//..............................................................................
TDialog::TDialog(TMainFrame *Parent, const wxString &Title,
  const wxString &ClassName, const wxPoint& position, const wxSize& size,
  int style)
  : wxDialog(Parent, -1,  Title, position, size, style, ClassName),
    AOlxCtrl(this),
    OnResize(Actions.New("OnResize")),
    Parent(Parent)
{
  manage_parent = false;
  if (Parent != NULL)
    Parent->RestorePosition(this);
  Bind(wxEVT_SIZE, &TDialog::OnSizeEvt, this);
}
//..............................................................................
TDialog::TDialog(wxWindow *Parent, const wxString &Title,
  const wxString &ClassName, const wxPoint& position, const wxSize& size,
  int style)
  : wxDialog(Parent, -1,  Title, position, size, style, ClassName),
    AOlxCtrl(this),
    OnResize(Actions.New("OnResize")),
    Parent(NULL)
{
  manage_parent = false;
}
//..............................................................................
TDialog::~TDialog()  {
  if( Parent != NULL )
    Parent->SavePosition(this);
}
//..............................................................................
void TDialog::OnSizeEvt(wxSizeEvent& event)  {
  event.Skip();
  OnResize.Execute(this, NULL);
}
//..............................................................................
bool TDialog::Show(bool show) {
  if (!show && manage_parent) {
    manage_parent = false;
    GetParent()->Enable(true);
  }
  return wxDialog::Show(show);
}
//..............................................................................
int TDialog::ShowModalEx(bool manage_parent) {
  if (manage_parent && GetParent() != NULL) {
    this->manage_parent = true;
    GetParent()->Enable(false);
  }
  return wxDialog::ShowModal();
}
//..............................................................................
//..............................................................................
//..............................................................................
bool olxCommandAction::Run() {
  TStrList toks = TParamList::StrtokLines(cmd, ">>");
  olex2::IOlex2Processor *ip = olex2::IOlex2Processor::GetInstance();
  for (size_t i = 0; i < toks.Count(); i++) {
    if (!ip->processMacro(toks[i])) {
      return false;
    }
  }
  return true;
}


#endif // native dialog/timer / original wx
