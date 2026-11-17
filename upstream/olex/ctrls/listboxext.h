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
#include "native_choice.h"
namespace ctrl_ext {
class TListBox:public TItemList<olex2::harmony::ui::NativeChoiceList>,public AOlxCtrl {
  olxstr Data;
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TListBox(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=0);
  ~TListBox(){_Clear();}
  DefPropC(olxstr,Data)
  void Clear(){_Clear();NativeChoiceList::Clear();}
  olxstr GetValue(size_t i)const{auto value=_GetText(i);return value.a?value.b:GetItem(i);}
  bool SetValue(const olxstr& value){auto found=_SetText(value);SetSelection(found.a==InvalidIndex?-1:static_cast<int>(found.a));return found.a!=InvalidIndex;}
  olxstr GetValue()const{int i=GetSelection();return i<0?EmptyString():_GetText(static_cast<size_t>(i)).b;}
  AOlxCtrl::ActionQueue& OnDblClick;
  AOlxCtrl::ActionQueue& OnSelect;
};
}

#else
#ifndef __olx_ctrl_listbox_H
#define __olx_ctrl_listbox_H
#include "olxctrlbase.h"
#include "estrlist.h"
#include "itemlist.h"
#include "wx/listbox.h"
#include "wx/listctrl.h"

namespace ctrl_ext {

  class TListBox : public TItemList<wxListBox>, public AOlxCtrl {
    void DblClickEvent(wxMouseEvent& event);
    void ItemSelectEvent(wxCommandEvent& event);
    olxstr Data;
  public:
    TListBox(wxWindow *Parent, wxWindowID id = -1,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize, long style = 0);
    
    ~TListBox() {
      _Clear();
    }

    DefPropC(olxstr, Data)

    void Clear() {
      _Clear();
      wxListBox::Clear();
    }

    olxstr GetValue(size_t i) const {
      olx_pair_t<bool, olxstr> v = _GetText(i);
      return (v.a ? v.b : GetItem(i));
    }

    bool SetValue(const olxstr &v) {
      olx_pair_t<size_t, olxstr> found = _SetText(v);
      if (found.a != InvalidIndex) {
        SetSelection((int)found.a);
        return true;
      }
      else {
        SetSelection(-1);
        return false;
      }
    }
    olxstr GetValue() const  {
      int i = GetSelection();
      return (i < 0 ? EmptyString() : _GetText(i).b);
    }

    ActionQueue &OnDblClick, &OnSelect;
  };
}; // end namespace ctrl_ext
#endif

#endif // native inputs / original wx
