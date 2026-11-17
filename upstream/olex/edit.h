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
#include "native_editable.h"
#include "ctrls/texteditext.h"
#include <memory>
#include <vector>
namespace olex2::harmony::ui { class ApplicationRuntime; }
class TdlgEdit: public ctrl_ext::TDialog {
  struct Alive;
  std::shared_ptr<Alive> alive_;
  std::shared_ptr<ctrl_ext::TTextEdit> Text;
  std::shared_ptr<olex2::harmony::ui::Widget> Buttons;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> ActionButtons;
  bool accepted_=false;
  bool multiline_=false;
  void Accept();
  void Cancel();
public:
protected:
  std::shared_ptr<olex2::harmony::ui::Widget> Status;
  void Layout();
public:
  TdlgEdit(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,
    olex2::harmony::ui::Widget* parent, bool MultiLine,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgEdit() override;
  void SetText(const olxstr& text);
  olxstr GetText() const;
  void SetTitle(const olxstr& title);
  bool ResultAccepted() const { return accepted_; }
  bool IsMultiLine() const { return multiline_; }
};

// Harmony provides the original plain-text editing workflow. The legacy lexer
// identifier is retained for callers, but ArkUI TextArea has no token-color
// interface in this adapter; SetLexer displays that capability limitation.
class TdlgStyledEdit: public TdlgEdit {
  int LexerStyle=0;
public:
  using TdlgEdit::TdlgEdit;
  void SetLexer(int style);
  int GetLexer() const { return LexerStyle; }
  bool SyntaxHighlightingAvailable() const { return false; }
};
#else


#ifndef __olx_editdlg_H
#define __olx_editdlg_H
#include "ctrls.h"

class TdlgEdit: public wxDialog  {
private:
  wxTextCtrl *Text;
  TMainFrame *FParent;
public:
  TdlgEdit(TMainFrame *ParentFrame, bool MultiLine);
  ~TdlgEdit();
  void SetText(const olxstr& Text);
  olxstr GetText();
  TWindowInterface WI;
};

class TdlgStyledEdit: public wxDialog  {
private:
  wxStyledTextCtrl *Text;
  TMainFrame *FParent;
public:
  TdlgStyledEdit(TMainFrame *ParentFrame, bool MultiLine);
  ~TdlgStyledEdit();
  void SetText(const olxstr& Text);
  void SetLexer(int style);
  olxstr GetText();
  TWindowInterface WI;
};
#endif

#endif // native editor / original wx
