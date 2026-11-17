#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "matprop.h"
#include "native_widget.h"
#include "native_dialog.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {
enum { MatResize=1,MatCopy,MatPaste,MatCancel,MatOK };
const char* FlagNames[] = {"Ambient front","Ambient back","Diffuse front","Diffuse back","Emission front","Emission back","Specular front","Specular back","Shininess front","Shininess back"};
const char* ColorNames[] = {"Ambient front color","Ambient back color","Diffuse front color","Diffuse back color","Emission front color","Emission back color","Specular front color","Specular back color"};
using FlagSetter=void(glObj::TGlMaterial::*)(bool);
const FlagSetter FlagSetters[] = {&glObj::TGlMaterial::SetAmbientF,&glObj::TGlMaterial::SetAmbientB,&glObj::TGlMaterial::SetDiffuseF,&glObj::TGlMaterial::SetDiffuseB,&glObj::TGlMaterial::SetEmissionF,&glObj::TGlMaterial::SetEmissionB,&glObj::TGlMaterial::SetSpecularF,&glObj::TGlMaterial::SetSpecularB,&glObj::TGlMaterial::SetShininessF,&glObj::TGlMaterial::SetShininessB};
glObj::TGlOption glObj::TGlMaterial::* ColorMembers[] = {&glObj::TGlMaterial::AmbientF,&glObj::TGlMaterial::AmbientB,&glObj::TGlMaterial::DiffuseF,&glObj::TGlMaterial::DiffuseB,&glObj::TGlMaterial::EmissionF,&glObj::TGlMaterial::EmissionB,&glObj::TGlMaterial::SpecularF,&glObj::TGlMaterial::SpecularB};
Color ToPickerColor(const glObj::TGlOption& value) {
  auto channel=[](float v){return static_cast<uint8_t>(std::lround(std::clamp(v,0.0f,1.0f)*255.0f));};
  return Color{channel(value[0]),channel(value[1]),channel(value[2]),channel(value[3])};
}
glObj::TGlOption FromPickerColor(const Color& value) {
  return glObj::TGlOption(value.red/255.0f,value.green/255.0f,value.blue/255.0f,value.alpha/255.0f);
}
std::string Utf8(const olxstr& value) { auto encoded=TUtf8::Encode(value);return std::string(encoded.c_str(),encoded.Length()); }
}
struct TdlgMatProp::Alive { TdlgMatProp* owner=nullptr; };
glObj::TGlMaterial& TdlgMatProp::MaterialCopy_() { static glObj::TGlMaterial material;return material; }
TdlgMatProp::TdlgMatProp(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,
  std::shared_ptr<UiScheduler> scheduler,Widget* parent,const glObj::TGlMaterial& initial,
  std::function<void(const glObj::TGlMaterial&)> apply,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,"Material Parameters","dlgMatProp",
    DialogGeometry{Point{0,0},Size{680,720},true},std::move(positions)),alive_(std::make_shared<Alive>()),material_(initial),apply_(std::move(apply)) {
  alive_->owner=this;BuildControls(nodes);WriteControls(material_);Layout();
}
TdlgMatProp::~TdlgMatProp() {
  alive_->owner=nullptr;OnResize.Remove(this);
  for(auto& button:buttons_) if(button) button->OnClick.Clear();
}
void TdlgMatProp::BuildControls(NodeBackend& nodes) {
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());Retain(scroll_);
  content_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());Retain(content_);content_->SetNumber(Property::RowSpacing,5);
  auto row=[&](const olxstr& title) {
    auto value=std::make_shared<Widget>(nodes,NodeKind::Row,content_.get());Retain(value);value->SetNumber(Property::RowSpacing,8);value->SetHeight(42);rows_.push_back(value);
    auto label=std::make_shared<Widget>(nodes,NodeKind::Text,value.get());Retain(label);label->SetText(TextProperty::Content,Utf8(title));label->SetWidth(190);label->SetHeight(36);
    return value;
  };
  for(size_t i=0;i<flags_.size();++i) {
    auto parentRow=row(FlagNames[i]);
    flags_[i]=std::make_shared<TCheckBox>(nodes,parentRow.get(),-1,"");Retain(flags_[i]);flags_[i]->SetWidth(250);flags_[i]->SetHeight(38);
  }
  for(size_t i=0;i<colors_.size();++i) {
    auto parentRow=row(ColorNames[i]);
    colors_[i]=std::make_shared<TColorCtrl>(nodes,parentRow.get());Retain(colors_[i]);colors_[i]->SetWidth(220);colors_[i]->SetHeight(38);
  }
  for(size_t i=0;i<shininess_.size();++i) {
    auto parentRow=row(i==0?"Shininess front value":"Shininess back value");
    shininess_[i]=std::make_shared<TTextEdit>(nodes,parentRow.get(),-1,"0");Retain(shininess_[i]);shininess_[i]->SetWidth(130);shininess_[i]->SetHeight(38);
  }
  auto alphaRow=row("Ambient / diffuse opacity (%)");
  blendAlpha_=std::make_shared<TSpinCtrl>(nodes,alphaRow.get(),-1,"100");Retain(blendAlpha_);blendAlpha_->SetRange(0,100);blendAlpha_->SetWidth(110);blendAlpha_->SetHeight(38);
  transparency_=std::make_shared<TSpinCtrl>(nodes,alphaRow.get(),-1,"100");Retain(transparency_);transparency_->SetRange(0,100);transparency_->SetWidth(110);transparency_->SetHeight(38);
  auto toggleRow=row("Material options");
  flags_[8]->SetCaption(FlagNames[8]);
  transparent_=std::make_shared<TCheckBox>(nodes,toggleRow.get(),-1,"Transparent");Retain(transparent_);transparent_->SetWidth(170);transparent_->SetHeight(38);
  identity_=std::make_shared<TCheckBox>(nodes,toggleRow.get(),-1,"Identity draw");Retain(identity_);identity_->SetWidth(170);identity_->SetHeight(38);
  auto footer=std::make_shared<Widget>(nodes,NodeKind::Row,&Body());Retain(footer);footer_=footer;footer_->SetNumber(Property::RowSpacing,8);footer_->SetHeight(48);
  const char* labels[]={"Copy material","Paste material","Cancel","OK"};const int ids[]={MatCopy,MatPaste,MatCancel,MatOK};
  for(size_t i=0;i<4;++i){auto button=std::make_shared<TButton>(nodes,footer_.get(),-1,labels[i]);Retain(button);button->SetWidth(150);button->SetHeight(42);button->SetNumber(Property::Focusable,1);button->OnClick.Add(this,static_cast<short>(ids[i]),msiExecute);buttons_.push_back(std::move(button));}
  auto statusRow=row("Status");status_=std::make_shared<Widget>(nodes,NodeKind::Text,statusRow.get());Retain(status_);status_->SetText(TextProperty::Accessibility,"Material editor status");status_->SetWidth(520);status_->SetHeight(40);status_->Hide();
  OnResize.Add(this,MatResize,msiExecute);
}
void TdlgMatProp::Layout() {
  if(!scroll_||!content_||!footer_) return;
  const int width=std::max(320,GetWidth()-24);scroll_->SetWidth(width);scroll_->SetHeight(std::max(180,GetHeight()-130));content_->SetWidth(width);footer_->SetWidth(width);
  int contentHeight=0;for(const auto& row:rows_)if(row)contentHeight+=row->GetHeight()+5;content_->SetHeight(std::max(180,contentHeight));
  for(auto& button:buttons_) button->SetWidth(std::max(64,(width-40)/4));
}
void TdlgMatProp::ShowStatus(const olxstr& value) { status_->SetText(TextProperty::Content,Utf8(value));status_->Show(!value.IsEmpty()); }
void TdlgMatProp::WriteControls(const glObj::TGlMaterial& value) {
  const bool enabled[]={value.HasAmbientF(),value.HasAmbientB(),value.HasDiffuseF(),value.HasDiffuseB(),value.HasEmissionF(),value.HasEmissionB(),value.HasSpecularF(),value.HasSpecularB(),value.HasShininessF(),value.HasShininessB()};
  for(size_t i=0;i<flags_.size();++i) flags_[i]->SetChecked(enabled[i]);
  for(size_t i=0;i<colors_.size();++i) colors_[i]->SetValue(ToPickerColor(value.*ColorMembers[i]));
  shininess_[0]->SetText(value.ShininessF);shininess_[1]->SetText(value.ShininessB);
  blendAlpha_->SetValue(static_cast<int>(std::lround(value.AmbientF[3]*100.0f)));
  transparency_->SetValue(static_cast<int>(std::lround(value.DiffuseF[3]*100.0f)));
  transparent_->SetChecked(value.IsTransparent());identity_->SetChecked(value.IsIdentityDraw());
}
glObj::TGlMaterial TdlgMatProp::ReadControls() const {
  glObj::TGlMaterial value=material_;
  for(size_t i=0;i<flags_.size();++i) (value.*FlagSetters[i])(flags_[i]->IsChecked());
  value.SetTransparent(transparent_->IsChecked());value.SetIdentityDraw(identity_->IsChecked());
  for(size_t i=0;i<colors_.size();++i) value.*ColorMembers[i]=FromPickerColor(colors_[i]->GetValue());
  value.AmbientF[3]=blendAlpha_->GetValue()/100.0f;
  value.DiffuseF[3]=value.DiffuseB[3]=transparency_->GetValue()/100.0f;
  value.ShininessF=static_cast<short>(shininess_[0]->GetText().ToInt());
  value.ShininessB=static_cast<short>(shininess_[1]->GetText().ToInt());
  return value;
}
bool TdlgMatProp::Dispatch(int event,short,const IOlxObject*,const IOlxObject*,TActionQueue*) {
  if(event==MatResize){Layout();return true;}
  if(event==MatCopy){material_=ReadControls();MaterialCopy_()=material_;ShowStatus("Material copied.");return true;}
  if(event==MatPaste){SetCurrent(MaterialCopy_());ShowStatus("Material pasted.");return true;}
  if(event==MatCancel){OnCancel();return true;}
  if(event==MatOK){OnOK();return true;}
  return false;
}
void TdlgMatProp::OnOK() {
  try {auto next=ReadControls();if(apply_)apply_(next);material_=next;ShowStatus(EmptyString());EndModal(1);}
  catch(const std::exception& error){ShowStatus(olxstr("Could not apply material: ")<<error.what());}
  catch(...){ShowStatus("Could not apply material: unknown error.");}
}
void TdlgMatProp::OnCancel() {if(IsClosing()){RetryClose();return;}EndModal(DialogCancelled);}
void TdlgMatProp::OnCopy() {material_=ReadControls();MaterialCopy_()=material_;ShowStatus("Material copied.");}
void TdlgMatProp::OnPaste() {SetCurrent(MaterialCopy_());ShowStatus("Material pasted.");}

