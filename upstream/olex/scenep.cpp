/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "scenep.h"
#include "gxapp.h"
#include "glfont.h"
#include "glscene.h"
#include "efile.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace olex2::harmony::ui;
namespace {
enum SceneEvent { SceneResize=1, SceneLightChoice, ScenePositionBase=10,
  SceneApply=30, SceneCancel, SceneOpen, SceneSave, SceneFontEdit, SceneHelp, SceneOk,
  SceneCutoff=40 };
std::string SceneUtf8(const olxstr& value) {
  const auto bytes=TUtf8::Encode(value);
  std::string result(bytes.c_str(),bytes.Length());
  if(result.find('\0')!=std::string::npos) throw std::invalid_argument("Scene value contains an embedded NUL");
  return result;
}
Color SceneColor(const TGlOption& value) {
  auto channel=[](float v){return static_cast<uint8_t>(std::lround(std::clamp(v,0.0f,1.0f)*255.0f));};
  return {channel(value[0]),channel(value[1]),channel(value[2]),channel(value[3])};
}
TGlOption SceneOption(Color value, int alphaPercent=-1) {
  const float alpha=alphaPercent<0?value.alpha/255.0f:std::clamp(alphaPercent,0,100)/100.0f;
  return TGlOption(value.red/255.0f,value.green/255.0f,value.blue/255.0f,alpha);
}
}
struct TdlgSceneProps::Alive { TdlgSceneProps* owner=nullptr; };

TdlgSceneProps::TdlgSceneProps(NodeBackend& nodes,
  std::shared_ptr<DialogBackend> dialogs,
  std::shared_ptr<UiScheduler> scheduler,TMainFrame* parent,
  std::shared_ptr<DialogPositionStore> positions)
  :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,
      "Scene Parameters","TdlgSceneProps",
      DialogGeometry{Point{0,0},Size{900,760},true},std::move(positions)),
    alive_(std::make_shared<Alive>()),ParentFrame(parent) {
  // The Harmony dialog host passes no desktop main frame; file open/save stay disabled.
  alive_->owner=this;
  FLightModel=TGXApp::GetInstance().GetRenderer().LightModel;
  FOriginalModel=FLightModel;
  BuildControls(nodes);
  OnResize.Add(this,SceneResize,msiExecute);
  Layout();
}

