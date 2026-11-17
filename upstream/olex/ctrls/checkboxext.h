/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#ifndef __olx_ctrl_checkbox_H
#define __olx_ctrl_checkbox_H
#include "olxctrlbase.h"
namespace ctrl_ext {
class TCheckBox:public olex2::harmony::ui::Widget,public AActionHandler,public AOlxCtrl {
  olxstr Data,DependMode;
  TActionQueue* ActionQueue=nullptr;
  bool checked_=false;
  olex2::harmony::ui::EventWidget mark_,caption_;
  void Changed(const olex2::harmony::ui::NodeEvent&);
  void Dispatch();
public:
  TCheckBox(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,const olxstr& label=EmptyString(),olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=0);
  ~TCheckBox()override;
  using Widget::IsEnabled;
  void SetActionQueue(TActionQueue&,const olxstr&);
  bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;
  void OnRemove(TActionQueue*)override{ActionQueue=nullptr;}
  DefPropC(olxstr,Data)
  AOlxCtrl::ActionQueue &OnClick,&OnCheck,&OnUncheck;
  void SetCaption(const olxstr&);
  olxstr GetCaption()const;
  bool IsChecked()const{return checked_;}
  void SetChecked(bool);
  void SetEventLifetime(const std::shared_ptr<void>& value){Widget::SetEventLifetime(value);mark_.SetEventLifetime(value);caption_.SetEventLifetime(value);}
  const std::string& LastEventError()const;
};
}
#endif

#else


#ifndef __olx_ctrl_checkbox_H
#define __olx_ctrl_checkbox_H
#include "olxctrlbase.h"

namespace ctrl_ext  {

  class TCheckBox: public wxCheckBox, public AActionHandler, public AOlxCtrl {
    void ClickEvent(wxCommandEvent& event);
    void MouseEnterEvent(wxMouseEvent& event);
    olxstr Data, OnCheckStr, OnUncheckStr, OnClickStr, DependMode;
    TActionQueue *ActionQueue;
  public:
    TCheckBox(wxWindow *Parent, wxWindowID id = -1,
      const wxString& label = wxEmptyString,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize, long style = 0);

    virtual ~TCheckBox() {
      if (ActionQueue != NULL)  ActionQueue->Remove(this);
    }

    void SetActionQueue(TActionQueue& q, const olxstr& dependMode);
    bool Execute(const IOlxObject *, const IOlxObject *, TActionQueue *);
    void OnRemove(TActionQueue *) { ActionQueue = NULL; }

    DefPropC(olxstr, Data)

    AOlxCtrl::ActionQueue &OnClick, &OnCheck, &OnUncheck;

    void SetCaption(const olxstr &T) { SetLabel(T.u_str()); }
    olxstr GetCaption() const { return GetLabel(); }

    bool IsChecked() const { return wxCheckBox::IsChecked(); }
    void SetChecked(bool v) { wxCheckBox::SetValue(v); }
  };
}; // end namespace ctrl_ext
#endif

#endif // native option controls / original wx
