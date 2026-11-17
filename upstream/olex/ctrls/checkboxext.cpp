/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "checkboxext.h"
#include "olx_control_state.h"
#include "eutf8.h"
#include <cmath>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TCheckBox::TCheckBox(NodeBackend& backend,Widget* parent,int id,const olxstr& label,Point pos,Size size,long style)
 :Widget(backend,NodeKind::Row,parent,id,pos,size,style),AOlxCtrl(this),
  mark_(backend,NodeKind::CheckBox,this,{EventKind::CheckChange},[this](const NodeEvent& e){Changed(e);}),
  caption_(backend,NodeKind::Text,this,{EventKind::Click},[this](const NodeEvent&){if(IsEnabled()&&IsShown()){SetChecked(!checked_);Dispatch();}}),
  OnClick(AOlxCtrl::ActionQueue::New(Actions,evt_on_click_id)),OnCheck(AOlxCtrl::ActionQueue::New(Actions,evt_on_check_id)),OnUncheck(AOlxCtrl::ActionQueue::New(Actions,evt_on_uncheck_id)){
  SetToDelete(false);SetNumber(Property::RowSpacing,5);mark_.SetWidth(22);mark_.SetHeight(22);mark_.SetNumber(Property::Focusable,1);SetCaption(label);SetChecked(false);
}
TCheckBox::~TCheckBox(){if(ActionQueue)ActionQueue->Remove(this);}
void TCheckBox::SetCaption(const olxstr& value){auto utf8=TUtf8::Encode(value);std::string text(utf8.c_str(),utf8.Length());caption_.SetText(TextProperty::Content,text);mark_.SetText(TextProperty::Accessibility,text);}
olxstr TCheckBox::GetCaption()const{auto text=caption_.GetText(TextProperty::Content);return TUtf8::Decode(text.c_str(),text.size());}
void TCheckBox::SetChecked(bool value){const bool old=checked_;checked_=value;try{mark_.SetNumber(Property::Checked,value?1:0);}catch(...){checked_=old;throw;}}
void TCheckBox::Changed(const NodeEvent& event){
  if(event.kind!=EventKind::CheckChange||!std::isfinite(event.value)||(event.value!=0&&event.value!=1))return;
  if(!IsEnabled()||!IsShown()){mark_.SetNumber(Property::Checked,checked_?1:0);return;}
  const bool value=event.value!=0;if(value==checked_)return;SetChecked(value);Dispatch();
}
void TCheckBox::Dispatch(){OnClick.Execute(static_cast<AOlxCtrl*>(this));if(checked_)OnCheck.Execute(static_cast<AOlxCtrl*>(this));else OnUncheck.Execute(static_cast<AOlxCtrl*>(this));}
void TCheckBox::SetActionQueue(TActionQueue& q,const olxstr& mode){if(ActionQueue)ActionQueue->Remove(this);ActionQueue=&q;DependMode=mode;ActionQueue->Add(this);}
bool TCheckBox::Execute(const IOlxObject*,const IOlxObject* data,TActionQueue*){
  if(data&&data->Is<TModeChange>())SetChecked(static_cast<const TModeChange*>(data)->GetStatus());
  else if(data&&data->Is<TStateChange>())SetChecked(static_cast<const TStateChange*>(data)->GetStatus());
  else Dispatch();return true;
}
const std::string& TCheckBox::LastEventError()const{if(!Widget::LastEventError().empty())return Widget::LastEventError();if(!mark_.LastEventError().empty())return mark_.LastEventError();return caption_.LastEventError();}

#else


#include "checkboxext.h"
#include "frameext.h"
#include "olxstate.h"
#include "olxvar.h"

using namespace ctrl_ext;

//..............................................................................
TCheckBox::TCheckBox(wxWindow *Parent, wxWindowID id, const wxString& label,
  const wxPoint& pos, const wxSize& size, long style)
: AOlxCtrl(this),
  OnClick(AOlxCtrl::ActionQueue::New(Actions, evt_on_click_id)),
  OnCheck(AOlxCtrl::ActionQueue::New(Actions, evt_on_check_id)),
  OnUncheck(AOlxCtrl::ActionQueue::New(Actions, evt_on_uncheck_id)),
  ActionQueue(NULL)
{
  wxCheckBox::Create(Parent, id, label, pos, size, style);
  Bind(wxEVT_CHECKBOX, &TCheckBox::ClickEvent, this);
  Bind(wxEVT_ENTER_WINDOW, &TCheckBox::MouseEnterEvent, this);
  SetToDelete(false);
}
//..............................................................................
void TCheckBox::MouseEnterEvent(wxMouseEvent& event)  {
  event.Skip();
  SetCursor(wxCursor(wxCURSOR_HAND));
}
//..............................................................................
void TCheckBox::SetActionQueue(TActionQueue& q, const olxstr& dependMode)  {
  if (ActionQueue != 0) {
    ActionQueue->Remove(this);
  }
  ActionQueue = &q;
  DependMode = dependMode;
  ActionQueue->Add(this);
}
bool TCheckBox::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Data != 0 && Data->Is<TModeChange>()) {
    const TModeChange* mc = (const TModeChange*)Data;
    SetChecked(mc->GetStatus());
  }
  else if (Data != 0 && Data->Is<TStateChange>()) {
    const TStateChange* sc = (const TStateChange*)Data;
    SetChecked(sc->GetStatus());
  }
  else {
    OnClick.Execute((AOlxCtrl*)this);
    if (IsChecked()) {
      OnCheck.Execute((AOlxCtrl*)this);
    }
    else {
      OnUncheck.Execute((AOlxCtrl*)this);
    }
  }
  return true;
}
//..............................................................................
void TCheckBox::ClickEvent(wxCommandEvent &event) {
  event.Skip();
  OnClick.Execute((AOlxCtrl*)this);
  if (IsChecked()) {
    OnCheck.Execute((AOlxCtrl*)this);
  }
  else {
    OnUncheck.Execute((AOlxCtrl*)this);
  }
}

#endif // native option controls / original wx