void TdlgSceneProps::BuildControls(NodeBackend& nodes) {
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());Retain(scroll_);
  content_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());Retain(content_);
  content_->SetNumber(Property::RowSpacing,7);
  auto row=[&](const char* label)->std::shared_ptr<Widget>{
    auto r=std::make_shared<Widget>(nodes,NodeKind::Row,content_.get());Retain(r);
    r->SetNumber(Property::RowSpacing,7);r->SetHeight(46);
    auto l=std::make_shared<Widget>(nodes,NodeKind::Text,r.get());Retain(l);
    l->SetText(TextProperty::Content,label);l->SetWidth(132);l->SetHeight(40);
    return r;
  };
  auto statusRow=std::make_shared<Widget>(nodes,NodeKind::Row,content_.get());Retain(statusRow);
  statusRow->SetHeight(36);
  status_=std::make_shared<Widget>(nodes,NodeKind::Text,statusRow.get());Retain(status_);
  status_->SetText(TextProperty::Accessibility,"Scene settings status");status_->SetWidth(820);status_->SetHeight(32);status_->Hide();

  auto fonts=row("Fonts");
  cbFonts=std::make_shared<TChoice>(nodes,fonts.get());Retain(cbFonts);cbFonts->SetWidth(390);cbFonts->SetHeight(40);
  auto& scene=TGXApp::GetInstance().GetRenderer().GetScene();
  for(size_t i=0;i<scene.FontCount();++i) cbFonts->AddObject(scene._GetFont(i).GetName(),&scene._GetFont(i));
  if(cbFonts->Count()>0) cbFonts->SetSelection(0);
  tbEditFont=std::make_shared<TButton>(nodes,fonts.get(),-1,"Edit Font");Retain(tbEditFont);tbEditFont->SetWidth(150);tbEditFont->SetHeight(40);
  tbEditFont->Enable(false); // A real Harmony font editor is not available yet.
  tbEditFont->OnClick.Add(this,SceneFontEdit,msiExecute);

  auto light=row("Light source");
  cbLights=std::make_shared<TChoice>(nodes,light.get());Retain(cbLights);cbLights->SetWidth(240);cbLights->SetHeight(40);
  for(int i=0;i<8;++i) cbLights->AddObject(olxstr("Light ")<<(i+1));
  cbLights->SetSelection(0);cbLights->OnChange.Add(this,SceneLightChoice,msiExecute);
  cbEnabled=std::make_shared<TCheckBox>(nodes,light.get(),-1,"Enabled");Retain(cbEnabled);cbEnabled->SetWidth(180);cbEnabled->SetHeight(40);

  const char* axes[]={"X position","Y position","Z position","W position"};
  for(size_t i=0;i<position_.size();++i){
    auto r=row(axes[i]);
    position_[i]=std::make_shared<TTrackBar>(nodes,r.get(),-1,0,i==3?-3:-100,i==3?3:100);
    Retain(position_[i]);position_[i]->SetWidth(480);position_[i]->SetHeight(38);
    position_[i]->OnChange.Add(this,static_cast<short>(ScenePositionBase+i),msiExecute);
    positionText_[i]=std::make_shared<TTextEdit>(nodes,r.get(),-1,"0");Retain(positionText_[i]);
    positionText_[i]->SetReadOnly(true);positionText_[i]->SetWidth(70);positionText_[i]->SetHeight(38);
  }
  const char* colorNames[]={"Ambient","Diffuse","Specular"};
  for(size_t i=0;i<lightColors_.size();++i){
    auto r=row(colorNames[i]);
    lightColors_[i]=std::make_shared<TColorCtrl>(nodes,r.get());Retain(lightColors_[i]);
    lightColors_[i]->SetWidth(220);lightColors_[i]->SetHeight(40);
    lightAlpha_[i]=std::make_shared<TSpinCtrl>(nodes,r.get(),-1,"100");Retain(lightAlpha_[i]);
    lightAlpha_[i]->SetRange(0,100);lightAlpha_[i]->SetWidth(140);lightAlpha_[i]->SetHeight(40);
  }
  auto exp=row("Spot exponent");
  scSExp=std::make_shared<TSpinCtrl>(nodes,exp.get(),-1,"0");Retain(scSExp);scSExp->SetRange(0,128);scSExp->SetWidth(150);scSExp->SetHeight(40);
  auto cutoff=row("Spot cutoff");
  scSCO=std::make_shared<TSpinCtrl>(nodes,cutoff.get(),-1,"180");Retain(scSCO);scSCO->SetRange(1,180);scSCO->SetWidth(150);scSCO->SetHeight(40);
  scSCO->OnChange.Add(this,SceneCutoff,msiExecute);
  cbUniform=std::make_shared<TCheckBox>(nodes,cutoff.get(),-1,"Uniform");Retain(cbUniform);cbUniform->SetWidth(180);cbUniform->SetHeight(40);
  const char* valueNames[]={"Attenuation constant","Attenuation linear","Attenuation quadratic","Spot direction X","Spot direction Y","Spot direction Z"};
  for(size_t i=0;i<lightValues_.size();++i){
    auto r=row(valueNames[i]);lightValues_[i]=std::make_shared<TTextEdit>(nodes,r.get(),-1,"0");Retain(lightValues_[i]);
    lightValues_[i]->SetWidth(260);lightValues_[i]->SetHeight(40);
  }
  auto viewer=row("Light model");
  cbLocalV=std::make_shared<TCheckBox>(nodes,viewer.get(),-1,"Local viewer");Retain(cbLocalV);cbLocalV->SetWidth(190);cbLocalV->SetHeight(40);
  cbTwoSide=std::make_shared<TCheckBox>(nodes,viewer.get(),-1,"Two sided");Retain(cbTwoSide);cbTwoSide->SetWidth(170);cbTwoSide->SetHeight(40);
  cbSmooth=std::make_shared<TCheckBox>(nodes,viewer.get(),-1,"Smooth shading");Retain(cbSmooth);cbSmooth->SetWidth(210);cbSmooth->SetHeight(40);
  auto ambient=row("Model ambient color");
  modelColors_[0]=std::make_shared<TColorCtrl>(nodes,ambient.get());Retain(modelColors_[0]);modelColors_[0]->SetWidth(220);modelColors_[0]->SetHeight(40);
  auto background=row("Background color");
  modelColors_[1]=std::make_shared<TColorCtrl>(nodes,background.get());Retain(modelColors_[1]);modelColors_[1]->SetWidth(220);modelColors_[1]->SetHeight(40);

  footer_=std::make_shared<Widget>(nodes,NodeKind::Row,&Body());Retain(footer_);footer_->SetNumber(Property::RowSpacing,8);footer_->SetHeight(48);
  std::weak_ptr<Alive> weak=alive_;
  const char* labels[]={"Apply","Open","Save","Help","Cancel","OK"};
  const int actions[]={SceneApply,SceneOpen,SceneSave,SceneHelp,SceneCancel,SceneOk};
  for(size_t i=0;i<6;++i){
    auto button=std::make_shared<TButton>(nodes,footer_.get(),-1,labels[i]);Retain(button);
    button->SetHeight(42);button->SetNumber(Property::Focusable,1);
    if(actions[i]==SceneOpen||actions[i]==SceneSave)button->Enable(false);
    button->OnClick.Add(this,static_cast<short>(actions[i]),msiExecute);
    buttons_.push_back(std::move(button));
  }
  (void)weak;
  InitLight(FLightModel.GetLight(FCurrentLight));
  InitLightModel(FLightModel);
  ShowStatus("Scene file open/save and font editing are not available on Harmony yet.");
}

TdlgSceneProps::~TdlgSceneProps() {
  alive_->owner=nullptr;OnResize.Remove(this);
  if(cbLights)cbLights->OnChange.Clear();
  if(scSCO)scSCO->OnChange.Clear();
  for(auto& slider:position_)if(slider)slider->OnChange.Clear();
  if(tbEditFont)tbEditFont->OnClick.Clear();
  for(auto& button:buttons_)button->OnClick.Clear();
}

void TdlgSceneProps::Layout() {
  const int width=std::max(320,GetWidth()-24);
  scroll_->SetWidth(width);scroll_->SetHeight(std::max(120,GetHeight()-136));
  content_->SetWidth(width);content_->SetHeight(1040);footer_->SetWidth(width);
  for(auto& button:buttons_)button->SetWidth(std::max(42,(width-40)/6));
}

void TdlgSceneProps::ShowStatus(const olxstr& value) {
  const auto text=SceneUtf8(value);status_->SetText(TextProperty::Content,text);status_->Show(!value.IsEmpty());
}

