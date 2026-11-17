#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#pragma once
#include "ctrls.h"
#include "ctrls/checkboxext.h"
#include "ctrls/colorext.h"
#include "ctrls/spinctrlext.h"
#include "ctrls/texteditext.h"
#include "ctrls/btnext.h"
#include "glmaterial.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

class TdlgMatProp: public TDialog, public AEventsDispatcher {
  struct Alive;
  std::shared_ptr<Alive> alive_;
  glObj::TGlMaterial material_;
  std::function<void(const glObj::TGlMaterial&)> apply_;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,content_,footer_,status_;
  std::array<std::shared_ptr<TCheckBox>,10> flags_;
  std::array<std::shared_ptr<TColorCtrl>,8> colors_;
  std::array<std::shared_ptr<TTextEdit>,2> shininess_;
  std::shared_ptr<TSpinCtrl> blendAlpha_,transparency_;
  std::shared_ptr<TCheckBox> transparent_,identity_;
  std::vector<std::shared_ptr<TButton>> buttons_;
  std::vector<std::shared_ptr<olex2::harmony::ui::Widget>> rows_;
  void BuildControls(olex2::harmony::ui::NodeBackend&);
  void Layout();
  void ShowStatus(const olxstr&);
  glObj::TGlMaterial ReadControls() const;
  void WriteControls(const glObj::TGlMaterial&);
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*) override;
  void OnOK();void OnCancel();void OnCopy();void OnPaste();
  static glObj::TGlMaterial& MaterialCopy_();
public:
  TdlgMatProp(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,
    olex2::harmony::ui::Widget*,const glObj::TGlMaterial&,
    std::function<void(const glObj::TGlMaterial&)> apply={},
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgMatProp() override;
  const glObj::TGlMaterial& GetCurrent() const { return material_; }
  void SetCurrent(const glObj::TGlMaterial& value) { material_=value;WriteControls(value); }
};

#else
/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_dlg_matprop_H
#define __olx_dlg_matprop_H
#include "ctrls.h"
#include "gxapp.h"

class TdlgMatProp: public TDialog, public AActionHandler {
private:
  wxCheckBox *cbAmbF, *cbAmbB, *cbDiffF, *cbDiffB, *cbEmmF, *cbEmmB,
      *cbSpecF, *cbSpecB, *cbShnF, *cbShnB, *cbTrans, *cbIDraw,
      *cbBlend;
  TColorCtrl *tcAmbF, *tcAmbB, *tcDiffF, *tcDiffB, *tcEmmF, *tcEmmB,
      *tcSpecF, *tcSpecB;
  TTextEdit *tcShnF, *tcShnB;
  TSpinCtrl *scBlend, *scTrans;
  wxButton* bEditFont;
  wxComboBox *cbApplyTo;
  TChoice *cbPrimitives;
protected:
  void OnOK(wxCommandEvent& event);
  bool Execute(const IOlxObject *, const IOlxObject *, TActionQueue *);
  TTypeList<TGlMaterial> Materials;
  AGDrawObject* Object;
  int FCurrentMaterial;
  void Init(const TGlMaterial& GlM);
  void Update(TGlMaterial& GlM);
  void Update()  {  wxWindow::Update();  }

  static TGlMaterial &MaterialCopy_() {
    static TGlMaterial mc;
    return mc;
  }
  void OnCopy(wxCommandEvent& event);
  void OnPaste(wxCommandEvent& event);
  void OnEditFont(wxCommandEvent& event);
  void Init();
public:
  TdlgMatProp(TMainFrame *ParentFrame, AGDrawObject &GPC);
  TdlgMatProp(TMainFrame *ParentFrame, TGlMaterial& mat);
  ~TdlgMatProp();
  const TGlMaterial& GetCurrent()  const {
    return Materials[FCurrentMaterial];
  }
  void SetCurrent(const TGlMaterial& m) {
    Materials[FCurrentMaterial] = m;
    Init(m);
  }
};

#endif

#endif // Harmony native material dialog / original wx implementation
