/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "choiceext.h"
#include "olxvar.h"
#include <algorithm>
#include <cmath>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TChoice::TChoice(NodeBackend& backend,Widget* parent,int id,Point pos,Size size,long style)
 :TItemList<NativeChoiceList>(backend,parent,id,pos,size,style),AOlxCtrl(this),
  OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)),OnLeave(AOlxCtrl::ActionQueue::New(Actions,evt_on_mouse_leave_id)),OnEnter(AOlxCtrl::ActionQueue::New(Actions,evt_on_mouse_enter_id)){
  OnLeave.SetEnabled(false);Listen({EventKind::Click,EventKind::ChoiceChange,EventKind::Focus,EventKind::Blur,EventKind::Hover});
}
TChoice::~TChoice(){_Clear();}
void TChoice::Clear(){_Clear();NativeChoiceList::Clear();StrValue.SetLength(0);}
void TChoice::SetText(const olxstr& text){auto found=_SetText(text);if(found.a!=InvalidIndex){SetSelection(static_cast<int>(found.a));StrValue=found.b;}else if(HasDefault()&&!IsEmpty()){SetSelection(0);StrValue=GetValue();}else{SetSelection(-1);StrValue=EmptyString();}}
olxstr TChoice::GetText()const{int selected=GetSelection();if(selected<0)return EmptyString();auto value=_GetText(static_cast<size_t>(selected));return value.a?value.b:GetValue();}
void TChoice::HandleOnEnter(){if(OnEnter.IsEnabled()){OnEnter.Execute(this);OnEnter.SetEnabled(false);OnLeave.SetEnabled(true);}}
void TChoice::HandleOnLeave(){if(OnLeave.IsEnabled()){auto value=GetValue();if(value!=StrValue){StrValue=value;OnChange.Execute(this);}OnLeave.Execute(this);OnLeave.SetEnabled(false);OnEnter.SetEnabled(true);}}
void TChoice::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::Click){Backend().OpenOptions(Handle());}
  else if(event.kind==EventKind::ChoiceChange){if(!ApplyUserSelection(event.value))return;auto value=GetValue();const bool changed=IsOnChangeAlways()||value!=StrValue;StrValue=value;SetNumber(Property::ChoiceSelection,GetSelection());Backend().CloseOptions(Handle());if(changed){if(!Data.IsEmpty())TOlxVars::SetVar(Data,GetText());OnChange.Execute(this);}}
  else if(event.kind==EventKind::Focus){if(!entered_){entered_=true;HandleOnEnter();}}
  else if(event.kind==EventKind::Blur){if(entered_){entered_=false;HandleOnLeave();}}
  else if(event.kind==EventKind::Hover){hovered_=event.value!=0;ApplyDrawParams();}
}
void TChoice::ApplyDrawParams(){
  if(drawParams.IsEmpty())return;
  auto width=drawParams.Find("border.width","1").ToDouble(),radius=drawParams.Find("border.radius","0").ToDouble();
  if(!std::isfinite(width)||!std::isfinite(radius)||width<0||radius<0)throw std::invalid_argument("Native choice border dimensions must be nonnegative");
  const uint32_t base=drawBase_;
  auto lighten=[base](double percent){auto channel=[percent](uint32_t v){return static_cast<uint32_t>(std::max(0.0,std::min(255.0,v*percent/100.0)));};return (base&0xff000000)|(channel((base>>16)&255)<<16)|(channel((base>>8)&255)<<8)|channel(base&255);};
  SetNumber(Property::BorderWidth,width);SetNumber(Property::BorderRadius,radius);
  SetNumber(Property::BorderColor,lighten(drawParams.Find("border.lightness",CustomDraw_Border_Lightness).ToDouble()));
  SetNumber(Property::Background,lighten(hovered_?drawParams.Find("highlight.lightness",CustomDraw_Highlight_Lightness).ToDouble():100));
}

#else


#include "choiceext.h"
#include "frameext.h"
#include "olxvar.h"

using namespace ctrl_ext;
//..............................................................................
TChoice::TChoice(wxWindow *Parent, wxWindowID id, const wxPoint& pos,
  const wxSize& size, long style)
