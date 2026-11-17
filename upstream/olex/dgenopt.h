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
#include "ctrls/comboext.h"
#include "ctrls/texteditext.h"
#include <array>
namespace olex2::harmony::ui {class ApplicationRuntime;}
class TdlgGenerate:public TDialog,public AEventsDispatcher {
  struct Alive;std::shared_ptr<Alive> alive_;
  struct Axis {
    std::shared_ptr<olex2::harmony::ui::Widget> row,labelFrom,labelTo;
    std::shared_ptr<TTextEdit> from,to;
    std::shared_ptr<TComboBox> range;
  };
  std::array<Axis,3> axes_;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,rows_,buttons_,status_;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> actionButtons_;
  bool accepted_=false,helpVisible_=false;
  void Layout();void UpdateRange(size_t);void ToggleHelp();
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*)override;
  void SetFromText(size_t,const olxstr&);void SetToText(size_t,const olxstr&);
  olxstr GetFromText(size_t)const;olxstr GetToText(size_t)const;olxstr GetRangeText(size_t)const;
public:
  TdlgGenerate(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget*,std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgGenerate()override;
  bool OnOK();void OnCancel();
  bool ResultAccepted()const{return accepted_;}
  DefPropP(float,AFrom) DefPropP(float,BFrom) DefPropP(float,CFrom)
  DefPropP(float,ATo) DefPropP(float,BTo) DefPropP(float,CTo)
private:
  float AFrom=-1,BFrom=-1,CFrom=-1,ATo=1,BTo=1,CTo=1;
};

#else
#ifndef __olx_dlg_genoptdlg_H
#define __olx_dlg_genoptdlg_H
#include "ctrls.h"

class TdlgGenerate: public TDialog, AActionHandler  {
private:
  wxStaticText *stAFrom, *stBFrom, *stCFrom, *stATo, *stBTo, *stCTo;
  wxTextCtrl *tcAFrom, *tcBFrom, *tcCFrom, *tcATo, *tcBTo, *tcCTo;
  TComboBox *cbA, *cbB, *cbC;
protected:
  void OnOK(wxCommandEvent& event);
  void OnLinkChanges(wxCommandEvent& event);
  void OnAChange();
  void OnBChange();
  void OnCChange();
  bool Execute(const IOlxObject *Sender, const IOlxObject *Data, TActionQueue *);

  float AFrom, BFrom, CFrom, ATo, BTo, CTo;
public:
  TdlgGenerate(TMainFrame *ParentFrame);
  virtual ~TdlgGenerate();
  DefPropP(float, AFrom)
  DefPropP(float, BFrom)
  DefPropP(float, CFrom)
  DefPropP(float, ATo)
  DefPropP(float, BTo)
  DefPropP(float, CTo)
};
#endif

#endif // native generation options / original wx
