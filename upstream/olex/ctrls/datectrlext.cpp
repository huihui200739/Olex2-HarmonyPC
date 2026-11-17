/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "datectrlext.h"
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TDateCtrl::TDateCtrl(NodeBackend& backend,Widget* parent,int id,const CalendarDate& value,Point pos,Size size,long style)
 :NativeDateControl(backend,parent,id,value,pos,size,style),AOlxCtrl(this),Value(GetValue()),OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)){}
void TDateCtrl::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::DateChange){if(!ApplyDateEvent(event))return;auto next=GetValue();if(next==Value)return;Value=next;OnChange.Execute(this);}
  else NativeDateControl::OnNativeEvent(event);
}

#else
#include "datectrlext.h"
#include "frameext.h"
#include "olxvar.h"

using namespace ctrl_ext;
//..............................................................................
TDateCtrl::TDateCtrl(wxWindow *Parent, wxWindowID id, const wxDateTime& value,
const wxPoint& pos, const wxSize& size, long style)
: wxDatePickerCtrl(Parent, id, value, pos, size, style),
  AOlxCtrl(this),
  OnChange(AOlxCtrl::ActionQueue::New(Actions, evt_change_id))
{
  Value = GetValue();
  Bind(wxEVT_DATE_CHANGED, &TDateCtrl::ChangeEvent, this);
}
//..............................................................................
void TDateCtrl::ChangeEvent(wxDateEvent& event) {
  event.Skip();
  if (event.GetDate() == Value)
    return;
  Value = event.GetDate();
  OnChange.Execute(this);
}
//..............................................................................

#endif
