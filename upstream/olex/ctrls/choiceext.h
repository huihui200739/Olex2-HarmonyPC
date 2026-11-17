/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#ifndef __olx_ctrl_choice_H
#define __olx_ctrl_choice_H
#include "olxctrlbase.h"
#include "native_choice.h"
#include "itemlist.h"
namespace ctrl_ext {
class TChoice:public TItemList<olex2::harmony::ui::NativeChoiceList>,public AOlxCtrl {
  olxstr Data,StrValue;
  bool entered_=false,hovered_=false,OnChangeAlways=false,hasDefault=false;
  olxstr_dict<olxstr> drawParams;
  uint32_t drawBase_=0;
  void ApplyDrawParams();
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TChoice(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=0);
  ~TChoice()override;
  void Clear();
  olxstr GetValue()const{return GetSelection()<0?EmptyString():GetString(static_cast<size_t>(GetSelection()));}
  void SetCustomDrawParams(const olxstr& value){drawBase_=GetBackgroundColor();drawParams=MapFromToks(value);ApplyDrawParams();}
  olxstr GetText()const;
  void SetText(const olxstr&);
  void AddItems(const TStrList& values){TItemList<olex2::harmony::ui::NativeChoiceList>::AddItems(values);if(HasDefault()&&!IsEmpty())SetSelection(0);}
  DefPropC(olxstr,Data)
  DefPropBIsSet(OnChangeAlways)
  bool HasDefault()const{return hasDefault;}
  void SetHasDefault(bool value){hasDefault=value;}
  void HandleOnLeave();
  void HandleOnEnter();
  ActionQueue &OnChange,&OnLeave,&OnEnter;
};
}
#endif

#else


#ifndef __olx_ctrl_choice_H
#define __olx_ctrl_choice_H
#include "olxctrlbase.h"
#include "itemlist.h"
#include "estrlist.h"
#include "wx/choice.h"

namespace ctrl_ext {
  class TChoice : public TItemList<wxChoice>, public AOlxCtrl {
    void ChangeEvent(wxCommandEvent& event);
    void EnterEvent(wxFocusEvent& event);
    void LeaveEvent(wxFocusEvent& event);
    void PaintEvent(wxPaintEvent& event);
    void MouseEnterEvent(wxMouseEvent& event);
    void MouseLeaveEvent(wxMouseEvent& event);
    olxstr Data;
    olxstr StrValue;
    int entered_counter;
    bool OnChangeAlways, hasDefault;
    olxstr_dict<olxstr> drawParams;
  public:
    TChoice(wxWindow *Parent, wxWindowID id = -1,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize, long style = 0);
    virtual ~TChoice();

    void Clear();

    wxString GetValue() const {
      return (GetSelection() != wxNOT_FOUND ?
        wxChoice::GetString(GetSelection()) : wxString(wxEmptyString));
    }

    void SetCustomDrawParams(const olxstr &params) {
      drawParams = MapFromToks(params);
    }

    olxstr GetText() const;
    void SetText(const olxstr &text);
    void AddItems(const TStrList &items) {
      TItemList<wxChoice>::AddItems(items);
      if (HasDefault() && !IsEmpty()) {
        SetSelection(0);
      }
    }

    DefPropC(olxstr, Data)

    DefPropBIsSet(OnChangeAlways)

    bool HasDefault() const { return hasDefault; }
    void SetHasDefault(bool v) { hasDefault = v; }

    void HandleOnLeave();
    void HandleOnEnter();

    ActionQueue &OnChange, &OnLeave, &OnEnter;
  };
};  // end namespace ctrl_ext
#endif

#endif // native option controls / original wx
