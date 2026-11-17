/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "spinctrlext.h"
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TSpinCtrl::TSpinCtrl(NodeBackend& backend,Widget* parent,int id,const olxstr& value,Point pos,Size size,long style)
 :NativeSpinControl(backend,parent,id,value,pos,size,style),AOlxCtrl(this),Value(GetValue()),OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)){}
void TSpinCtrl::NotifyChanged(){int next=GetValue();if(next==Value)return;Value=next;OnChange.Execute(this);}
void TSpinCtrl::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::TextChange){if(ApplyTextEvent(event))NotifyChanged();}
  else if(event.kind==EventKind::Submit||event.kind==EventKind::Blur){CommitText();NotifyChanged();}
  else if(event.kind==EventKind::SpinStep){Step(event.value>0?1:-1);NotifyChanged();}
  else if(event.kind==EventKind::Key&&event.key&&event.key->down&&(GetWindowStyle()&SpinArrowKeys)&&!event.key->control&&!event.key->alt&&!event.key->meta&&!event.key->shift){
    if(event.key->logical==LogicalKey::Up||event.key->logical==LogicalKey::Down){event.key->handled=true;Step(event.key->logical==LogicalKey::Up?1:-1);NotifyChanged();}
  }
}

#else
#include "spinctrlext.h"
#include "frameext.h"
#include "olxvar.h"
#include "wx/renderer.h"

using namespace ctrl_ext;
//..............................................................................
TSpinCtrl::TSpinCtrl(wxWindow *Parent, wxWindowID id, const wxString &value,
  const wxPoint& pos, const wxSize& size, long style)
: wxSpinCtrl(Parent, id, value, pos, size, style),
  AOlxCtrl(this),
  OnChange(AOlxCtrl::ActionQueue::New(Actions, evt_change_id))
{
  Bind(wxEVT_TEXT, &TSpinCtrl::TextChangeEvent, this);
  Bind(wxEVT_SPINCTRL, &TSpinCtrl::SpinChangeEvent, this);
  Bind(wxEVT_TEXT_ENTER, &TSpinCtrl::EnterPressedEvent, this);
  Bind(wxEVT_KILL_FOCUS, &TSpinCtrl::LeaveEvent, this);
  Bind(wxEVT_SET_FOCUS, &TSpinCtrl::EnterEvent, this);
  Bind(wxEVT_PAINT, &TSpinCtrl::PaintEvent, this);
}
//..............................................................................
void TSpinCtrl::SpinChangeEvent(wxSpinEvent& event) {
  event.Skip();
  int val = GetValue();
  if (val == Value) return;
  Value = val;
  OnChange.Execute(this);
}
//..............................................................................
void TSpinCtrl::TextChangeEvent(wxCommandEvent& event) {
  event.Skip();
  int val = GetValue();
  if (val == Value) return;
  Value = val;
  OnChange.Execute(this);
}
//..............................................................................
void TSpinCtrl::LeaveEvent(wxFocusEvent& event) {
  event.Skip();
  int val = GetValue();
  if (val == Value) return;
  Value = val;
  OnChange.Execute(this);
}
//..............................................................................
void TSpinCtrl::EnterEvent(wxFocusEvent& event) {
  event.Skip();
}
//..............................................................................
void TSpinCtrl::EnterPressedEvent(wxCommandEvent& event) {
  event.Skip();
  int val = GetValue();
  if (val == Value) return;
  Value = val;
  OnChange.Execute(this);
}
//..............................................................................
void TSpinCtrl::PaintEvent(wxPaintEvent& event) {
  event.Skip(true);
}
//..............................................................................

#endif // native inputs / original wx
