/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "trackbarext.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TTrackBar::TTrackBar(NodeBackend& backend,Widget* parent,int id,int value,int minimum,int maximum,Point position,Size size,long style)
  :Widget(backend,NodeKind::Slider,parent,id,position,size,style),AOlxCtrl(this),this_Val(value),
   OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)),
   OnMouseUp(AOlxCtrl::ActionQueue::New(Actions,evt_on_mouse_up_id)) {
  SetRange(minimum,maximum);SetNumber(Property::SliderStep,1);SetValue(value);SetNumber(Property::Focusable,1);Listen({EventKind::SliderChange});
}
int TTrackBar::GetValue()const{return static_cast<int>(std::lround(GetNumber(Property::SliderValue)));}
int TTrackBar::GetMin()const{return static_cast<int>(std::lround(GetNumber(Property::SliderMinimum)));}
int TTrackBar::GetMax()const{return static_cast<int>(std::lround(GetNumber(Property::SliderMaximum)));}
void TTrackBar::SetValue(int value){int clamped=std::clamp(value,GetMin(),GetMax());SetNumber(Property::SliderValue,clamped);this_Val=clamped;}
void TTrackBar::SetRange(int minimum,int maximum){
  if(minimum>maximum)throw std::invalid_argument("Slider minimum exceeds maximum");
  int value=GetValue();
  if(minimum>GetMax()){SetNumber(Property::SliderMaximum,maximum);SetNumber(Property::SliderMinimum,minimum);}
  else {SetNumber(Property::SliderMinimum,minimum);SetNumber(Property::SliderMaximum,maximum);}
  SetValue(value);
}
void TTrackBar::SetMin(int value){SetRange(value,GetMax());}
void TTrackBar::SetMax(int value){SetRange(GetMin(),value);}
void TTrackBar::ScrollEvent(double value){
  if(!std::isfinite(value))return;
  int next=static_cast<int>(std::lround(std::clamp(value,double(GetMin()),double(GetMax()))));
  if(this_Val==next)return;
  SetNumber(Property::SliderValue,next);this_Val=next;OnChange.Execute(static_cast<AOlxCtrl*>(this));
}
void TTrackBar::MouseUpEvent(){OnMouseUp.Execute(static_cast<AOlxCtrl*>(this));}
void TTrackBar::OnNativeEvent(const NodeEvent& event){if(event.kind!=EventKind::SliderChange)return;ScrollEvent(event.value);if(event.phase==SliderPhase::End||event.phase==SliderPhase::Click)MouseUpEvent();}
#else
#include "trackbarext.h"
#include "frameext.h"
#include "olxvar.h"

using namespace ctrl_ext;
//..............................................................................
TTrackBar::TTrackBar(wxWindow *Parent, wxWindowID id,
  int value, int min_v, int max_v,
  const wxPoint& pos, const wxSize& size, long style)
: wxSlider(Parent, id, value, min_v, max_v, pos, size, style),
  AOlxCtrl(this),
  this_Val(value),
  OnChange(AOlxCtrl::ActionQueue::New(Actions, evt_change_id)),
  OnMouseUp(AOlxCtrl::ActionQueue::New(Actions, evt_on_mouse_up_id))
{
  Bind(wxEVT_SLIDER, &TTrackBar::ScrollEvent, this);
  Bind(wxEVT_LEFT_UP, &TTrackBar::MouseUpEvent, this);
}
//..............................................................................
void TTrackBar::ScrollEvent(wxCommandEvent& evt) {
  evt.Skip();
  if (this_Val == GetValue())  return;
  this_Val = GetValue();
  OnChange.Execute((AOlxCtrl*)this);
}
//..............................................................................
void TTrackBar::MouseUpEvent(wxMouseEvent& evt) {
  evt.Skip();
  OnMouseUp.Execute((AOlxCtrl*)this);
}

#endif // native widgets / original wx
