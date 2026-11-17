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
#include "ctrls/choiceext.h"
#include "primitive_options.h"
namespace glObj {class AGDrawObject;}
class TdlgPrimitive:public TDialog,public AEventsDispatcher {
  struct Alive;std::shared_ptr<Alive> alive_;
  std::vector<std::shared_ptr<TCheckBox>> boxes_;
  std::shared_ptr<TChoice> cbApplyTo;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,column_,scopeLabel_,status_,help_,buttons_;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> actionButtons_;
  bool accepted_=false,helpVisible_=false;
  void Layout();void ToggleHelp();
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*)override;
public:
  TdlgPrimitive(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget*,const olex2::harmony::ui::PrimitiveDisplaySnapshot&,std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  // Copies the real object's display state synchronously; no reference retained.
  TdlgPrimitive(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget*,glObj::AGDrawObject&,std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgPrimitive()override;
  uint32_t DraftMask()const;
  int ScopeSelection()const;
  size_t ScopeChoiceCount()const;
  void SetDraft(uint32_t mask,int selection=-1);
  bool OnOK();void OnCancel();
  bool ResultAccepted()const{return accepted_;}
  // -1 is also a valid all-bits mask. Use the modal result / ResultAccepted,
  // not Mask alone, to distinguish acceptance from cancellation.
  int32_t Mask=-1;short Level=-1;
};

#else
#ifndef __olx_dlg_primtvs_H
#define __olx_dlg_primtvs_H
#include "ctrls.h"
#include "gpcollection.h"

class TdlgPrimitive: public TDialog  {
protected:
  void OnOK(wxCommandEvent& event);
  TPtrList<wxCheckBox> Boxes;
  wxComboBox *cbApplyTo;
  AGDrawObject& Object;
public:
  TdlgPrimitive(TMainFrame *P, AGDrawObject& object);
  virtual ~TdlgPrimitive()  {}
  int32_t Mask;
  short Level;
};

#endif

#endif // native display dialog / original wx
