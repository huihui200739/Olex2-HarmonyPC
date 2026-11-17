/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#pragma once
#include "olxctrlbase.h"
#include "native_editable.h"
namespace ctrl_ext {
class TSpinCtrl:public olex2::harmony::ui::NativeSpinControl,public AOlxCtrl {
  int Value;
  void NotifyChanged();
protected:
  olxstr Data;
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TSpinCtrl(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,const olxstr& value=EmptyString(),olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=olex2::harmony::ui::SpinArrowKeys|olex2::harmony::ui::InputAlignRight);
  void SetValue(int value){NativeSpinControl::SetValue(value);Value=GetValue();}
  void SetRange(int minimum,int maximum){NativeSpinControl::SetRange(minimum,maximum);Value=GetValue();}
  DefPropC(olxstr,Data)
  AOlxCtrl::ActionQueue& OnChange;
};
}

#else
#ifndef __olx_ctrl_spin_H
#define __olx_ctrl_spin_H
#include "olxctrlbase.h"
#include "wx/spinctrl.h"

namespace ctrl_ext  {
  class TSpinCtrl: public wxSpinCtrl, public AOlxCtrl {
  private:
    int Value;
  protected:
    void SpinChangeEvent(wxSpinEvent& event);
    void TextChangeEvent(wxCommandEvent& event);
    void LeaveEvent(wxFocusEvent& event);
    void EnterEvent(wxFocusEvent& event);
    void EnterPressedEvent(wxCommandEvent& event);
    void PaintEvent(wxPaintEvent& event);
    olxstr Data;
  public:
    TSpinCtrl(wxWindow *Parent, wxWindowID id = -1,
      const wxString &value = wxEmptyString,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      long style = wxSP_ARROW_KEYS | wxALIGN_RIGHT);

    DefPropC(olxstr, Data)

    int GetValue() const {  return wxSpinCtrl::GetValue(); }
    void SetValue(int v) {
      Value = v;
      wxSpinCtrl::SetValue(v);
    }

    AOlxCtrl::ActionQueue &OnChange;
  };
}; // end namespace ctrl_ext
#endif

#endif // native inputs / original wx
