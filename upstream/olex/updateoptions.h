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
#include "ctrls/texteditext.h"
#include "ctrls/comboext.h"
#include "ctrls/choiceext.h"
#include "ctrls/checkboxext.h"
#include "update_options_store.h"
class TdlgUpdateOptions:public TDialog,public AEventsDispatcher {
  struct Alive;std::shared_ptr<Alive> alive_;
  std::shared_ptr<olex2::harmony::ui::UpdateOptionsStore> store_;
  std::shared_ptr<TTextEdit> tcProxy;
  std::shared_ptr<TComboBox> cbRepository;
  std::shared_ptr<TCheckBox> cbQueryUpdate;
  std::shared_ptr<TChoice> rbUpdateInterval;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,column_,stLastUpdated,status_,help_,buttons_;
  std::vector<std::shared_ptr<olex2::harmony::ui::Widget>> labels_;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> actionButtons_;
  bool helpVisible_=false,saved_=false;std::string saveError_;
  void Layout();void ToggleHelp();
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*)override;
public:
  TdlgUpdateOptions(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget* parent,
    std::shared_ptr<olex2::harmony::ui::UpdateOptionsStore>,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgUpdateOptions()override;
  olex2::harmony::ui::UpdateOptionsValues GetDraft()const;
  void SetDraft(const olex2::harmony::ui::UpdateOptionsValues&);
  bool OnOK();void OnCancel();
  const std::string& LastSaveError()const{return saveError_;}
  bool SettingsSaved()const{return saved_;}
};

#else
#ifndef __olx_updateoptions_H
#define __olx_updateoptions_H
#include "ctrls.h"
#include "updateapi.h"

class TdlgUpdateOptions: public TDialog  {
  wxStaticText *stProxy, *stProxyUser, *stProxyPasswd,
    *stRepository, *stLastUpdated;
  wxTextCtrl *tcProxy;
  wxComboBox* cbRepository;
  wxCheckBox* cbQueryUpdate;
  wxRadioBox *rbUpdateInterval;
protected:
  void OnOK(wxCommandEvent& event);
  updater::UpdateAPI uapi;
public:

  TdlgUpdateOptions(TMainFrame *ParentFrame);
  virtual ~TdlgUpdateOptions();
};
#endif

#endif // native update options / original wx
