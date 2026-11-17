/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "colorext.h"
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TColorCtrl::TColorCtrl(NodeBackend& backend,Widget* parent,int id,olex2::harmony::ui::Color value,Point pos,Size size,long style)
 :NativeColorControl(backend,parent,id,value,pos,size,style),AOlxCtrl(this),OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)){}
void TColorCtrl::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::ColorChange){if(!ApplyColorEvent(event))return;auto next=GetValue();if(HasColorBaseline&&SameColor(next,Color))return;Color=next;HasColorBaseline=true;OnChange.Execute(this);}
  else NativeColorControl::OnNativeEvent(event);
}

#else
#include "colorext.h"
#include "frameext.h"
#include "olxvar.h"

using namespace ctrl_ext;

//..............................................................................
TColorCtrl::TColorCtrl(wxWindow *Parent, wxWindowID id, const wxColor &value,
  const wxPoint& pos, const wxSize& size, long style)
: wxColourPickerCtrl(Parent, id, value, pos, size, style),
  AOlxCtrl(this),
  OnChange(AOlxCtrl::ActionQueue::New(Actions, evt_change_id))
{
  Bind(wxEVT_COLOURPICKER_CHANGED, &TColorCtrl::ChangeEvent, this);
}
//..............................................................................
void TColorCtrl::ChangeEvent(wxColourPickerEvent& event) {
  event.Skip();
  wxColor c = GetColour();
  if (Color == c) {
    return;
  }
  Color = c;
  OnChange.Execute(this);
}
//..............................................................................

#endif
