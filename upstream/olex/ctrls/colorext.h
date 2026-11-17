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
#include "native_picker.h"
namespace ctrl_ext {
class TColorCtrl:public olex2::harmony::ui::NativeColorControl,public AOlxCtrl {
protected:
  olxstr Data;
  olex2::harmony::ui::Color Color;
  // wxColour's original default cache is invalid until a user/programmatic
  // assignment. Preserve that first-notification behavior, not a fake value.
  bool HasColorBaseline=false;
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TColorCtrl(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,olex2::harmony::ui::Color value={},olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long=0);
  void SetValue(olex2::harmony::ui::Color value){NativeColorControl::SetValue(value);Color=value;HasColorBaseline=true;}
  DefPropC(olxstr,Data)
  AOlxCtrl::ActionQueue& OnChange;
};
}

#else
#ifndef __olx_ctrl_color_H
#define __olx_ctrl_color_H
#include "olxctrlbase.h"
#include "wx/clrpicker.h"

namespace ctrl_ext {

  class TColorCtrl: public wxColourPickerCtrl, public AOlxCtrl {
  protected:
    void ChangeEvent(wxColourPickerEvent &event);
    olxstr Data;
    wxColor Color;
  public:
    TColorCtrl(wxWindow *Parent, wxWindowID id = -1,
      const wxColor &value = *wxBLACK,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize, long style = 0);
    bool IsReadOnly() const {  return WI.HasWindowStyle(wxTE_READONLY); }
    void SetReadOnly(bool v) { WI.AddWindowStyle(wxTE_READONLY); }

    void SetValue(wxColor v) {
      Color = v;
      wxColourPickerCtrl::SetColour(v);
    }
    wxColor GetValue() const {
      return wxColourPickerCtrl::GetColour();
    }

    DefPropC(olxstr, Data)

    AOlxCtrl::ActionQueue &OnChange;
  };
}; // end namespace ctrl_ext
#endif

#endif
