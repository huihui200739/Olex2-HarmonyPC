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
#include "itemlist.h"
#include "native_editable.h"
namespace ctrl_ext {
class TComboBox:public TItemList<olex2::harmony::ui::NativeComboList>,public AOlxCtrl {
  olxstr Data,StrValue;
  int entered_counter=0;
  bool OnChangeAlways=false;
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TComboBox(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,const olxstr& value=EmptyString(),olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=0);
  ~TComboBox(){_Clear();}
  void Clear();
  bool HasValue()const{return !(IsReadOnly()&&GetSelection()==-1);}
  olxstr GetValue()const{return HasValue()?NativeComboList::GetValue():EmptyString();}
  olxstr GetText()const;
  void SetText(const olxstr&);
  DefPropC(olxstr,Data)
  DefPropBIsSet(OnChangeAlways)
  void HandleOnLeave();
  void HandleOnEnter();
  AOlxCtrl::ActionQueue &OnChange,&OnLeave,&OnEnter,&OnReturn;
};
}

#else
#ifndef __olx_ctrl_combo_H
#define __olx_ctrl_combo_H
#include "olxctrlbase.h"
#include "itemlist.h"
#include "estrlist.h"
#include "wx/combo.h"

namespace ctrl_ext {
  class TComboBox : public TItemList<wxComboBox>, public AOlxCtrl {
    void ChangeEvent(wxCommandEvent& event);
    void EnterPressedEvent(wxCommandEvent& event);
    void LeaveEvent(wxFocusEvent& event);
    void EnterEvent(wxFocusEvent& event);
    olxstr Data;
    olxstr StrValue;
    int entered_counter;
    bool OnChangeAlways;
  public:
    TComboBox(wxWindow *Parent, wxWindowID id = -1,
      const wxString &value = wxEmptyString,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      long style = wxTE_PROCESS_ENTER);
    virtual ~TComboBox();

    void Clear();

    bool HasValue() const {
      return !(IsReadOnly() && GetSelection() == -1);
    }

    wxString GetValue() const {
      return (HasValue() ? wxComboBox::GetValue() : wxString(wxEmptyString));
    }

    olxstr GetText() const;
    void SetText(const olxstr &text);

    DefPropC(olxstr, Data)

    bool IsReadOnly() const { return WI.HasWindowStyle(wxCB_READONLY); }
    void SetReadOnly(bool v) {
      v ? WI.AddWindowStyle(wxCB_READONLY) : WI.DelWindowStyle(wxCB_READONLY);
    }
    DefPropBIsSet(OnChangeAlways)

    void HandleOnLeave();
    void HandleOnEnter();

    ActionQueue &OnChange, &OnLeave, &OnEnter, &OnReturn;
  };
};  // end namespace ctrl_ext
#endif

#endif // native inputs / original wx
