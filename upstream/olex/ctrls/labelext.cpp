/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "labelext.h"
#include "eutf8.h"
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
TLabel::TLabel(NodeBackend& backend,Widget* parent,const olxstr& label,long style)
  :Widget(backend,NodeKind::Text,parent,-1,{}, {},style),AOlxCtrl(this),Data(EmptyString()),
   OnClick(AOlxCtrl::ActionQueue::New(Actions,evt_on_click_id)) {
  SetCaption(label);Listen({EventKind::Click});
}
void TLabel::SetCaption(const olxstr& caption){olxcstr utf8=TUtf8::Encode(caption);SetText(TextProperty::Content,std::string(utf8.c_str(),utf8.Length()));}
olxstr TLabel::GetCaption()const{auto value=GetText(TextProperty::Content);return TUtf8::Decode(value.c_str(),value.size());}
void TLabel::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Click)OnClick.Execute(this);}
#else
#include "labelext.h"
#include "frameext.h"
#include "egc.h"

using namespace ctrl_ext;
//..............................................................................
void TLabel::ClickEvent(wxMouseEvent& event)  {
  event.Skip();
  OnClick.Execute(this);
}
void TLabel::OnPaint(wxPaintEvent& event) {
#ifdef _WIN32
  wxPaintDC dc(this);
  dc.SetBrush(GetBackgroundColour());
  dc.SetPen(GetForegroundColour());
  //dc.Clear();
  wxStaticText::OnPaint(event);
#else
  event.Skip(true);
#endif
}
//..............................................................................
void TLabel::onEraseBG(wxEraseEvent& event) {
#ifdef _WIN32
  event.Skip();
#else
  event.Skip(true);
#endif
}

#endif // native widgets / original wx
