/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_ctrl_label_H
#define __olx_ctrl_label_H
#include "olxctrlbase.h"

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
namespace ctrl_ext {
class TLabel:public olex2::harmony::ui::Widget,public AOlxCtrl {
  olxstr Data;
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TLabel(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,const olxstr&,long style=0);
  DefPropC(olxstr, Data)
  AOlxCtrl::ActionQueue& OnClick;
  void SetCaption(const olxstr&);
  olxstr GetCaption()const;
};
}
#else
namespace ctrl_ext  {
  class TLabel: public wxStaticText, public AOlxCtrl  {
    TActionQList *FActions;
    void ClickEvent(wxMouseEvent& event);
    olxstr Data;
    void OnPaint(wxPaintEvent& event);
    void onEraseBG(wxEraseEvent& event);
  public:
    TLabel(wxWindow *Parent, const olxstr &label, long style=0) :
      AOlxCtrl(this),
      OnClick(AOlxCtrl::ActionQueue::New(Actions, evt_on_click_id)),
      wxStaticText(Parent, wxID_ANY, label.u_str(), wxDefaultPosition, wxDefaultSize, style),
      Data(EmptyString())
    {
      Bind(wxEVT_LEFT_DOWN, &TLabel::ClickEvent, this);
      Bind(wxEVT_PAINT, &TLabel::OnPaint, this);
      Bind(wxEVT_ERASE_BACKGROUND, &TLabel::onEraseBG, this);
    }

    DefPropC(olxstr, Data);

    AOlxCtrl::ActionQueue &OnClick;

    void SetCaption(const olxstr &T) {  SetLabel(T.u_str()); }
    olxstr GetCaption() const {  return GetLabel(); }
  };
}; // end namespace ctrl_ext
#endif // native widgets / original wx
#endif
