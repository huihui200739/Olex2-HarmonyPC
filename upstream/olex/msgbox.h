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
#include "ctrls.h"
#include "ctrls/checkboxext.h"
#include "native_message_icons.h"
// Explicit native flags, not wx headers or pretend wx objects.
enum NativeMessageFlag:uint32_t {
  MessageOk=1u<<0,MessageYes=1u<<1,MessageNo=1u<<2,MessageCancel=1u<<3,
  MessageExclamation=1u<<4,MessageHand=1u<<5,MessageError=1u<<6,
  MessageInformation=1u<<7,MessageQuestion=1u<<8
};
enum NativeMessageResult:int {MessageResultOk=1,MessageResultYes=2,MessageResultNo=3,MessageResultCancel=olex2::harmony::ui::DialogCancelled};
class TdlgMsgBox:public TDialog,public AEventsDispatcher {
  struct Alive;
  struct Button {int result;olxstr caption;std::shared_ptr<olex2::harmony::ui::EventWidget> node;};
  std::shared_ptr<Alive> alive_;
  std::shared_ptr<olex2::harmony::ui::UiScheduler> scheduler_;
  std::unique_ptr<TTimer> timer_;
  std::shared_ptr<TCheckBox> cbRemember;
  std::vector<Button> buttons;
  std::vector<std::shared_ptr<olex2::harmony::ui::Widget>> buttonRows_;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,column_,messageRow_,image_,message_;
  uint64_t SelfTimeout=0,ShownAt=0;int SelfTimeoutId=0;
  void LayoutMessage();void BeginTimeout();void UpdateCountdown();
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*)override;
public:
  TdlgMsgBox(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget* parent,
    const olxstr& message,const olxstr& title,const olxstr& tickBoxMessage="Remember my decision",
    uint32_t flags=MessageOk|MessageInformation,bool showRemember=false,long selfTimeout=0,int selfTimeoutId=0,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgMsgBox()override;
  bool IsChecked()const{return cbRemember&&cbRemember->IsChecked();}
  void SetChecked(bool);
  bool Show(bool show=true);
  void ShowModalEx(bool manageParent,std::function<void(int)> completion);
  void SetSize(olex2::harmony::ui::Size);
  olxstr Result(int result)const;
  static std::shared_ptr<TdlgMsgBox> Execute(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,
    olex2::harmony::ui::Widget* parent,const olxstr& message,const olxstr& title,
    std::function<void(const olxstr&)> completion,const olxstr& tickBoxMessage="Remember my decision",
    uint32_t flags=MessageOk|MessageInformation,bool showRemember=false,long selfTimeout=0,int selfTimeoutId=0,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
};

#else
#ifndef __olx_dlg_msgbox_H
#define __olx_dlg_msgbox_H
#include "ctrls.h"

class TdlgMsgBox: public TDialog, public AEventsDispatcher {
protected:
  wxCheckBox* cbRemember;
  bool Dispatch(int MsgId, short MsgSubId, const IOlxObject* Sender,
    const IOlxObject* Data, TActionQueue*);
  TPtrList<TButton> buttons;
  uint64_t SelfTimeout, ShownAt;
  wxWindowID SelfTimeoutId;
public:
  TdlgMsgBox(TMainFrame* Parent, const olxstr& msg, const olxstr& title,
    const olxstr& tickBoxMsg, long flags, bool ShowRememberCheckBox,
    long selfTimeout=0, wxWindowID selfTimeoutId=0);
  virtual ~TdlgMsgBox();
  bool IsChecked()  {  return cbRemember == 0 ? false : cbRemember->IsChecked();  }
  // return a string YNCO for yes, no, cancel, or OK, R-remember choice
  static olxstr Execute(TMainFrame* Parent, const olxstr& msg, const olxstr& title,
    const olxstr& tickBoxMsg ="Remember my decision",
    long flags=wxID_OK|wxICON_INFORMATION,
    bool ShowRememberCheckBox=false,
    long selfTimeout=0, wxWindowID selfTimeoutId=0);
};
#endif

#endif // native message box / original wx
