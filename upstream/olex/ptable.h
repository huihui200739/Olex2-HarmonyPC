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
#include "chemdata.h"
#include <array>
#include <vector>
class TPTableDlg:public TDialog {
  struct Alive;std::shared_ptr<Alive> alive_;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,table_,spacer_;
  std::array<std::shared_ptr<olex2::harmony::ui::Widget>,10> rows_;
  std::array<std::array<std::shared_ptr<olex2::harmony::ui::Widget>,18>,10> cells_;
  std::vector<std::shared_ptr<olex2::harmony::ui::EventWidget>> buttons_;
  size_t createdButtons_=0;cm_Element* Selected=nullptr;
  void CreateButton(int row,int column);
  void LayoutViewport();
  void Select(short index);
public:
  TPTableDlg(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,
    olex2::harmony::ui::Widget* parent,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TPTableDlg()override;
  cm_Element* GetSelected(){return Selected;}
  size_t GetButtonCount()const{return buttons_.size();}
};
#else
#ifndef __olx_dlg_ptable_H
#define __olx_dlg_ptable_H
#include "ctrls.h"
#include "chemdata.h"

class TPTableDlg: public TDialog, public AActionHandler  {
protected:
  void CreateButton(int i, int j, int offset);
  TPtrList<TButton> ButtonsList;
  cm_Element *Selected;
public:
  TPTableDlg(TMainFrame *Parent);
  virtual ~TPTableDlg();
  cm_Element* GetSelected()  {  return Selected; }
  bool Execute(const IOlxObject *Sender, const IOlxObject *Data, TActionQueue *);
};
#endif

#endif // native periodic table / original wx