: AOlxCtrl(this),
  OnChange(AOlxCtrl::ActionQueue::New(Actions, evt_change_id)),
  OnLeave(AOlxCtrl::ActionQueue::New(Actions, evt_on_mouse_leave_id)),
  OnEnter(AOlxCtrl::ActionQueue::New(Actions, evt_on_mouse_enter_id))
{
  wxChoice::Create(Parent, id, pos, size, 0, 0, style);
  Bind(wxEVT_CHOICE, &TChoice::ChangeEvent, this);
  Bind(wxEVT_KILL_FOCUS, &TChoice::LeaveEvent, this);
  Bind(wxEVT_SET_FOCUS, &TChoice::EnterEvent, this);
  Bind(wxEVT_ENTER_WINDOW, &TChoice::MouseEnterEvent, this);
  Bind(wxEVT_LEAVE_WINDOW, &TChoice::MouseLeaveEvent, this);
  Bind(wxEVT_PAINT, &TChoice::PaintEvent, this);
  OnLeave.SetEnabled(false);
  entered_counter = 0;
  OnChangeAlways = false;
}
//..............................................................................
TChoice::~TChoice() {
  _Clear();
}
//..............................................................................
void TChoice::SetText(const olxstr& T) {
  olx_pair_t<size_t, olxstr> found = _SetText(T);
  if (found.a != InvalidIndex) {
    StrValue = found.b;
    wxChoice::SetSelection(found.a);
  }
  else {
    if (HasDefault() && !IsEmpty()) {
      wxChoice::SetSelection(0);
      StrValue = GetText();
    }
    else {
      wxChoice::SetSelection(wxNOT_FOUND);
      StrValue = EmptyString();
    }
  }
}
//..............................................................................
void TChoice::Clear() {
  StrValue.SetLength(0);
  if (GetCount() == 0) {
    return;
  }
  _Clear();
  wxChoice::Clear();
}
//..............................................................................
void TChoice::ChangeEvent(wxCommandEvent& event) {
  olxstr v = GetValue();
  if (IsOnChangeAlways() || v != StrValue) {
    StrValue = v;
    if (!Data.IsEmpty()) {
      TOlxVars::SetVar(Data, GetText());
    }
    OnChange.Execute(this);
  }
  event.Skip();
}
//..............................................................................
void TChoice::LeaveEvent(wxFocusEvent& event)  {
  if (--entered_counter == 0) {
    HandleOnLeave();
  }
  event.Skip();
}
//..............................................................................
void TChoice::EnterEvent(wxFocusEvent& event)  {
  if (++entered_counter == 1) {
    HandleOnEnter();
  }
  event.Skip();
}
//..............................................................................
olxstr TChoice::GetText() const {
  int sel = wxChoice::GetSelection();
  if (sel == wxNOT_FOUND) {
    return EmptyString();
  }
  olx_pair_t<bool, olxstr> v = _GetText(sel);
  return (v.a ? v.b : olxstr(GetValue()));
}
//..............................................................................
void TChoice::HandleOnLeave() {
  if (OnLeave.IsEnabled()) {
    olxstr v = GetValue();
    bool changed = (v != StrValue);
    if (changed) {
      StrValue = v;
      OnChange.Execute(this);
    }
    OnLeave.Execute(this);
    OnLeave.SetEnabled(false);
    OnEnter.SetEnabled(true);
  }
}
//..............................................................................
void TChoice::HandleOnEnter()  {
  if (OnEnter.IsEnabled()) {
    OnEnter.Execute(this);
    OnEnter.SetEnabled(false);
    OnLeave.SetEnabled(true);
  }
}
//..............................................................................
void TChoice::MouseEnterEvent(wxMouseEvent& event) {
  SetCursor(wxCursor(wxCURSOR_HAND));
  event.Skip();
}
//..............................................................................
void TChoice::MouseLeaveEvent(wxMouseEvent& event) {
  event.Skip();
}
//..............................................................................
void TChoice::PaintEvent(wxPaintEvent& event) {
  if (IsBeingDeleted() || drawParams.IsEmpty()) {
    event.Skip();
    return;
  }
  int alpha = drawParams.Find("border.lightness",
    CustomDraw_Border_Lightness).ToInt();
  wxColor bg;
  bool in_wnd = GetScreenRect().Contains(wxGetMousePosition());
  if (in_wnd) {
    int alpha1 = drawParams.Find("highlight.lightness",
      CustomDraw_Highlight_Lightness).ToInt();
    bg = GetBackgroundColour().ChangeLightness(alpha1);
  }
  else {
    bg = GetBackgroundColour();
  }
  wxPaintDC dc(this);
  dc.SetBrush(wxBrush(bg, wxBRUSHSTYLE_SOLID));
  dc.SetPen(wxPen(GetBackgroundColour().ChangeLightness(alpha), 1, wxPENSTYLE_SOLID));

  int arrow_w = drawParams.Find("arrow_width", "20").ToInt(),
    text_offset = drawParams.Find("text_offset", "2").ToInt();
  wxSize sz = dc.GetTextExtent(GetValue());
  int w = WI.GetWidth(), h = WI.GetHeight();
  dc.DrawRectangle(0, 0, w, h);
  wxRendererNative::Get().DrawComboBoxDropButton(this, dc,
    wxRect(w - arrow_w, 0, arrow_w, h),
    in_wnd ? wxCONTROL_CURRENT : 0);
  dc.SetClippingRegion(wxRect(0, 0, w-arrow_w- text_offset, h));
  dc.DrawText(GetValue(), text_offset, (h - sz.GetHeight()) / 2);
  event.Skip(false);
}
//..............................................................................

#endif // native option controls / original wx
