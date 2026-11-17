/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "listboxext.h"
#include "olxvar.h"
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TListBox::TListBox(NodeBackend& backend,Widget* parent,int id,Point pos,Size size,long style)
 :TItemList<NativeChoiceList>(backend,parent,id,pos,Size{size.width<0?240:size.width,size.height<0?160:size.height},style,NodeKind::ListBox),AOlxCtrl(this),OnDblClick(AOlxCtrl::ActionQueue::New(Actions,evt_on_dbl_click_id)),OnSelect(AOlxCtrl::ActionQueue::New(Actions,evt_on_select_id)){
  Listen({EventKind::ChoiceChange,EventKind::DoubleClick,EventKind::Key});
}
void TListBox::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::ChoiceChange||event.kind==EventKind::DoubleClick){
    if(!ApplyUserSelection(event.value))return;SetSelection(GetSelection());
    if(event.kind==EventKind::DoubleClick){OnDblClick.Execute(this);return;}
    if(!Data.IsEmpty())TOlxVars::SetVar(Data,GetValue());OnSelect.Execute(this);
  }else if(event.kind==EventKind::Key&&event.key&&event.key->down&&!event.key->control&&!event.key->alt&&!event.key->meta&&!event.key->shift){
    int next=GetSelection(),last=static_cast<int>(GetCount())-1;
    switch(event.key->logical){case LogicalKey::Up:next=next<0?0:next-1;break;case LogicalKey::Down:next=next<0?0:next+1;break;case LogicalKey::Home:next=0;break;case LogicalKey::End:next=last;break;default:return;}
    event.key->handled=true;if(last<0)return;if(next<0)next=0;if(next>last)next=last;if(next==GetSelection())return;
    OnNativeEvent(NodeEvent{EventKind::ChoiceChange,static_cast<double>(next)});
  }
}

#else
#include "listboxext.h"
#include "frameext.h"
#include "olxvar.h"

using namespace ctrl_ext;
//..............................................................................
TListBox::TListBox(wxWindow *Parent, wxWindowID id,
  const wxPoint& pos, const wxSize& size, long style)
: AOlxCtrl(this),
  OnSelect(AOlxCtrl::ActionQueue::New(Actions, evt_on_select_id)),
  OnDblClick(AOlxCtrl::ActionQueue::New(Actions, evt_on_dbl_click_id))
{
  wxListBox::Create(Parent, id, pos, size, 0, 0, style);
  Bind(wxEVT_LEFT_DCLICK, &TListBox::DblClickEvent, this);
  Bind(wxEVT_LISTBOX, &TListBox::ItemSelectEvent, this);
}
void TListBox::DblClickEvent(wxMouseEvent& event) {
  event.Skip();
  OnDblClick.Execute(this);
}
//..............................................................................
void TListBox::ItemSelectEvent(wxCommandEvent& event) {
  event.Skip();
  if (!Data.IsEmpty())
    TOlxVars::SetVar(Data, GetValue());
  OnSelect.Execute(this);
}
//..............................................................................


#endif // native inputs / original wx
