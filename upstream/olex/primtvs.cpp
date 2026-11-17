/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "primtvs.h"
#include "eutf8.h"
#include "native_text_positions.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {
constexpr int ResizeEvent=1;
std::string Text(const olxstr& value){TextUtf16Offset(value,value.Length());auto bytes=TUtf8::Encode(value);return {bytes.c_str(),bytes.Length()};}
}
struct TdlgPrimitive::Alive {TdlgPrimitive* owner=nullptr;};
TdlgPrimitive::TdlgPrimitive(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,const PrimitiveDisplaySnapshot& snapshot,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,"Primitives","dlgPrimitives",DialogGeometry{Point{0,0},Size{500,500},true},std::move(positions)),alive_(std::make_shared<Alive>()){
  if(snapshot.labels.size()>32)throw std::length_error("The original primitive mask supports at most 32 entries");
  for(const auto& label:snapshot.labels)Text(label);
  alive_->owner=this;std::weak_ptr<Alive> weak=alive_;auto retain=[&](const std::shared_ptr<Widget>& value){Retain(value);};
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());retain(scroll_);column_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());retain(column_);column_->SetNumber(Property::RowSpacing,8);
  if(snapshot.hasApplyScope){
    scopeLabel_=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(scopeLabel_);scopeLabel_->SetText(TextProperty::Content,"Apply to:");scopeLabel_->SetHeight(28);
    cbApplyTo=std::make_shared<TChoice>(nodes,column_.get());retain(cbApplyTo);cbApplyTo->SetHeight(40);cbApplyTo->Widget::SetText(TextProperty::Accessibility,"Primitive apply scope");
    if(snapshot.currentLevel==0)cbApplyTo->AddObject("Atom Type");
    if(snapshot.currentLevel<=1)cbApplyTo->AddObject("Atom Name");
    cbApplyTo->AddObject("Individual Atom/Bond");cbApplyTo->SetSelection(0);
  }
  for(size_t i=0;i<snapshot.labels.size();++i){auto box=std::make_shared<TCheckBox>(nodes,column_.get(),-1,snapshot.labels[i]);retain(box);box->SetHeight(38);box->SetText(TextProperty::Accessibility,"Primitive "+Text(snapshot.labels[i]));box->SetChecked((snapshot.mask&(uint32_t{1}<<i))!=0);boxes_.push_back(box);}
  status_=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(status_);status_->SetHeight(48);status_->SetText(TextProperty::Accessibility,"Primitive selection status");
  buttons_=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());retain(buttons_);buttons_->SetNumber(Property::RowSpacing,8);buttons_->SetHeight(44);
  for(int action=0;action<3;++action){auto button=std::make_shared<EventWidget>(nodes,NodeKind::Button,buttons_.get(),std::vector<EventKind>{EventKind::Click},[weak,action](const NodeEvent&){auto life=weak.lock();if(!life||!life->owner)return;if(action==0)life->owner->OnOK();else if(action==1)life->owner->OnCancel();else life->owner->ToggleHelp();});retain(button);button->SetText(TextProperty::Content,action==0?"OK":action==1?"Cancel":"Help");button->SetHeight(40);button->SetNumber(Property::Focusable,1);actionButtons_.push_back(button);}
  help_=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(help_);help_->SetText(TextProperty::Content,"Choose the displayed primitive types and, when available, the application scope. OK returns these choices to the caller; this window does not change the graphics object itself. Cancel returns no selection.");help_->SetHeight(120);help_->Hide();
  OnResize.Add(this,ResizeEvent,msiExecute);Layout();
}
TdlgPrimitive::~TdlgPrimitive(){alive_->owner=nullptr;OnResize.Remove(this);}
void TdlgPrimitive::Layout(){
  const int width=std::max(120,GetWidth()-24);scroll_->SetWidth(width);scroll_->SetHeight(std::max(40,GetHeight()-88));column_->SetWidth(width);column_->SetHeight(static_cast<int>(boxes_.size())*46+(cbApplyTo?84:0)+116+(helpVisible_?128:0));
  if(cbApplyTo){cbApplyTo->SetWidth(width);scopeLabel_->SetWidth(width);}
  for(auto& box:boxes_)box->SetWidth(width);status_->SetWidth(width);buttons_->SetWidth(width);help_->SetWidth(width);
  for(auto& button:actionButtons_)button->SetWidth(std::max(24,(width-16)/3));
}
void TdlgPrimitive::ToggleHelp(){helpVisible_=!helpVisible_;help_->Show(helpVisible_);Layout();}
bool TdlgPrimitive::Dispatch(int event,short,const IOlxObject*,const IOlxObject*,TActionQueue*){if(event==ResizeEvent){Layout();return true;}return false;}
uint32_t TdlgPrimitive::DraftMask()const{uint32_t result=0;for(size_t i=0;i<boxes_.size();++i)if(boxes_[i]->IsChecked())result|=uint32_t{1}<<i;return result;}
int TdlgPrimitive::ScopeSelection()const{return cbApplyTo?cbApplyTo->GetSelection():-1;}
size_t TdlgPrimitive::ScopeChoiceCount()const{return cbApplyTo?cbApplyTo->GetCount():0;}
void TdlgPrimitive::SetDraft(uint32_t mask,int selection){
  if(IsClosing()||accepted_)throw std::logic_error("Primitive choices are already accepted or closing");
  if(cbApplyTo){if(selection<0||static_cast<size_t>(selection)>=cbApplyTo->GetCount())throw std::invalid_argument("Select an available primitive application scope");}
  else if(selection!=-1)throw std::invalid_argument("This graphics object has no primitive application scope");
  if(boxes_.size()<32&&(mask>>boxes_.size())!=0)throw std::invalid_argument("Primitive mask contains bits without displayed entries");
  for(size_t i=0;i<boxes_.size();++i)boxes_[i]->SetChecked((mask&(uint32_t{1}<<i))!=0);
  if(cbApplyTo)cbApplyTo->SetSelection(selection);
}
bool TdlgPrimitive::OnOK(){
  if(IsClosing())return false;
  if(!accepted_){
    const int selection=ScopeSelection();if(cbApplyTo&&(selection<0||static_cast<size_t>(selection)>=cbApplyTo->GetCount())){status_->SetText(TextProperty::Content,"Select an application scope before confirming.");return false;}
    if(cbApplyTo)cbApplyTo->Backend().CloseOptions(cbApplyTo->Handle());
    const auto bits=DraftMask();std::memcpy(&Mask,&bits,sizeof(bits));Level=cbApplyTo?static_cast<short>(selection+(3-static_cast<int>(cbApplyTo->GetCount()))):short(-1);accepted_=true;
  }
  EndModal(1);return true;
}
void TdlgPrimitive::OnCancel(){if(IsClosing()){RetryClose();return;}if(cbApplyTo)cbApplyTo->Backend().CloseOptions(cbApplyTo->Handle());EndModal(accepted_?1:DialogCancelled);}

