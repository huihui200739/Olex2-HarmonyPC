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
#include "ctrls/texteditext.h"

// The native command field is a genuine Row containing a non-editable prompt
// and the original TTextEdit. IME, paste, cut and selection can therefore never
// mutate the prompt. Public text/caret positions still include that prefix.
class TCmdLine : public olex2::harmony::ui::Widget, public ctrl_ext::AOlxCtrl {
  class Input final : public ctrl_ext::TTextEdit {
    TCmdLine& owner_;
  protected:
    void OnNativeEvent(const olex2::harmony::ui::NodeEvent&) override;
    void OnInteractionBlocked(const olex2::harmony::ui::NodeEvent&) override;
  public:
    Input(olex2::harmony::ui::NodeBackend&,TCmdLine&,long);
  };
  olex2::harmony::ui::Widget prompt_;
  Input input_;
  olxstr committedCommand_,Data;
  void OnInputEvent(const olex2::harmony::ui::NodeEvent&);
  void SubmitCommand();
protected:
  olxstr PromptStr;
  TStrList Commands;
  std::size_t CmdIndex=0;
public:
  TCmdLine(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int flags=0);
  virtual ~TCmdLine();
  bool ProcessKey(olex2::harmony::ui::KeyEvent&);
  bool WillProcessKey(olex2::harmony::ui::KeyEvent&);
  const olxstr& GetPromptStr()const{return PromptStr;}
  void SetPromptStr(const olxstr&);
  olxstr GetText()const{return PromptStr+input_.GetText();}
  void SetText(const olxstr&);
  olxstr GetCommand()const{return input_.GetText();}
  void SetCommand(const olxstr&);
  const olxstr& GetLastCommand(const olxstr&)const;
  long GetInsertionPoint()const;
  void SetInsertionPoint(long);
  void SetInsertionPointEnd();
  void GetSelection(long*,long*)const;
  void SetSelection(long,long);
  void SetFocus(){input_.SetNumber(olex2::harmony::ui::Property::Focused,1);}
  bool IsReadOnly()const{return input_.IsReadOnly();}
  void SetReadOnly(bool value){SetWindowStyle(value?GetWindowStyle()|olex2::harmony::ui::InputReadOnly:GetWindowStyle()&~olex2::harmony::ui::InputReadOnly);}
  void SetWindowStyle(long)override;
  std::string LastEventError()const;
  olex2::harmony::ui::NodeHandle EntryHandle()const{return input_.Handle();}
  olex2::harmony::ui::NodeHandle PromptHandle()const{return prompt_.Handle();}
  using Widget::GetText;
  using Widget::SetText;
  DefPropC(olxstr,Data)
  TActionQueue& OnCommand;
  AOlxCtrl::ActionQueue &OnChange,&OnReturn,&OnLeave,&OnEnter,&OnClick,&OnChar,&OnKeyDown;
};

#else
#ifndef __olx_cmdline_H
#define __olx_cmdline_H
#include "ctrls.h"

class TCmdLine : public TTextEdit {
protected:
  olxstr PromptStr;
  TStrList Commands;
  int CmdIndex;
public:
  TCmdLine(wxWindow *parent, int flags);
  virtual ~TCmdLine();

  bool ProcessKey(wxKeyEvent& evt);
  // checks only Del, Backspace
  bool WillProcessKey(wxKeyEvent& evt);

  DefPropC(olxstr, PromptStr)

  olxstr GetCommand()  {
    return GetText().Length() > PromptStr.Length()
      ? GetText().SubStringFrom(PromptStr.Length())
      : EmptyString();
  }
  void SetCommand(const olxstr& cmd);
  const olxstr& GetLastCommand(const olxstr &name) const;

  TActionQueue &OnCommand;
};

#endif

#endif // native command line / original wx