bool TdlgSceneProps::Dispatch(int event,short,const IOlxObject* sender,const IOlxObject*,TActionQueue*) {
  if(event==SceneResize){Layout();return true;}
  if(event>=ScenePositionBase&&event<ScenePositionBase+4){
    const size_t i=static_cast<size_t>(event-ScenePositionBase);
    if(sender==position_[i].get())positionText_[i]->SetText(position_[i]->GetValue());
    return true;
  }
  if(event==SceneLightChoice&&sender==cbLights.get()){
    if(FCurrentLight>=0&&FCurrentLight<8)UpdateLight(FLightModel.GetLight(static_cast<size_t>(FCurrentLight)));
    const int selected=cbLights->GetSelection();
    if(selected>=0&&selected<8){FCurrentLight=selected;InitLight(FLightModel.GetLight(static_cast<size_t>(selected)));}
    return true;
  }
  if(event==SceneCutoff&&sender==scSCO.get()){cbUniform->SetChecked(scSCO->GetValue()>90);return true;}
  if(event==SceneOk){OnApply();EndModal(1);return true;}
  if(event==SceneApply){OnApply();return true;}
  if(event==SceneCancel){OnCancel();return true;}
  if(event==SceneOpen){OnOpen();return true;}
  if(event==SceneSave){OnSave();return true;}
  if(event==SceneHelp){ToggleHelp();return true;}
  if(event==SceneFontEdit){ShowStatus("The native font editor is not available on this target yet.");return true;}
  return false;
}

void TdlgSceneProps::InitLightModel(TGlLightModel& model) {
  cbLocalV->SetChecked(model.IsLocalViewer());cbTwoSide->SetChecked(model.IsTwoSides());cbSmooth->SetChecked(model.IsSmoothShade());
  modelColors_[0]->SetValue(SceneColor(model.GetAmbientColor()));modelColors_[1]->SetValue(SceneColor(model.GetClearColor()));
}
void TdlgSceneProps::UpdateLightModel(TGlLightModel& model) {
  model.SetLocalViewer(cbLocalV->IsChecked());model.SetTwoSides(cbTwoSide->IsChecked());model.SetSmoothShade(cbSmooth->IsChecked());
  auto ambient=modelColors_[0]->GetValue(),background=modelColors_[1]->GetValue();
  model.SetAmbientColor(SceneOption(ambient));model.SetClearColor(SceneOption(background));
}
void TdlgSceneProps::InitLight(TGlLight& light) {
  for(size_t i=0;i<4;++i){const int value=static_cast<int>(light.GetPosition()[i]);position_[i]->SetValue(value);positionText_[i]->SetText(value);}
  const TGlOption* colors[]={&light.GetAmbient(),&light.GetDiffuse(),&light.GetSpecular()};
  for(size_t i=0;i<3;++i){lightColors_[i]->SetValue(SceneColor(*colors[i]));lightAlpha_[i]->SetValue(static_cast<int>(std::lround((*colors[i])[3]*100.0f)));}
  const auto& attenuation=light.GetAttenuation();
  lightValues_[0]->SetText(attenuation[0]);lightValues_[1]->SetText(attenuation[1]);lightValues_[2]->SetText(attenuation[2]);
  const auto& direction=light.GetSpotDirection();
  lightValues_[3]->SetText(direction[0]);lightValues_[4]->SetText(direction[1]);lightValues_[5]->SetText(direction[2]);
  cbEnabled->SetChecked(light.IsEnabled());scSExp->SetValue(light.GetSpotExponent());scSCO->SetValue(light.GetSpotCutoff());cbUniform->SetChecked(light.GetSpotCutoff()==180);
}
void TdlgSceneProps::UpdateLight(TGlLight& light) {
  light.SetPosition(TGlOption(position_[0]->GetValue(),position_[1]->GetValue(),position_[2]->GetValue(),position_[3]->GetValue()));
  light.SetAmbient(SceneOption(lightColors_[0]->GetValue(),lightAlpha_[0]->GetValue()));
  light.SetDiffuse(SceneOption(lightColors_[1]->GetValue(),lightAlpha_[1]->GetValue()));
  light.SetSpecular(SceneOption(lightColors_[2]->GetValue(),lightAlpha_[2]->GetValue()));
  light.SetAttenuation(TGlOption(lightValues_[0]->GetText().ToDouble(),lightValues_[1]->GetText().ToDouble(),lightValues_[2]->GetText().ToDouble()));
  light.SetEnabled(cbEnabled->IsChecked());light.SetSpotExponent(scSExp->GetValue());
  light.SetSpotCutoff(cbUniform->IsChecked()?180:scSCO->GetValue());
  light.SetSpotDirection(TGlOption(lightValues_[3]->GetText().ToDouble(),lightValues_[4]->GetText().ToDouble(),lightValues_[5]->GetText().ToDouble()));
}
void TdlgSceneProps::OnApply() {
  UpdateLight(FLightModel.GetLight(static_cast<size_t>(FCurrentLight)));UpdateLightModel(FLightModel);
  auto& renderer=TGXApp::GetInstance().GetRenderer();renderer.LightModel=FLightModel;renderer.InitLights();renderer.Draw();
  ShowStatus("Scene settings applied. File open/save and font editing remain unavailable on Harmony.");
}
void TdlgSceneProps::OnCancel() {
  if(IsClosing()){RetryClose();return;}
  auto& renderer=TGXApp::GetInstance().GetRenderer();renderer.LightModel=FOriginalModel;renderer.InitLights();renderer.Draw();EndModal(DialogCancelled);
}
void TdlgSceneProps::OnOpen() {
  ShowStatus("Opening scene files is disabled until Harmony document URI access is connected.");
}
void TdlgSceneProps::OnSave() {
  ShowStatus("Saving scene files is disabled until Harmony document URI access is connected.");
}
void TdlgSceneProps::ToggleHelp() {
  helpVisible_=!helpVisible_;
  ShowStatus(helpVisible_?olxstr("Adjust the selected light and model. Apply updates the preview; cancel restores the original scene. File open/save and font editing are not available on Harmony yet."):olxstr("Scene file open/save and font editing are not available on Harmony yet."));
}
void TdlgSceneProps::LoadFromFile(TGlLightModel&,const olxstr& file) {
  TDataFile data;data.LoadFromXLFile(file,0);auto& renderer=TGXApp::GetInstance().GetRenderer();renderer.GetScene().FromDataItem(data.Root());FLightModel=renderer.LightModel;
}
void TdlgSceneProps::SaveToFile(TGlLightModel&,const olxstr& file) {
  TDataFile data;auto& renderer=TGXApp::GetInstance().GetRenderer();renderer.GetScene().ToDataItem(data.Root());data.SaveToXLFile(file);
}

