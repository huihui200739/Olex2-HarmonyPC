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
#include "ctrls/frameext.h"
#include "ctrls/checkboxext.h"
#include "ctrls/spinctrlext.h"
#include "ctrls/trackbarext.h"
#include "ctrls/colorext.h"
#include "ctrls/texteditext.h"
#include "ctrls/btnext.h"
#include "ctrls/choiceext.h"
#include "gllightmodel.h"
#include <array>
#include <memory>
#include <vector>
namespace olex2::harmony::ui { class ApplicationRuntime; }
class TdlgSceneProps: public TDialog, public AEventsDispatcher {
  struct Alive;
  std::shared_ptr<Alive> alive_;
  TMainFrame* ParentFrame=nullptr;
  TGlLightModel FLightModel, FOriginalModel;
  int FCurrentLight=0;
  std::shared_ptr<olex2::harmony::ui::Widget> scroll_,content_,footer_,status_;
  std::shared_ptr<TChoice> cbLights,cbFonts;
  std::shared_ptr<TButton> tbEditFont;
  std::shared_ptr<TCheckBox> cbEnabled,cbUniform,cbLocalV,cbTwoSide,cbSmooth;
  std::array<std::shared_ptr<TTrackBar>,4> position_;
  std::array<std::shared_ptr<TTextEdit>,4> positionText_;
  std::array<std::shared_ptr<TColorCtrl>,3> lightColors_;
  std::array<std::shared_ptr<TSpinCtrl>,3> lightAlpha_;
  std::shared_ptr<TSpinCtrl> scSCO,scSExp;
  std::array<std::shared_ptr<TTextEdit>,6> lightValues_;
  std::array<std::shared_ptr<TColorCtrl>,2> modelColors_;
  std::vector<std::shared_ptr<olex2::harmony::ui::Widget>> rows_;
  std::vector<std::shared_ptr<TButton>> buttons_;
  bool helpVisible_=false;
  void BuildControls(olex2::harmony::ui::NodeBackend&);
  void Layout();
  void InitLight(TGlLight&);
  void UpdateLight(TGlLight&);
  void InitLightModel(TGlLightModel&);
  void UpdateLightModel(TGlLightModel&);
  bool Dispatch(int,short,const IOlxObject*,const IOlxObject*,TActionQueue*) override;
  void OnApply();void OnCancel();void OnOpen();void OnSave();void ToggleHelp();
  void ShowStatus(const olxstr&);
public:
  TdlgSceneProps(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,TMainFrame*,
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={});
  ~TdlgSceneProps() override;
  void LoadFromFile(TGlLightModel&,const olxstr&);
  void SaveToFile(TGlLightModel&,const olxstr&);
};

#else
#ifndef __olx_dlg_scenep_H
#define __olx_dlg_scenep_H
#include "ctrls.h"
#include "gloption.h"
#include "gllightmodel.h"

class TdlgSceneProps: public TDialog, public AActionHandler  {
private:
  wxCheckBox *cbEnabled, *cbUniform;
  TSpinCtrl *scSCO, *scAmbA, *scDiffA, *scSpecA, *scSExp;
  TTrackBar *tbX, *tbY, *tbZ, *tbR;
  TColorCtrl *teAmb, *teDiff, *teSpec;
  TTextEdit *teSCX, *teSCY, *teSCZ, *teAA, *teAB, *teAC;
  TTextEdit *teX, *teY, *teZ, *teR;
  // light model
  wxCheckBox *cbLocalV, *cbTwoSide, *cbSmooth;
  TColorCtrl *tcAmbLM, *tcBgClr;
  TButton* tbEditFont;
  TChoice *cbLights, *cbFonts;
  TGlOption ConsoleFontColor, NotesFontColor, LabelsFontColor, XColor;
protected:
  void OnOK(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);
  void OnOpen(wxCommandEvent& event);
  void OnSave(wxCommandEvent& event);
  void OnApply(wxCommandEvent& event);
  bool Execute(const IOlxObject *Sender, const IOlxObject *Data, TActionQueue *);
  TGlLightModel FLightModel, FOriginalModel;
  int FCurrentLight;
  void InitLight( TGlLight &GlL );
  void UpdateLight( TGlLight &GlL );
  void InitLightModel( TGlLightModel &GlLM );
  void UpdateLightModel( TGlLightModel &GlLM );
  void Update(){  wxWindow::Update(); };
public:
  TdlgSceneProps(TMainFrame *ParentForm);
  virtual ~TdlgSceneProps();
  void LoadFromFile(TGlLightModel &FLM, const olxstr &FN);
  void SaveToFile(TGlLightModel &FLM, const olxstr &FN);
};
#endif

#endif // Harmony scene-parameters dialog / original wx dialog