#else
/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#include "matprop.h"
#include "wx/colordlg.h"
#include "wx/fontdlg.h"
#include "gpcollection.h"
#include "glgroup.h"
#include "xatom.h"
#include "xbond.h"

// material properties dialog
enum {
  ID_COPY = 1,
  ID_PASTE,
  ID_EDITFONT
};
//..............................................................................
TdlgMatProp::TdlgMatProp(TMainFrame *ParentFrame, AGDrawObject& object) :
  TDialog(ParentFrame, wxT("Material Parameters"), wxT("dlgMatProp"))
{
  Object = &object;
  Init();
}
//..............................................................................
TdlgMatProp::TdlgMatProp(TMainFrame *ParentFrame, TGlMaterial& mat) :
  TDialog(ParentFrame, wxT("Material Parameters"), wxT("dlgMatProp"))
{
  Object = 0;
  Materials.AddCopy(mat);
  Init();
}
//..............................................................................
void TdlgMatProp::Init() {
  Bind(wxEVT_BUTTON, &TdlgMatProp::OnOK, this, wxID_OK);
  Bind(wxEVT_BUTTON, &TdlgMatProp::OnCopy, this, ID_COPY);
  Bind(wxEVT_BUTTON, &TdlgMatProp::OnPaste, this, ID_PASTE);
  Bind(wxEVT_BUTTON, &TdlgMatProp::OnEditFont, this, ID_EDITFONT);
  AActionHandler::SetToDelete(false);
  bEditFont = 0;
  cbApplyTo = 0;
  cbBlend = 0;
  scBlend = 0;
  short Border = 3, SpW = 40;
  FCurrentMaterial = 0;
  if (Object != 0 && Object->GetPrimitives().PrimitiveCount() > 1) {
    cbPrimitives = new TChoice(this);
    TGPCollection& gpc = Object->GetPrimitives();
    for (size_t i=0; i < gpc.PrimitiveCount(); i++) {
      TGlPrimitive& GlP = gpc.GetPrimitive(i);
      cbPrimitives->AddObject(GlP.GetName(), &GlP);
      Materials.AddCopy(GlP.GetProperties());
    }
    cbPrimitives->OnChange.Add(this);
    // will thi comment fix the SF svn?
    cbPrimitives->SetSelection(0);
  }
  else {
    cbPrimitives = 0;
    if (Object != 0) {
      if (Object->Is<TGlGroup>()) {
        Materials.AddCopy(((TGlGroup*)Object)->GetGlM());
        cbBlend = new wxCheckBox(this, -1, wxT("Blend"));
        cbBlend->SetValue(((TGlGroup*)Object)->IsBlended());
        scBlend = new TSpinCtrl(this);
        scBlend->WI.SetWidth(SpW);
        scBlend->SetRange(0, 100);
      }
      else if (Object->GetPrimitives().PrimitiveCount() != 0) {
        Materials.AddCopy(
          Object->GetPrimitives().GetPrimitive(0).GetProperties());
      }
    }
  }
  if (Object != 0) {
    if (Object->Is<TXAtom>() || Object->Is<TXBond>()) {
      const uint16_t current_level =
        TXAtom::LegendLevel(Object->GetPrimitives().GetName());
      wxArrayString choices;
      if (current_level == 0)
        choices.Add(wxT("Atom Type"));
      if (current_level <= 1)
        choices.Add(wxT("Atom Name"));
      choices.Add(wxT("Individual Atom/Bond"));
      cbApplyTo = new wxComboBox(this, -1, choices[0], wxDefaultPosition,
        wxDefaultSize, choices, wxCB_READONLY);
      cbApplyTo->SetSelection(0);
    }
  }
  long flags = 0; //wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER;
  cbAmbF = new wxCheckBox(this, -1,
    wxT("Ambient Front"), wxDefaultPosition, wxDefaultSize, flags);
  cbAmbB = new wxCheckBox(this, -1,
    wxT("Ambient Back"), wxDefaultPosition, wxDefaultSize, flags);
  cbDiffF = new wxCheckBox(this, -1,
    wxT("Diffuse Front"), wxDefaultPosition, wxDefaultSize, flags);
  cbDiffB = new wxCheckBox(this, -1,
    wxT("Diffuse Back"), wxDefaultPosition, wxDefaultSize, flags);
  cbEmmF = new wxCheckBox(this, -1,
    wxT("Emission Front"), wxDefaultPosition, wxDefaultSize, flags);
  cbEmmB = new wxCheckBox(this, -1,
    wxT("Emission Back"), wxDefaultPosition, wxDefaultSize, flags);
  cbSpecF = new wxCheckBox(this, -1,
    wxT("Specular Front"), wxDefaultPosition, wxDefaultSize, flags);
  cbSpecB = new wxCheckBox(this, -1,
    wxT("Specular Back"), wxDefaultPosition, wxDefaultSize, flags);
  cbShnF = new wxCheckBox(this, -1,
    wxT("Shininess Front"), wxDefaultPosition, wxDefaultSize, flags);
  cbShnB = new wxCheckBox(this, -1,
    wxT("Shininess Back"), wxDefaultPosition, wxDefaultSize, flags);

  cbTrans = new wxCheckBox(this, -1,
    wxT("Translucent"), wxDefaultPosition, wxDefaultSize);
    scTrans = new TSpinCtrl(this);
    scTrans->SetRange(0, 100);
    scTrans->OnChange.Add(this);
  cbIDraw = new wxCheckBox(this, -1,
    wxT("Identity Draw"), wxDefaultPosition, wxDefaultSize);

  tcAmbF = new TColorCtrl(this);  tcAmbF->SetReadOnly(true);
  tcAmbB = new TColorCtrl(this);  tcAmbB->SetReadOnly(true);
  tcDiffF = new TColorCtrl(this); tcDiffF->SetReadOnly(true);
  tcDiffB = new TColorCtrl(this); tcDiffB->SetReadOnly(true);
  tcEmmF = new TColorCtrl(this);  tcEmmF->SetReadOnly(true);
  tcEmmB = new TColorCtrl(this);  tcEmmB->SetReadOnly(true);
  tcSpecF = new TColorCtrl(this); tcSpecF->SetReadOnly(true);
  tcSpecB = new TColorCtrl(this); tcSpecB->SetReadOnly(true);
  tcShnF = new TTextEdit(this);  tcShnF->SetReadOnly(false);
  tcShnB = new TTextEdit(this);  tcShnB->SetReadOnly(false);

  wxBoxSizer *Sizer0 = new wxBoxSizer(wxHORIZONTAL);
  if (cbPrimitives != NULL || cbApplyTo != NULL) {
    if (cbPrimitives != NULL) {
      Sizer0->Add(new wxStaticText(this, -1,
        wxT("Primitive: ")), 0, wxEXPAND | wxALL, Border);
      Sizer0->Add(cbPrimitives, 0, wxFIXED_MINSIZE | wxALL, Border);
    }
    if (cbApplyTo != NULL) {
      Sizer0->Add(
        new wxStaticText(this, -1, wxT("Apply to: ")), 0, wxEXPAND | wxALL, Border);
      Sizer0->Add(cbApplyTo, 0, wxFIXED_MINSIZE | wxALL, Border);
    }
    bEditFont = new wxButton(this, ID_EDITFONT, wxT("Edit Font"));
    bEditFont->Enable(Object->GetPrimitives().GetPrimitive(0).GetFont() != NULL &&
      !Object->GetPrimitives().GetPrimitive(0).GetFont()->IsVectorFont());
    Sizer0->Add(bEditFont, 1, wxEXPAND | wxALL, Border);
  }
  else if (cbBlend != NULL) {
    Sizer0->Add(new wxStaticText(this, -1,
      wxT("Group color properties: ")), 0, wxEXPAND | wxALL, Border);
    Sizer0->Add(cbBlend, 1, wxEXPAND | wxALL, Border);
    Sizer0->Add(scBlend, 0, wxALL, Border);
  }
  Sizer0->Add(new wxButton(this, ID_COPY, wxT("Copy  Mat.")), 1, wxEXPAND | wxALL, Border);
  Sizer0->Add(new wxButton(this, ID_PASTE, wxT("Paste Mat.")), 1, wxEXPAND | wxALL, Border);

#if !wxCHECK_VERSION(2,9,0)
  wxFlexGridSizer *grid = new wxFlexGridSizer(4, 5);
#else
  wxFlexGridSizer *grid = new wxFlexGridSizer(4, 3, 6);
#endif
  grid->Add(new wxStaticText(this, -1,
    wxT("Destination"), wxDefaultPosition), 0,
    wxALIGN_CENTRE_HORIZONTAL | wxEXPAND | wxALL, Border);
  grid->Add(new wxStaticText(this, -1,
    wxT("Colour"), wxDefaultPosition), 0,
    wxALIGN_CENTRE_HORIZONTAL | wxEXPAND | wxALL, Border);
  grid->Add(new wxStaticText(this, -1,
    wxT("Destination"), wxDefaultPosition), 0,
    wxALIGN_CENTRE_HORIZONTAL | wxEXPAND | wxALL, Border);
  grid->Add(new wxStaticText(this, -1,
    wxT("Colour"), wxDefaultPosition), 0,
    wxALIGN_CENTRE_HORIZONTAL | wxEXPAND | wxALL, Border);

  grid->Add(cbAmbF, 0, wxALL, Border);
  grid->Add(tcAmbF, 0, wxEXPAND | wxALL, Border);
  grid->Add(cbAmbB, 0, wxALL, Border);
  grid->Add(tcAmbB, 0, wxEXPAND | wxALL, Border);

  grid->Add(cbDiffF, 0, wxALL, Border);
  grid->Add(tcDiffF, 0, wxEXPAND | wxALL, Border);
  grid->Add(cbDiffB, 0, wxALL, Border);
  grid->Add(tcDiffB, 0, wxEXPAND | wxALL, Border);

  grid->Add(cbEmmF, 0, wxALL, Border);
  grid->Add(tcEmmF, 0, wxEXPAND | wxALL, Border);
  grid->Add(cbEmmB, 0, wxALL, Border);
  grid->Add(tcEmmB, 0, wxEXPAND | wxALL, Border);

  grid->Add(cbSpecF, 0, wxALL, Border);
  grid->Add(tcSpecF, 0, wxEXPAND | wxALL, Border);
  grid->Add(cbSpecB, 0, wxALL, Border);
  grid->Add(tcSpecB, 0, wxEXPAND | wxALL, Border);

  grid->Add(cbShnF, 0, wxALL, Border);
  grid->Add(tcShnF, 0, wxEXPAND | wxALL, Border);
  grid->Add(cbShnB, 0, wxALL, Border);
  grid->Add(tcShnB, 0, wxEXPAND | wxALL, Border);

  grid->Add(cbTrans, 0, wxALL, Border);
  grid->Add(scTrans, 0, wxEXPAND | wxALL, Border);
  grid->AddSpacer(10);
  grid->Add(cbIDraw, 0, wxALL, Border);

  grid->AddGrowableCol(1);
  grid->AddGrowableCol(3);

  wxBoxSizer *ButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  ButtonsSizer->Add(
    new wxButton(this, wxID_OK, wxT("OK") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_CANCEL, wxT("Cancel") ), 0, wxEXPAND | wxALL, Border);
  ButtonsSizer->Add(
    new wxButton(this, wxID_HELP, wxT("Help") ), 0, wxEXPAND | wxALL, Border);

  wxBoxSizer *TopSiser = new wxBoxSizer(wxVERTICAL);
  if (Sizer0) {
    TopSiser->Add(Sizer0, 0, wxEXPAND | wxALL, 5);
  }
  TopSiser->Add(grid, 0, wxEXPAND | wxALL, 5);
  TopSiser->Add(ButtonsSizer, 0, wxALL, 10);
  // use the sizer for layout
  SetSizer(TopSiser);
  // set size hints to honour minimum size
  TopSiser->SetSizeHints(this);
  Center();
  Init(Materials[0]);
}
//..............................................................................
TdlgMatProp::~TdlgMatProp() {
  if (cbPrimitives != 0) {
    cbPrimitives->OnChange.Clear();
  }
  scTrans->OnChange.Clear();
}
//..............................................................................
bool TdlgMatProp::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Sender == cbPrimitives) {
    Update(Materials[FCurrentMaterial]);
    int i = cbPrimitives->FindString(cbPrimitives->GetValue());
    if (i >= 0) {
      FCurrentMaterial = i;
      Init(Materials[FCurrentMaterial]);
      const TGlPrimitive* glp = dynamic_cast<const TGlPrimitive*>(
        cbPrimitives->GetObject(i));
      bEditFont->Enable(glp->GetFont() != 0 && !glp->GetFont()->IsVectorFont());
    }
  }
  return true;
}
//..............................................................................
void TdlgMatProp::Init(const TGlMaterial &Glm) {
  cbAmbF->SetValue(Glm.HasAmbientF());
  cbAmbB->SetValue(Glm.HasAmbientB());
  if (scBlend != 0) {
    scBlend->SetValue(olx_round(Glm.AmbientF.Data()[3]*100));
  }

  cbDiffF->SetValue(Glm.HasDiffuseF());
  cbDiffB->SetValue(Glm.HasDiffuseB());
  scTrans->SetValue(olx_round(Glm.DiffuseF.Data()[3]*100));

  cbEmmF->SetValue(Glm.HasEmissionF());
  cbEmmB->SetValue(Glm.HasEmissionB());

  cbSpecF->SetValue(Glm.HasSpecularF());
  cbSpecB->SetValue(Glm.HasSpecularB());

  cbShnF->SetValue(Glm.HasShininessF());
  cbShnB->SetValue(Glm.HasShininessB());

  cbTrans->SetValue(Glm.IsTransparent());
  cbIDraw->SetValue(Glm.IsIdentityDraw());

  tcAmbF->SetColour(Glm.AmbientF);
  tcAmbB->SetColour(Glm.AmbientB);

  tcDiffF->SetColour(Glm.DiffuseF);
  tcDiffB->SetColour(Glm.DiffuseB);

  tcEmmF->SetColour(Glm.EmissionF);
  tcEmmB->SetColour(Glm.EmissionB);

  tcSpecF->SetColour(Glm.SpecularF);
  tcSpecB->SetColour(Glm.SpecularB);

  tcShnF->SetValue(olxstr(Glm.ShininessF).u_str());
  tcShnB->SetValue(olxstr(Glm.ShininessB).u_str());
}
//..............................................................................
void TdlgMatProp::Update(TGlMaterial &Glm)  {
  Glm.SetAmbientF(cbAmbF->GetValue());
  Glm.SetAmbientB(cbAmbB->GetValue());

  Glm.SetDiffuseF(cbDiffF->GetValue());
  Glm.SetDiffuseB(cbDiffB->GetValue());

  Glm.SetEmissionF(cbEmmF->GetValue());
  Glm.SetEmissionB(cbEmmB->GetValue());

  Glm.SetSpecularF(cbSpecF->GetValue());
  Glm.SetSpecularB(cbSpecB->GetValue());

  Glm.SetShininessF(cbShnF->GetValue());
  Glm.SetShininessB(cbShnB->GetValue());

  Glm.SetTransparent(cbTrans->GetValue());
  Glm.SetIdentityDraw(cbIDraw->GetValue());

  Glm.AmbientF = tcAmbF->GetColour();
  if (scBlend != 0) {
    Glm.AmbientF[3] = (double)scBlend->GetValue()/100;
  }
  Glm.AmbientB = tcAmbB->GetColour();

  Glm.DiffuseF = tcDiffF->GetColour();
  Glm.DiffuseB = tcDiffB->GetColour();
  Glm.DiffuseB[3] = Glm.DiffuseF[3] = (double)scTrans->GetValue()/100;

  Glm.EmissionF = tcEmmF->GetColour();
  Glm.EmissionB = tcEmmB->GetColour();

  Glm.SpecularF = tcSpecF->GetColour();
  Glm.SpecularB = tcSpecB->GetColour();

  Glm.ShininessF = olxstr(tcShnF->GetValue()).ToInt();
  Glm.ShininessB = olxstr(tcShnB->GetValue()).ToInt();
}
//..............................................................................
void TdlgMatProp::OnOK(wxCommandEvent& event) {
  Update(Materials[FCurrentMaterial]);
  if (Object != NULL) {
    const short cl = TXAtom::LegendLevel(Object->GetPrimitives().GetName());
    TGXApp& app = TGXApp::GetInstance();
    if (Object->Is<TGlGroup>()) {
      ((TGlGroup*)Object)->SetGlM(Materials[0]);
      ((TGlGroup*)Object)->SetBlended(cbBlend->GetValue());
    }
    else if (Object->IsSelected()) {
      TGlGroup& gl = app.GetSelection();
      TGPCollection* ogpc = &Object->GetPrimitives();
      sorted::PointerPointer<TGPCollection> uniqCol;
      const bool is_bond = Object->Is<TXBond>();
      if (cbApplyTo == NULL || cbApplyTo->GetSelection() == 0) {
        for (size_t i = 0; i < gl.Count(); i++) {
          if (olx_type_check(gl[i], *Object)) {
            if (is_bond) {
              app.Individualise(((TXBond&)gl[i]), 3);
            }
            uniqCol.AddUnique(&gl[i].GetPrimitives());
          }
        }
      }
      else  {
        TXAtomPList atoms;
        TXBondPList bonds;
        for (size_t i = 0; i < gl.Count(); i++) {
          if (olx_type_check(gl[i], *Object)) {
            if (gl[i].Is<TXAtom>()) {
              atoms.Add((TXAtom&)gl[i]);
            }
            else if (gl[i].Is<TXBond>()) {
              bonds.Add((TXBond&)gl[i]);
            }
          }
        }
        if (!atoms.IsEmpty()) {
          app.Individualise(atoms, cl+cbApplyTo->GetSelection());
          for (size_t i = 0; i < atoms.Count(); i++) {
            uniqCol.AddUnique(&atoms[i]->GetPrimitives());
          }
        }
        else {
          app.Individualise(bonds, cl + cbApplyTo->GetSelection());
          for (size_t i = 0; i < bonds.Count(); i++) {
            uniqCol.AddUnique(&bonds[i]->GetPrimitives());
          }
        }
      }
      for (size_t i = 0; i < uniqCol.Count(); i++) {
        for (size_t j = 0; j < ogpc->PrimitiveCount(); j++) {
          if (j >= Materials.Count())  // just in case...
            break;
          TGlPrimitive* glp = uniqCol[i]->FindPrimitiveByName(
            ogpc->GetPrimitive(j).GetName());
          if (glp != 0) {
            glp->SetProperties(Materials[j]);
            uniqCol[i]->GetStyle().SetMaterial(glp->GetName(), Materials[j]);
          }
        }
      }
    }
    else {
      /* we have to re-enfoce the primitive mask as the new collection can have
      different size!
      */
      if (Object->Is<TXBond>()) {
        const uint32_t pmask = ((TXBond*)Object)->GetPrimitiveMask();
        app.Individualise(
          *((TXBond*)Object), cl + cbApplyTo->GetSelection(), pmask);
      }
      if (cbApplyTo != 0 && cbApplyTo->GetSelection() != 0) {
        if (Object->Is<TXAtom>()) {
          const uint32_t pmask = ((TXAtom*)Object)->GetPrimitiveMask();
          app.Individualise(
            *((TXAtom*)Object), cl + cbApplyTo->GetSelection(), pmask);
        }
      }
      TGPCollection& gpc = Object->GetPrimitives();
      for (size_t i = 0; i < gpc.PrimitiveCount(); i++) {
        // this should not happens, since the mask is re-enforced...
        if (i >= Materials.Count()) {
          break;
        }
        gpc.GetPrimitive(i).SetProperties(Materials[i]);
        gpc.GetStyle().SetMaterial(gpc.GetPrimitive(i).GetName(), Materials[i]);
      }
    }
    if (Object->Is<TXAtom>()) {
      TGXApp::BondIterator bi = app.GetBonds();
      while (bi.HasNext()) {
        bi.Next().UpdateStyle();
      }
    }
  }
  //  FDrawObject->UpdatePrimitives(24);
  EndModal(wxID_OK);
}
//..............................................................................
void TdlgMatProp::OnCopy(wxCommandEvent& event) {
  Update(Materials[FCurrentMaterial]);
  MaterialCopy_() = Materials[FCurrentMaterial];
}
//..............................................................................
void TdlgMatProp::OnPaste(wxCommandEvent& event) {
//  Materials[FCurrentMaterial] = MaterialCopy;
  Init(MaterialCopy_());
}
//..............................................................................
void TdlgMatProp::OnEditFont(wxCommandEvent& event) {
  TGXApp::GetInstance().GetRenderer().GetScene().ShowFontDialog(
    Object->GetPrimitives().GetPrimitive(FCurrentMaterial).GetFont());
  TGPCollection& gpc = Object->GetPrimitives();
  for (size_t i = 0; i < gpc.ObjectCount(); i++) {
    gpc.GetObject(i).UpdateLabel();
  }
}
//..............................................................................

#endif // Harmony native material dialog / original wx implementation