#else
#include "primtvs.h"
#include "styles.h"
#include "xbond.h"
#include "xatom.h"

//..............................................................................
TdlgPrimitive::TdlgPrimitive(TMainFrame *P, AGDrawObject& object) :
  TDialog(P, wxT("Primitives"), wxT("dlgPrimitives")),
  Object(object), cbApplyTo(NULL), Mask(-1), Level(-1)
{
  wxBoxSizer *TopSizer = new wxBoxSizer(wxVERTICAL);
  const int border = 1;
  if ((Object.Is<TXAtom>() || Object.Is<TXBond>())) {
    const uint16_t current_level =
      TXAtom::LegendLevel(Object.GetPrimitives().GetName());
    wxArrayString choices;
    if( current_level == 0 )
      choices.Add(wxT("Atom Type"));
    if( current_level <= 1 )
      choices.Add(wxT("Atom Name"));
    choices.Add(wxT("Individual Atom/Bond"));
    cbApplyTo = new wxComboBox(this, -1,
      choices[0], wxDefaultPosition, wxDefaultSize, choices, wxCB_READONLY);
    cbApplyTo->SetSelection(0);
    wxBoxSizer* Sizer0 = new wxBoxSizer(wxHORIZONTAL);
    Sizer0->Add(new wxStaticText(this, -1, wxT("Apply to: ")), 1,
      wxEXPAND|wxALL, border);
    Sizer0->Add(cbApplyTo, 1, wxFIXED_MINSIZE|wxALL, border);
    TopSizer->Add(Sizer0, 1, wxEXPAND|wxALL, border);
    TopSizer->Add(-1, 10);
  }
  TStrList L;
  Object.ListPrimitives(L);
  int mask = Object.GetPrimitives().GetStyle().GetParam(
    Object.GetPrimitiveMaskName(), "0").ToInt();
  for (size_t i=0; i < L.Count(); i++) {
    wxCheckBox* Box = Boxes.Add(
      new wxCheckBox(this, -1, L[i].u_str(), wxDefaultPosition));
    TopSizer->Add(Box, 0, wxALL, border);
    Box->SetValue((mask & (1 << i)) != 0);
  }
  wxBoxSizer *ButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  ButtonsSizer->Add(
    new wxButton(this, wxID_OK, wxT("OK")), 1, wxALL, 1);
  ButtonsSizer->Add(
    new wxButton(this, wxID_CANCEL, wxT("Cancel")), 1, wxALL, 1);
  ButtonsSizer->Add(
    new wxButton(this, wxID_HELP, wxT("Help")), 1, wxALL, 1);
  TopSizer->Add(-1, 10);
  TopSizer->Add(ButtonsSizer, 0, wxALL, border);
  TopSizer->SetSizeHints(this);   // set size hints to honour minimum size
  SetSizer(TopSizer);
  Center();
  Bind(wxEVT_BUTTON, &TdlgPrimitive::OnOK, this, wxID_OK);
}
//..............................................................................
void TdlgPrimitive::OnOK(wxCommandEvent& event) {
  Mask = 0;
  for (size_t i = 0; i < Boxes.Count(); i++) {
    if (Boxes[i]->GetValue()) {
      Mask |= (1 << i);
    }
  }
  if (cbApplyTo != 0) {
    Level = cbApplyTo->GetSelection() + (3 - cbApplyTo->GetCount());
  }
  EndModal(wxID_OK);
}
//..............................................................................

#endif // native display dialog / original wx
