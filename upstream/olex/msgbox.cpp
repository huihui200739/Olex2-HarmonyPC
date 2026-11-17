/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "msgbox.h"
#include "eutf8.h"
#include <algorithm>
#include <limits>
using namespace olex2::harmony::ui;
namespace {
constexpr int ButtonEvent=1,TimerEvent=2,SizeEvent=3;
std::string MessageText(const olxstr& value){auto text=TUtf8::Encode(value);return {text.c_str(),text.Length()};}
MessageIcon IconFor(uint32_t flags){if(flags&MessageExclamation)return MessageIcon::Exclamation;if(flags&MessageHand)return MessageIcon::Hand;if(flags&MessageError)return MessageIcon::Error;if(flags&MessageInformation)return MessageIcon::Information;if(flags&MessageQuestion)return MessageIcon::Question;return MessageIcon::None;}
}
struct TdlgMsgBox::Alive{TdlgMsgBox* owner=nullptr;};
TdlgMsgBox::TdlgMsgBox(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,const olxstr& message,const olxstr& title,const olxstr& tickBoxMessage,uint32_t flags,bool showRemember,long selfTimeout,int selfTimeoutId,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),scheduler,parent,title,"TdlgMsgBox",{},std::move(positions)),alive_(std::make_shared<Alive>()),scheduler_(std::move(scheduler)),SelfTimeoutId(selfTimeoutId){
  if(flags&~uint32_t((1u<<9)-1))throw std::invalid_argument("Unknown native message-box flags");if(selfTimeout<0)throw std::invalid_argument("Message-box timeout cannot be negative");if(static_cast<uint64_t>(selfTimeout)>std::numeric_limits<uint64_t>::max()/1000)throw std::out_of_range("Message-box timeout is too large");SelfTimeout=static_cast<uint64_t>(selfTimeout);alive_->owner=this;std::weak_ptr<Alive> weak=alive_;
  auto retain=[&](const std::shared_ptr<Widget>& node){Retain(node);};
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());retain(scroll_);column_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());retain(column_);column_->SetNumber(Property::RowSpacing,10);
  messageRow_=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());retain(messageRow_);messageRow_->SetNumber(Property::RowSpacing,10);
  const auto icon=OriginalIcon(IconFor(flags));if(icon.png){auto decoded=nodes.DecodeImage(std::vector<uint8_t>(icon.png,icon.png+icon.bytes));if(!decoded)throw std::runtime_error("Original message icon could not be decoded");image_=std::make_shared<Widget>(nodes,NodeKind::Image,messageRow_.get());retain(image_);image_->SetWidth(32);image_->SetHeight(32);image_->SetImage(decoded);image_->SetText(TextProperty::Accessibility,"消息提示图标");}
  message_=std::make_shared<Widget>(nodes,NodeKind::Text,messageRow_.get());retain(message_);message_->SetText(TextProperty::Content,MessageText(message));message_->SetNumber(Property::FontSize,16);
  if(showRemember){cbRemember=std::make_shared<TCheckBox>(nodes,column_.get(),-1,tickBoxMessage);retain(cbRemember);cbRemember->SetHeight(28);}
  const struct {uint32_t flag;int result;const char* caption;} definitions[]={{MessageOk,MessageResultOk,"OK"},{MessageYes,MessageResultYes,"Yes"},{MessageNo,MessageResultNo,"No"},{MessageCancel,MessageResultCancel,"Cancel"}};
  for(const auto& definition:definitions){if(!(flags&definition.flag))continue;if(buttons.size()%2==0){auto row=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());row->SetNumber(Property::RowSpacing,8);row->SetHeight(40);retain(row);buttonRows_.push_back(row);}const int result=definition.result;
    auto node=std::make_shared<EventWidget>(nodes,NodeKind::Button,buttonRows_.back().get(),std::vector<EventKind>{EventKind::Click},[weak,result](const NodeEvent&){if(auto state=weak.lock())if(state->owner)state->owner->Dispatch(ButtonEvent,static_cast<short>(result),state->owner,nullptr,nullptr);});retain(node);node->SetText(TextProperty::Content,definition.caption);node->SetNumber(Property::Focusable,1);node->SetHeight(36);buttons.push_back({result,olxstr(definition.caption),node});
  }
  timer_=std::make_unique<TTimer>(scheduler_);timer_->OnTimer.Add(this,TimerEvent,msiExecute);OnResize.Add(this,SizeEvent,msiExecute);LayoutMessage();
}
TdlgMsgBox::~TdlgMsgBox(){alive_->owner=nullptr;OnResize.Remove(this);if(timer_){timer_->OnTimer.Remove(this);try{timer_->Stop();}catch(...){}}}
void TdlgMsgBox::LayoutMessage(){const int width=std::max(100,GetWidth()-24),height=std::max(40,GetHeight()-88);scroll_->SetWidth(width);scroll_->SetHeight(height);column_->SetWidth(width);messageRow_->SetWidth(width);message_->SetWidth(width-(image_?42:0));if(cbRemember)cbRemember->SetWidth(width);for(size_t i=0;i<buttonRows_.size();++i){buttonRows_[i]->SetWidth(width);const size_t count=std::min<size_t>(2,buttons.size()-i*2);const int buttonWidth=(width-(count==2?8:0))/static_cast<int>(count);for(size_t j=0;j<count;++j)buttons[i*2+j].node->SetWidth(buttonWidth);}}
void TdlgMsgBox::BeginTimeout(){ShownAt=scheduler_->NowMs();if(SelfTimeout&&IsShown()&&!IsClosing()){UpdateCountdown();timer_->Start(200,false);}}
void TdlgMsgBox::UpdateCountdown(){if(!SelfTimeout)return;const auto now=scheduler_->NowMs();const auto elapsed=now>=ShownAt?now-ShownAt:0;const auto total=SelfTimeout*1000;if(elapsed>total){EndModal(SelfTimeoutId);timer_->Stop();return;}const auto remaining=(total-elapsed)/1000;for(auto& button:buttons)if(button.result==SelfTimeoutId){olxstr label=button.caption;label<<" ("<<remaining<<')';button.node->SetText(TextProperty::Content,MessageText(label));break;}}
bool TdlgMsgBox::Dispatch(int message,short subId,const IOlxObject*,const IOlxObject*,TActionQueue*){if(message==ButtonEvent){if(IsClosing()||!IsShown())return true;EndModal(subId);timer_->Stop();}else if(message==TimerEvent){if(!IsShown()||IsClosing())timer_->Stop();else UpdateCountdown();}else if(message==SizeEvent)LayoutMessage();return true;}
void TdlgMsgBox::SetChecked(bool value){if(!cbRemember){if(value)throw std::logic_error("This message box has no remember checkbox");return;}cbRemember->SetChecked(value);}
bool TdlgMsgBox::Show(bool show){const bool changed=TDialog::Show(show);if(changed&&show)BeginTimeout();else if(changed&&!show)timer_->Stop();return changed;}
void TdlgMsgBox::ShowModalEx(bool manageParent,std::function<void(int)> completion){std::weak_ptr<Alive> weak=alive_;TDialog::ShowModalEx(manageParent,[weak,completion=std::move(completion)](int result){if(auto state=weak.lock())if(state->owner){state->owner->timer_->Stop();if(completion)completion(result);}});BeginTimeout();}
void TdlgMsgBox::SetSize(Size size){TDialog::SetSize(size);LayoutMessage();}
olxstr TdlgMsgBox::Result(int result)const{olxstr output;if(IsChecked())output='R';if(result==MessageResultOk)output<<'O';else if(result==MessageResultYes)output<<'Y';else if(result==MessageResultNo)output<<'N';else if(result==MessageResultCancel)output<<'C';return output;}
std::shared_ptr<TdlgMsgBox> TdlgMsgBox::Execute(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,const olxstr& message,const olxstr& title,std::function<void(const olxstr&)> completion,const olxstr& tickBoxMessage,uint32_t flags,bool showRemember,long selfTimeout,int selfTimeoutId,std::shared_ptr<DialogPositionStore> positions){
  auto dialog=std::make_shared<TdlgMsgBox>(nodes,std::move(dialogs),std::move(scheduler),parent,message,title,tickBoxMessage,flags,showRemember,selfTimeout,selfTimeoutId,std::move(positions));
  dialog->ShowModalEx(true,[dialog,completion=std::move(completion)](int result){if(completion)completion(dialog->Result(result));});return dialog;
}