#else
#include "scenep.h"
#include "wx/fontdlg.h"
#include "wx/colordlg.h"
#include "gxapp.h"
#include "glfont.h"
#include "glscene.h"
#include "efile.h"

#if wxCHECK_VERSION(2,9,0)
#  define wxComp(a,b) (a)
#else
#  define wxComp(a,b) (b)
#endif

//..............................................................................
TdlgSceneProps::TdlgSceneProps(TMainFrame *ParentFrame)
  : TDialog(ParentFrame, wxT("Scene Parameters"),
    EsdlClassName(TdlgSceneProps).u_str() )
{
  AActionHandler::SetToDelete(false);
  short Border = 2;
  // Fonts ********************************************************************
  wxStaticBox *boxFonts = new wxStaticBox(this, -1, wxT("Fonts"));
  cbFonts = new TChoice(wxComp(boxFonts, this));
  AGlScene& ascene = TGXApp::GetInstance().GetRenderer().GetScene();
  for (size_t i = 0; i < ascene.FontCount(); i++) {
    cbFonts->AddObject(ascene._GetFont(i).GetName(), &ascene._GetFont(i));
  }
  cbFonts->SetSelection(0);
  cbFonts->OnChange.Add(this);
  tbEditFont = new TButton(wxComp(boxFonts, this));
  tbEditFont->SetCaption("Edit Font");
  tbEditFont->OnClick.Add(this);
  wxStaticBoxSizer *sizerFonts = new wxStaticBoxSizer(boxFonts, wxHORIZONTAL);
  sizerFonts->Add(cbFonts, 1, wxEXPAND | wxALL, Border); //fonts frame
  sizerFonts->Add(tbEditFont, 1, wxEXPAND | wxALL, Border);
  // Light sources*************************************************************
  wxStaticBox *boxLS = new wxStaticBox(this, -1, wxT("Light sources"));
  //    Light position ********************************************************
  wxStaticBox *boxLP = new wxStaticBox(wxComp(boxLS, this), -1, wxT("Light position"));
  wxStaticText *stX = new wxStaticText(wxComp(boxLP, this), -1, wxT("X"), wxDefaultPosition);
  tbX = new TTrackBar(wxComp(boxLP, this));
    tbX->OnChange.Add(this);
    tbX->SetRange(-100,100);
  teX = new TTextEdit(wxComp(boxLP, this));
    teX->SetReadOnly(true);
  wxStaticText *stY = new wxStaticText(wxComp(boxLP, this), -1, wxT("Y"), wxDefaultPosition);
  tbY = new TTrackBar(wxComp(boxLP, this));
    tbY->OnChange.Add(this);
    tbY->SetRange(-100,100);
  teY = new TTextEdit(wxComp(boxLP, this));
    teY->SetReadOnly(true);
  wxStaticText *stZ = new wxStaticText(wxComp(boxLP, this), -1, wxT("Z"), wxDefaultPosition);
  tbZ = new TTrackBar(wxComp(boxLP, this));
    tbZ->OnChange.Add(this);
    tbZ->SetRange(-100,100);
  teZ = new TTextEdit(wxComp(boxLP, this));
    teZ->SetReadOnly(true);
  wxStaticText *stR = new wxStaticText(wxComp(boxLP, this), -1, wxT("R"), wxDefaultPosition);
  tbR  = new TTrackBar(wxComp(boxLP, this));
    tbR->OnChange.Add(this);
    tbR->SetRange(-3,3);
  teR = new TTextEdit(wxComp(boxLP, this));
    teR->SetReadOnly(true);

  wxFlexGridSizer *LightPosGridSizer = new wxFlexGridSizer(4, 3, Border, Border);
  LightPosGridSizer->Add(stX, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  LightPosGridSizer->Add(tbX, 1, wxEXPAND | wxALL, 1);
  LightPosGridSizer->Add(teX, 0, wxALIGN_CENTER_VERTICAL | wxALL, 1);

  LightPosGridSizer->Add(stY, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  LightPosGridSizer->Add(tbY, 1, wxEXPAND | wxALL, 1);
  LightPosGridSizer->Add(teY, 0, wxALIGN_CENTER_VERTICAL | wxALL, 1);

  LightPosGridSizer->Add(stZ, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  LightPosGridSizer->Add(tbZ, 1, wxEXPAND | wxALL, 1);
  LightPosGridSizer->Add(teZ, 0, wxALIGN_CENTER_VERTICAL | wxALL, 1);

  LightPosGridSizer->Add(stR, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  LightPosGridSizer->Add(tbR, 1, wxEXPAND | wxALL, 1);
  LightPosGridSizer->Add(teR, 0, wxALIGN_CENTER_VERTICAL | wxALL, 1);
  LightPosGridSizer->AddGrowableCol(1);

  wxStaticBoxSizer *sizerLP = new wxStaticBoxSizer(boxLP, wxVERTICAL);
  sizerLP->Add(LightPosGridSizer, 1, wxEXPAND | wxALL, 1);
  //    light colours *********************************************************
  wxStaticBox *boxLC = new wxStaticBox(wxComp(boxLS, this), -1, wxT("Light colours"));
  wxStaticText *stcolour = new wxStaticText(wxComp(boxLC, this), -1, wxT("Colour"));
  wxStaticText *sttransparency = new wxStaticText(wxComp(boxLC, this), -1, wxT("Transparency"));

  wxStaticText *stAmb = new wxStaticText(wxComp(boxLC, this), -1, wxT("Ambient"));
  teAmb = new TColorCtrl(wxComp(boxLC, this));
  scAmbA = new TSpinCtrl(wxComp(boxLC, this));
  wxStaticText *stDiff = new wxStaticText(wxComp(boxLC, this), -1, wxT("Diffuse"));
  teDiff = new TColorCtrl(wxComp(boxLC, this));
  scDiffA = new TSpinCtrl(wxComp(boxLC, this));

  wxStaticText *stSpec = new wxStaticText(wxComp(boxLC, this), -1, wxT("Specular"));
  teSpec  = new TColorCtrl(wxComp(boxLC, this));
  scSpecA = new TSpinCtrl(wxComp(boxLC, this));
  wxStaticText *stSExp = new wxStaticText(wxComp(boxLC, this), -1, wxT("Spot exponent"));
  scSExp = new TSpinCtrl(wxComp(boxLC, this));
    scSExp->SetRange(0, 128);

  wxFlexGridSizer *GridSizer = new wxFlexGridSizer(5, 3, Border, Border);
  GridSizer->Add(-1, 10, 0, wxALIGN_CENTER | wxRIGHT, 2);
  GridSizer->Add(stcolour, 0, wxALIGN_CENTER | wxALL, 1);
  GridSizer->Add(sttransparency, 0, wxALIGN_CENTER | wxALL, 1);

  GridSizer->Add(stAmb, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  GridSizer->Add(teAmb, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);
  GridSizer->Add(scAmbA, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);

  GridSizer->Add(stDiff, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  GridSizer->Add(teDiff, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);
  GridSizer->Add(scDiffA, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);

  GridSizer->Add(stSpec, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  GridSizer->Add(teSpec, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);
  GridSizer->Add(scSpecA, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);

  GridSizer->Add(stSExp, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  GridSizer->Add(scSExp, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxALL, 1);
  GridSizer->AddGrowableCol(1);
  GridSizer->SetSizeHints(this);

  wxStaticBoxSizer *LSizer = new wxStaticBoxSizer(boxLC, wxHORIZONTAL );
  LSizer->Add(GridSizer, 1, wxEXPAND | wxALL, 1);

  wxBoxSizer *TSizer0 = new wxBoxSizer(wxHORIZONTAL);
  TSizer0->Add(LSizer, 1, wxEXPAND | wxALL, Border);//Light frame
  TSizer0->Add(sizerLP, 1, wxEXPAND | wxALL, Border);//Light position frame

  //Light dropdown menu
  cbLights = new TChoice(wxComp(boxLS, this));
  for (int i = 0; i < 8; i++) {
    cbLights->AddObject(olxstr("Light ") << (i + 1));
  }
  cbLights->SetSelection(0);
  cbLights->OnChange.Add(this);
  //end Light dropdown menu

  // checkbox enabled
  cbEnabled = new wxCheckBox(wxComp(boxLS, this), -1, wxT("Enabled"));
  wxBoxSizer *sizerE = new wxBoxSizer(wxHORIZONTAL);
  sizerE->Add(cbEnabled, 1, wxEXPAND | wxALL, Border);
  //end checkbox enabled

  wxBoxSizer *SizerLt = new wxBoxSizer(wxHORIZONTAL);
  SizerLt->Add(cbLights, 1, wxEXPAND | wxALL, Border);
  SizerLt->Add(sizerE, 1, wxEXPAND | wxALL, Border);

  // spot cutoff **************************************************************
  wxStaticBox *boxSC = new wxStaticBox(wxComp(boxLS, this), -1, wxT("Spot cutoff"));
  cbUniform = new wxCheckBox(wxComp(boxSC, this), -1, wxT("Uniform"));
  scSCO = new TSpinCtrl(wxComp(boxSC, this));
  scSCO->SetRange(1, 180);
    scSCO->OnChange.Add(this);
  wxStaticBoxSizer *sizerSC = new wxStaticBoxSizer(boxSC, wxHORIZONTAL);
  sizerSC->Add(scSCO, 0, wxEXPAND | wxALL, Border);
  sizerSC->Add(cbUniform, 0, wxEXPAND | wxALL, Border);
  //spot direction ************************************************************
  wxStaticBox *boxSD = new wxStaticBox(wxComp(boxLS, this), -1, wxT("Spot direction"));
  wxStaticText *stSCX = new wxStaticText(wxComp(boxSD, this), -1, wxT("X"));
  teSCX = new TTextEdit(wxComp(boxSD, this));
  wxStaticText *stSCY = new wxStaticText(wxComp(boxSD, this), -1, wxT("Y"));
  teSCY = new TTextEdit(wxComp(boxSD, this));
  wxStaticText *stSCZ = new wxStaticText(wxComp(boxSD, this), -1, wxT("Z"));
  teSCZ = new TTextEdit(wxComp(boxSD, this));

  wxStaticBoxSizer *sizerSD = new wxStaticBoxSizer(boxSD, wxHORIZONTAL);
  sizerSD->Add(stSCX, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 2);
  sizerSD->Add(teSCX, 1, wxRIGHT, 15);
  sizerSD->Add(stSCY, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  sizerSD->Add(teSCY, 1, wxRIGHT, 15);
  sizerSD->Add(stSCZ, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  sizerSD->Add(teSCZ, 1, wxRIGHT, 2);
  //spot direction frame + spot cut off frame
  wxBoxSizer *sizerSpot = new wxBoxSizer(wxHORIZONTAL);
  sizerSpot->Add(sizerSC, 1, wxEXPAND | wxALL, Border);
  sizerSpot->Add(sizerSD, 1, wxEXPAND | wxALL, Border);
  // attenuation **************************************************************
  wxStaticBox *boxA = new wxStaticBox(wxComp(boxLS, this), -1, wxT("Attenuation"));
  teAA = new TTextEdit(wxComp(boxA, this));  //GL_QUADRATIC_ATTENUATION
  wxStaticText *stAA = new wxStaticText(wxComp(boxA, this), -1, wxT("Quadratic"));
  teAB = new TTextEdit(wxComp(boxA, this));  //GL_LINEAR_ATTENUATION
  wxStaticText *stAB = new wxStaticText(wxComp(boxA, this), -1, wxT("Linear"));
  teAC = new TTextEdit(wxComp(boxA, this));  //GL_CONSTANT_ATTENUATION
  wxStaticText *stAC = new wxStaticText(wxComp(boxA, this), -1, wxT("Constant"));

  wxStaticBoxSizer *sizerA = new wxStaticBoxSizer(boxA, wxHORIZONTAL);
  sizerA->Add(stAA, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 2);
  sizerA->Add(teAA, 1, wxRIGHT, 15);
  sizerA->Add(stAB, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  sizerA->Add(teAB, 1, wxRIGHT, 15);
  sizerA->Add(stAC, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  sizerA->Add(teAC, 1, wxRIGHT, 2);
  // assembling Light Sources
  wxStaticBoxSizer *sizerLS = new wxStaticBoxSizer(boxLS, wxVERTICAL);
  sizerLS->Add(SizerLt, 0, wxEXPAND | wxALL, Border);//Light dropdown menu
  sizerLS->Add(TSizer0, 0, wxEXPAND | wxALL, Border);//Light frame + Light position frame
  sizerLS->Add(sizerSpot, 0, wxEXPAND | wxALL, Border);//spot cut off frame
  sizerLS->Add(sizerA, 0, wxEXPAND | wxALL, Border);//frame attenuation

  //light model ***************************************************************
  wxStaticBox *boxLM = new wxStaticBox(this, -1, wxT("Light model"));
  cbLocalV = new wxCheckBox(wxComp(boxLM, this), -1, wxT("Local viewer"));
  cbTwoSide = new wxCheckBox(wxComp(boxLM, this), -1, wxT("Two side"));
  cbSmooth = new wxCheckBox(wxComp(boxLM, this), -1, wxT("Smooth shade"));
  tcAmbLM = new TColorCtrl(wxComp(boxLM, this));
  wxStaticText *stAmbLM = new wxStaticText(wxComp(boxLM, this), -1, wxT("Ambient color"));
  tcBgClr = new TColorCtrl(wxComp(boxLM, this)); tcBgClr->SetReadOnly(true);
  wxStaticText *stBgClr = new wxStaticText(wxComp(boxLM, this), -1, wxT("Background color"));

  wxStaticBoxSizer *SizerLM = new wxStaticBoxSizer(boxLM, wxHORIZONTAL);
#if !wxCHECK_VERSION(2,9,0)
  wxGridSizer *SizerLM1 = new wxGridSizer(2, 2);
  wxFlexGridSizer *SizerLM2 = new wxFlexGridSizer(2, 2);
#else
  wxGridSizer *SizerLM1 = new wxGridSizer(2, 2, wxDefaultSize);
  wxFlexGridSizer *SizerLM2 = new wxFlexGridSizer(2, 2, wxDefaultSize);
#endif

  SizerLM1->Add(cbLocalV, 0, wxALIGN_CENTER_VERTICAL | wxALL, 0); //light model second column
  SizerLM1->Add(cbTwoSide, 0, wxALIGN_CENTER_VERTICAL | wxALL, 0);
  SizerLM1->Add(cbSmooth, 0, wxALIGN_CENTER_VERTICAL | wxALL, 0);

  SizerLM2->Add(stAmbLM, 0, wxALL, 2);//light model first column
  SizerLM2->Add(tcAmbLM, 0, wxEXPAND | wxALL, 1);
  SizerLM2->Add(stBgClr, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
  SizerLM2->Add(tcBgClr, 0, wxEXPAND | wxALL, 1);

  SizerLM->Add(SizerLM2, 1, wxEXPAND | wxALL, Border);//light model frame
  SizerLM->Add(SizerLM1, 1, wxEXPAND | wxALL, Border);
  //light model frame

  wxBoxSizer *TSizer2 = new wxBoxSizer(wxVERTICAL);
  TSizer2->Add(sizerFonts, 0, wxEXPAND | wxALL, Border);//fonts frame
  TSizer2->Add(-1,10);//spacer
  TSizer2->Add(SizerLM, 0, wxEXPAND | wxALL, Border);//light model frame
  TSizer2->Add(-1,10);
  TSizer2->Add(sizerLS, 1, wxEXPAND | wxALL, Border);//big light frame

  //right buttons list
  wxBoxSizer *ButtonsSizer = new wxBoxSizer(wxVERTICAL);
  ButtonsSizer->Add(
    new wxButton(this, wxID_OK, wxT("OK") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_CANCEL, wxT("Cancel") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_OPEN, wxT("Open") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_SAVE, wxT("Save") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_APPLY, wxT("Apply") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_HELP, wxT("Help") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->SetSizeHints(this);
  //end right buttons list

  wxBoxSizer *TSizer3 = new wxBoxSizer(wxHORIZONTAL);
  TSizer3->Add(TSizer2, 1, wxALL, 5);
  TSizer3->Add(ButtonsSizer, 0, wxALL, 5);
  SetSizer(TSizer3);
  TSizer3->SetSizeHints(this);

  Center();
  Bind(wxEVT_BUTTON, &TdlgSceneProps::OnOK, this, wxID_OK);
  Bind(wxEVT_BUTTON, &TdlgSceneProps::OnCancel, this, wxID_CANCEL);
  Bind(wxEVT_BUTTON, &TdlgSceneProps::OnOpen, this, wxID_OPEN);
  Bind(wxEVT_BUTTON, &TdlgSceneProps::OnSave, this, wxID_SAVE);
  Bind(wxEVT_BUTTON, &TdlgSceneProps::OnApply, this, wxID_APPLY);

  FCurrentLight = 0;
  FLightModel = TGXApp::GetInstance().GetRenderer().LightModel;
  FOriginalModel = TGXApp::GetInstance().GetRenderer().LightModel;
  InitLight(FLightModel.GetLight(0));
  InitLightModel(FLightModel);
}
//..............................................................................
TdlgSceneProps::~TdlgSceneProps()  {
  tbX->OnChange.Clear();
  tbY->OnChange.Clear();
  tbZ->OnChange.Clear();
  tbR->OnChange.Clear();
  scSCO->OnChange.Clear();
  cbLights->OnChange.Clear();
  cbFonts->OnChange.Clear();
  tbEditFont->OnClick.Clear();
}
//..............................................................................
bool TdlgSceneProps::Execute(const IOlxObject* Sender, const IOlxObject* Data,
  TActionQueue *)
{
  if (Sender == tbX) {
    teX->SetText(tbX->GetValue());
  }
  else if (Sender == tbY) {
    teY->SetText(tbY->GetValue());
  }
  else if (Sender == tbZ) {
    teZ->SetText(tbZ->GetValue());
  }
  else if (Sender == tbR) {
    teR->SetText(tbR->GetValue());
  }
  else if (Sender == cbLights) {
    UpdateLight(FLightModel.GetLight(FCurrentLight));
    int i = cbLights->FindString(cbLights->GetValue());
    if (i >= 0) {
      FCurrentLight = i;
      InitLight(FLightModel.GetLight(FCurrentLight));
    }
  }
  else if (Sender == scSCO) {
    if (scSCO->GetValue() > 90) {
      cbUniform->SetValue(true);
    }
    else {
      cbUniform->SetValue(false);
    }
  }
  else if (Sender == tbEditFont) {
    int sel = cbFonts->GetSelection();
    if (sel == -1) {
      return false;
    }
    TGXApp::GetInstance().GetRenderer().GetScene().ShowFontDialog(
      (TGlFont*)cbFonts->GetObject(sel));
  }
  return true;
}
//..............................................................................
void TdlgSceneProps::InitLightModel(TGlLightModel& GlLM) {
  cbLocalV->SetValue(GlLM.IsLocalViewer());
  cbTwoSide->SetValue(GlLM.IsTwoSides());
  cbSmooth->SetValue(GlLM.IsSmoothShade());
  tcAmbLM->SetColour(GlLM.GetAmbientColor());
  tcBgClr->SetColour(GlLM.GetClearColor());
}
//..............................................................................
void TdlgSceneProps::UpdateLightModel(TGlLightModel& GlLM) {
  GlLM.SetLocalViewer(cbLocalV->GetValue());
  GlLM.SetTwoSides(cbTwoSide->GetValue());
  GlLM.SetSmoothShade(cbSmooth->GetValue());
  GlLM.SetAmbientColor(tcAmbLM->GetColour());
  GlLM.SetClearColor(tcBgClr->GetColour());
}
//..............................................................................
void TdlgSceneProps::InitLight(TGlLight& L) {
  tbX->SetValue((int)L.GetPosition()[0]);  teX->SetText((int)L.GetPosition()[0]);
  tbY->SetValue((int)L.GetPosition()[1]);  teY->SetText((int)L.GetPosition()[1]);
  tbZ->SetValue((int)L.GetPosition()[2]);  teZ->SetText((int)L.GetPosition()[2]);
  tbR->SetValue((int)L.GetPosition()[3]);  teR->SetText((int)L.GetPosition()[3]);

  teAmb->SetColour(L.GetAmbient());
  scAmbA->SetValue((int)L.GetAmbient()[3] * 100);
  teDiff->SetColour(L.GetDiffuse());
  scDiffA->SetValue(L.GetDiffuse()[3] * 100);
  teSpec->SetColour(L.GetSpecular());
  scSpecA->SetValue((int)L.GetSpecular()[3] * 100);
  teAA->SetText(L.GetAttenuation()[2]);
  teAB->SetText(L.GetAttenuation()[1]);
  teAC->SetText(L.GetAttenuation()[0]);
  cbEnabled->SetValue(L.IsEnabled());
  scSExp->SetValue(L.GetSpotExponent());
  scSCO->SetValue(L.GetSpotCutoff());
  cbUniform->SetValue(L.GetSpotCutoff() == 180);
  teSCX->SetText(L.GetSpotDirection()[0]);
  teSCY->SetText(L.GetSpotDirection()[1]);
  teSCZ->SetText(L.GetSpotDirection()[2]);
}
//..............................................................................
void TdlgSceneProps::UpdateLight(TGlLight& L) {
  L.SetPosition(TGlOption(tbX->GetValue(), tbY->GetValue(),
    tbZ->GetValue(), tbR->GetValue()));
  L.SetAmbient(
    teAmb->GetColour().GetRGB() | (uint32_t)((256 * scAmbA->GetValue() / 100) << 24));
  L.SetDiffuse(
    teDiff->GetColour().GetRGB() | (uint32_t)((256 * scDiffA->GetValue() / 100) << 24));
  L.SetSpecular(
    teSpec->GetColour().GetRGB() | (uint32_t)((256 * scSpecA->GetValue() / 100) << 24));
  L.SetAttenuation(TGlOption(teAC->GetText().ToDouble(),
    teAB->GetText().ToDouble(), teAA->GetText().ToDouble()));
  L.SetEnabled(cbEnabled->GetValue());
  L.SetSpotExponent(scSExp->GetValue());
  L.SetSpotCutoff(cbUniform->GetValue() ? 180 : scSCO->GetValue());
  L.SetSpotDirection(TGlOption(teSCX->GetText().ToDouble(),
    teSCY->GetText().ToDouble(), teSCZ->GetText().ToDouble()));
}
//..............................................................................
void TdlgSceneProps::OnApply(wxCommandEvent& event)  {
  UpdateLight(FLightModel.GetLight(FCurrentLight));
  UpdateLightModel(FLightModel);
  TGXApp::GetInstance().GetRenderer().LightModel = FLightModel;
  TGXApp::GetInstance().GetRenderer().InitLights();
  TGXApp::GetInstance().Draw();
}
//..............................................................................
void TdlgSceneProps::OnCancel(wxCommandEvent& event)  {
  TGXApp::GetInstance().GetRenderer().LightModel = FOriginalModel;
  TGXApp::GetInstance().GetRenderer().InitLights();
  EndModal(wxID_OK);
}
//..............................................................................
void TdlgSceneProps::OnOK(wxCommandEvent& event)  {
  OnApply(event);
  EndModal(wxID_OK);
}
//..............................................................................
void TdlgSceneProps::OnOpen(wxCommandEvent& event)  {
  olxstr FN = Parent->PickFile("Load scene parameters",
    "Scene parameters|*.glsp", Parent->GetScenesFolder(), EmptyString(), true);
  if( !FN.IsEmpty() )  {
    LoadFromFile(FLightModel, FN);
    Parent->SetScenesFolder(TEFile::ExtractFilePath(FN));
    InitLight(FLightModel.GetLight(FCurrentLight));
    InitLightModel(FLightModel);
  }
}
//..............................................................................
void TdlgSceneProps::OnSave(wxCommandEvent& event)  {
  olxstr FN = Parent->PickFile("Save scene parameters",
  "Scene parameters|*.glsp", Parent->GetScenesFolder(), EmptyString(), false);
  if( !FN.IsEmpty() )  {
    UpdateLight(FLightModel.GetLight(FCurrentLight));
    Parent->SetScenesFolder(TEFile::ExtractFilePath(FN));
    UpdateLightModel(FLightModel);
    SaveToFile(FLightModel, FN);
  }
}
//..............................................................................
void TdlgSceneProps::LoadFromFile(TGlLightModel &FLM, const olxstr &FN) {
  TDataFile F;
  F.LoadFromXLFile(FN, 0);
  TGlRenderer &gr = TGXApp::GetInstance().GetRenderer();
  gr.GetScene().FromDataItem(F.Root());
  FLightModel = gr.LightModel;
}
//..............................................................................
void TdlgSceneProps::SaveToFile(TGlLightModel &FLM, const olxstr &FN) {
  TDataFile DF;
  TGlRenderer &gr = TGXApp::GetInstance().GetRenderer();
  gr.GetScene().ToDataItem(DF.Root());
  try { DF.SaveToXLFile(FN); }
  catch (...) {
    TBasicApp::NewLogEntry(logError) << "Failed to save scene parameters!";
  }
}
//..............................................................................

#endif // Harmony scene-parameters dialog / original wx implementation
