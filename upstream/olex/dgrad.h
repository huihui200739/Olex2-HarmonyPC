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
#include "ctrls/colorext.h"
#include "gradient_options.h"
namespace olex2::harmony {class ApplicationRuntime;}
class TdlgGradient:public TDialog,public AEventsDispatcher {
  struct Alive;std::shared_ptr<Alive> alive_;
  std::shared_ptr<olex2::harmony::ui::GradientStore> store_;
  std::array<std::shared_ptr<TColorCtrl>,4> colors_;
  std::array<std::shared_ptr<olex2::harmony::ui::Widget>,4> labels_,pairs_;
  std::array<std::shared_ptr<olex2::harmony::ui::Widget>,2> rows_;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,column_,description_,status_,buttons_;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> actionButtons_;
  bool applied_=false,uncertain_=false;std::string saveError_;
  void Layout();void CloseEditors();
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*)override;
  int PackedCorner(size_t)const;void SetPackedCorner(size_t,int);
public:
  TdlgGradient(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget*,std::shared_ptr<olex2::harmony::ui::GradientStore>,std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  TdlgGradient(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget*,olex2::harmony::ApplicationRuntime&,std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgGradient()override;
  olex2::harmony::ui::GradientColors GetDraft()const;
  void SetDraft(const olex2::harmony::ui::GradientColors&);
  bool OnOK();void OnCancel();
  bool ColorsApplied()const{return applied_;}
  bool BackgroundMayHaveChanged()const{return uncertain_;}
  const std::string& LastSaveError()const{return saveError_;}
  int GetA()const{return PackedCorner(0);}void SetA(int v){SetPackedCorner(0,v);}
  int GetB()const{return PackedCorner(1);}void SetB(int v){SetPackedCorner(1,v);}
  int GetC()const{return PackedCorner(2);}void SetC(int v){SetPackedCorner(2,v);}
  int GetD()const{return PackedCorner(3);}void SetD(int v){SetPackedCorner(3,v);}
};

#else
#ifndef __olx_graddlg_H
#define __olx_graddlg_H
#include "ctrls.h"

class TdlgGradient: public TDialog, AActionHandler  {
private:
  TTextEdit *tcA, *tcB, *tcC, *tcD;
  wxStaticText *stcA, *stcB, *stcC, *stcD;
protected:
  void OnOK(wxCommandEvent& event);
  bool Execute(const IOlxObject *Sender, const IOlxObject *Data, TActionQueue *);
  int A, B, C, D;
  void Init();
public:
  TdlgGradient(TMainFrame *ParentFrame);
  virtual ~TdlgGradient();
  DefPropP(int, A)
  DefPropP(int, B)
  DefPropP(int, C)
  DefPropP(int, D)
};
#endif

#endif // native display dialog / original wx