#else
#include "msgbox.h"
#include "icons/exception.xpm"
#include "icons/exclamation.xpm"
#include "icons/information.xpm"
#include "icons/question.xpm"
#include "icons/stop.xpm"
#include "bapp.h"
#include "etime.h"

enum {
  MSGBOX_EVT_BUTTON,
  MSGBOX_EVT_TIMER
};

TdlgMsgBox::TdlgMsgBox(TMainFrame* Parent, const olxstr& msg, const olxstr& title,
  const olxstr& tickBoxMsg, long flags, bool ShowRememberCheckBox,
  long selfTimeout, wxWindowID selfTimeoutId)
  : TDialog(Parent, title.u_str(), EsdlClassName(TdlgMsgBox).u_str()),
  SelfTimeout(selfTimeout), SelfTimeoutId(selfTimeoutId)
{
  ShownAt = TETime::msNow();
  int Border = 5;
  wxStaticText* text = new wxStaticText(this, -1, msg.u_str());
  wxStaticBitmap* sbmp = 0;
  cbRemember = (ShowRememberCheckBox ? new wxCheckBox(this, -1, tickBoxMsg.u_str()) : 0);
  if ((flags & wxICON_EXCLAMATION) != 0) {
    sbmp = new wxStaticBitmap(this, -1, wxBitmap(exclamation_xpm));
  }
  else if ((flags & wxICON_HAND) != 0) {
    sbmp = new wxStaticBitmap(this, -1, wxBitmap(stop_xpm));
  }
  else if ((flags & wxICON_ERROR) != 0) {
    sbmp = new wxStaticBitmap(this, -1, wxBitmap(exception_xpm));
  }
  else if ((flags & wxICON_INFORMATION) != 0) {
    sbmp = new wxStaticBitmap(this, -1, wxBitmap(information_xpm));
  }
  else if ((flags & wxICON_QUESTION) != 0) {
    sbmp = new wxStaticBitmap(this, -1, wxBitmap(question_xpm));
  }

  wxBoxSizer* ASizer = new wxBoxSizer(wxHORIZONTAL);
  if (sbmp != 0) {
    ASizer->Add(sbmp, 0, wxALL, Border);
  }
  ASizer->Add(text, 0, wxALL, Border);
  wxBoxSizer* BSizer = 0;
  if (ShowRememberCheckBox) {
    BSizer = new wxBoxSizer(wxHORIZONTAL);
    BSizer->Add(cbRemember, 0, wxALL, Border);
  }

  wxBoxSizer* ButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  TButton* btn;
  if ((flags & wxOK) != 0) {
    btn = new TButton(this, wxID_OK, wxT("OK"));
    btn->OnClick.Add(this, MSGBOX_EVT_BUTTON);
    ButtonsSizer->Add(btn, 0, wxALL, Border);
    buttons.Add(btn);
  }
  if ((flags & wxYES) != 0) {
    btn = new TButton(this, wxID_YES, wxT("Yes"));
    btn->OnClick.Add(this, MSGBOX_EVT_BUTTON);
    ButtonsSizer->Add(btn, 0, wxALL, Border);
    buttons.Add(btn);
  }
  if ((flags & wxNO) != 0) {
    btn = new TButton(this, wxID_NO, wxT("No"));
    btn->OnClick.Add(this, MSGBOX_EVT_BUTTON);
    ButtonsSizer->Add(btn, 0, wxALL, Border);
    buttons.Add(btn);
  }
  if ((flags & wxCANCEL) != 0) {
    btn = new TButton(this, wxID_CANCEL, wxT("Cancel"));
    btn->OnClick.Add(this, MSGBOX_EVT_BUTTON);
    ButtonsSizer->Add(btn, 0, wxALL, Border);
    buttons.Add(btn);
  }

  wxBoxSizer* TopSiser = new wxBoxSizer(wxVERTICAL);
  TopSiser->Add(ASizer, 0, wxALL, 5);
  if (BSizer != 0) {
    TopSiser->Add(BSizer, 0, wxALL | wxALIGN_RIGHT, 5);
  }
  TopSiser->Add(ButtonsSizer, 0, wxALL | wxALIGN_RIGHT, 5);

  SetSizer(TopSiser);      // use the sizer for layout

  TopSiser->SetSizeHints(this);   // set size hints to honour minimum size

  Center();

  if (SelfTimeout != 0) {
    TBasicApp::GetInstance().OnTimer.Add(this, MSGBOX_EVT_TIMER);
  }
  
  //  TDialog::Init();
}
//..............................................................................
TdlgMsgBox::~TdlgMsgBox() {
  TBasicApp::GetInstance().OnTimer.Remove(this);
  for (size_t i = 0; i < buttons.Count(); i++) {
    buttons[i]->OnClick.Clear();
  }
}
//..............................................................................
bool TdlgMsgBox::Dispatch(int MsgId, short MsgSubId, const IOlxObject* Sender,
  const IOlxObject* Data, TActionQueue*)
{
  if (MsgId == MSGBOX_EVT_BUTTON) {
    TBasicApp::GetInstance().OnTimer.Remove(this);
    EndModal(dynamic_cast<const TButton*>(Sender)->GetId());
  }
  else if (MsgId == MSGBOX_EVT_TIMER) {
    if (TETime::msNow() - ShownAt > SelfTimeout*1000) {
      TBasicApp::GetInstance().OnTimer.Remove(this);
      EndModal(SelfTimeoutId);
    }
    else {
      for (size_t i = 0; i < buttons.Count(); i++) {
        if (buttons[i]->GetId() == SelfTimeoutId) {
          olxstr l = buttons[i]->GetLabel();
          size_t ti = l.LastIndexOf('(');
          if (ti != InvalidIndex) {
            l = l.SubStringTo(ti - 1).TrimWhiteChars();
          }
          l << " (" << (SelfTimeout*1000 + ShownAt - TETime::msNow())/1000 << ')';
          buttons[i]->SetLabel(l.u_str());
          buttons[i]->Update();
          break;
        }
      }
    }
  }
  return true;
}
//..............................................................................
olxstr TdlgMsgBox::Execute(TMainFrame* Parent, const olxstr& msg,
  const olxstr& title, const olxstr& tickBoxMsg, long flags,
  bool ShowRememberCheckBox,
  long selfTimeout, wxWindowID selfTimeoutId)
{
  TdlgMsgBox* dlg = new TdlgMsgBox(Parent, msg, title, tickBoxMsg, flags,
    ShowRememberCheckBox, selfTimeout, selfTimeoutId);
  int mv = dlg->ShowModal();
  olxstr rv;
  if (ShowRememberCheckBox && dlg->cbRemember->IsChecked()) {
    rv = 'R';
  }
  dlg->Destroy();
  if (mv == wxID_OK) {
    rv << 'O';
  }
  else if (mv == wxID_YES) {
    rv << 'Y';
  }
  else if (mv == wxID_NO) {
    rv << 'N';
  }
  else if (mv == wxID_CANCEL) {
    rv << 'C';
  }
  return rv;
}

#endif // native message box / original wx
