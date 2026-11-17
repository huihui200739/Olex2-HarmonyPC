/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "dgrad.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {constexpr int ResizeEvent=1;}
struct TdlgGradient::Alive {TdlgGradient* owner=nullptr;};
TdlgGradient::TdlgGradient(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,std::shared_ptr<GradientStore> store,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,"Gradient","TdlgGradient",DialogGeometry{Point{0,0},Size{560,500},true},std::move(positions)),alive_(std::make_shared<Alive>()),store_(std::move(store)){
  if(!store_)throw std::invalid_argument("Original background store is required");
  const auto initial=store_->Load();alive_->owner=this;std::weak_ptr<Alive> weak=alive_;
  auto retain=[&](const std::shared_ptr<Widget>& value){Retain(value);};
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());retain(scroll_);
  column_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());retain(column_);column_->SetNumber(Property::RowSpacing,8);
  description_=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(description_);description_->SetText(TextProperty::Content,"Gradient colour of the background of the structure window");description_->SetHeight(56);
  const std::array<const char*,4> names{{"Bottom left","Bottom right","Top right","Top Left"}};
  const std::array<size_t,4> order{{3,2,0,1}};
  for(size_t r=0;r<2;++r){rows_[r]=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());retain(rows_[r]);rows_[r]->SetNumber(Property::RowSpacing,10);rows_[r]->SetHeight(88);}
  for(size_t position=0;position<order.size();++position){
    const auto i=order[position];pairs_[i]=std::make_shared<Widget>(nodes,NodeKind::Column,rows_[position/2].get());retain(pairs_[i]);pairs_[i]->SetHeight(84);
    labels_[i]=std::make_shared<Widget>(nodes,NodeKind::Text,pairs_[i].get());retain(labels_[i]);labels_[i]->SetText(TextProperty::Content,names[i]);labels_[i]->SetHeight(32);
    colors_[i]=std::make_shared<TColorCtrl>(nodes,pairs_[i].get(),-1,initial[i]);retain(colors_[i]);colors_[i]->SetValue(initial[i]);colors_[i]->SetHeight(44);colors_[i]->SetText(TextProperty::Accessibility,std::string("Gradient ")+names[i]);
  }
  status_=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(status_);status_->SetText(TextProperty::Accessibility,"Gradient save status");status_->SetHeight(80);
  buttons_=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());retain(buttons_);buttons_->SetNumber(Property::RowSpacing,8);buttons_->SetHeight(44);
  for(int action=0;action<2;++action){auto button=std::make_shared<EventWidget>(nodes,NodeKind::Button,buttons_.get(),std::vector<EventKind>{EventKind::Click},[weak,action](const NodeEvent&){auto life=weak.lock();if(!life||!life->owner)return;if(action==0)life->owner->OnOK();else life->owner->OnCancel();});retain(button);button->SetText(TextProperty::Content,action==0?"OK":"Cancel");button->SetHeight(40);button->SetNumber(Property::Focusable,1);actionButtons_.push_back(button);}
  OnResize.Add(this,ResizeEvent,msiExecute);Layout();
}
TdlgGradient::~TdlgGradient(){alive_->owner=nullptr;OnResize.Remove(this);}
void TdlgGradient::Layout(){
  const int width=std::max(120,GetWidth()-24),half=std::max(48,(width-10)/2);scroll_->SetWidth(width);scroll_->SetHeight(std::max(40,GetHeight()-136));column_->SetWidth(width);column_->SetHeight(420);
  description_->SetWidth(width);status_->SetWidth(width);buttons_->SetWidth(width);
  for(auto& row:rows_)row->SetWidth(width);
  for(size_t i=0;i<4;++i){pairs_[i]->SetWidth(half);labels_[i]->SetWidth(half);colors_[i]->SetWidth(half);}
  for(auto& button:actionButtons_)button->SetWidth(std::max(24,(width-8)/2));
}
bool TdlgGradient::Dispatch(int event,short,const IOlxObject*,const IOlxObject*,TActionQueue*){if(event==ResizeEvent){Layout();return true;}return false;}
GradientColors TdlgGradient::GetDraft()const{GradientColors result;for(size_t i=0;i<4;++i)result[i]=colors_[i]->GetValue();return result;}
void TdlgGradient::SetDraft(const GradientColors& value){if(IsClosing()||applied_)throw std::logic_error("Gradient is already applied or closing");for(size_t i=0;i<4;++i)colors_[i]->SetValue(value[i]);}
int TdlgGradient::PackedCorner(size_t i)const{uint32_t v=ToOriginalRgba(colors_[i]->GetValue());int result;static_assert(sizeof(result)==sizeof(v));std::memcpy(&result,&v,sizeof(v));return result;}
void TdlgGradient::SetPackedCorner(size_t i,int value){if(IsClosing()||applied_)throw std::logic_error("Gradient is already applied or closing");colors_[i]->SetValue(FromOriginalRgba(static_cast<uint32_t>(value)));}
void TdlgGradient::CloseEditors(){auto editors=colors_;for(auto& color:editors)color->ClosePopup();}
bool TdlgGradient::OnOK(){
  if(IsClosing())return false;
  if(!applied_){
    auto life=alive_;auto store=store_;
    try{CloseEditors();if(life->owner!=this)return false;const auto draft=GetDraft();store->Save(draft);}
    catch(const GradientSaveError& e){if(life->owner!=this)return false;uncertain_=uncertain_||e.BackgroundMayHaveChanged();saveError_=uncertain_?"Could not apply all background colors. Some colors may have changed; your draft is kept. Retry before closing.":"Could not apply background colors; previous colors were restored. Your draft is kept; retry or cancel.";status_->SetText(TextProperty::Content,saveError_);return false;}
    catch(...){if(life->owner!=this)return false;saveError_="Could not apply background colors. Your draft is kept; retry or cancel.";status_->SetText(TextProperty::Content,saveError_);return false;}
    if(life->owner!=this)return true;
    applied_=true;uncertain_=false;saveError_.clear();status_->SetText(TextProperty::Content,"Colors applied; waiting for this window to close.");
  }
  EndModal(1);return true;
}
void TdlgGradient::OnCancel(){if(IsClosing()){RetryClose();return;}auto life=alive_;CloseEditors();if(life->owner==this)EndModal(applied_?1:DialogCancelled);}

#else
#include "dgrad.h"
#include "wx/colordlg.h"
#include "glbackground.h"
#include "gxapp.h"

TdlgGradient::TdlgGradient(TMainFrame *ParentFrame):
  TDialog(ParentFrame, wxT("Gradient"), EsdlClassName(TdlgGradient).u_str())
{
  AActionHandler::SetToDelete(false);
  Bind(wxEVT_BUTTON, &TdlgGradient::OnOK, this, wxID_OK);
  short Border = 3;

  wxStaticText *stcA = new wxStaticText(this, -1, wxT("Bottom left"), wxDefaultPosition);
  tcA = new TTextEdit(this);  tcA->SetReadOnly(true);  tcA->OnClick.Add(this);
  wxStaticText *stcB = new wxStaticText(this, -1, wxT("Bottom right"), wxDefaultPosition);
  tcB = new TTextEdit(this);  tcB->SetReadOnly(true);  tcB->OnClick.Add(this);
  wxStaticText *stcC = new wxStaticText(this, -1, wxT("Top right"), wxDefaultPosition);
  tcC = new TTextEdit(this);  tcC->SetReadOnly(true);  tcC->OnClick.Add(this);
  wxStaticText *stcD = new wxStaticText(this, -1, wxT("Top Left"), wxDefaultPosition);
  tcD = new TTextEdit(this);  tcD->SetReadOnly(true);  tcD->OnClick.Add(this);

  wxFlexGridSizer *GridSizer = new wxFlexGridSizer(2, 4, Border, Border);
  GridSizer->Add( stcD, 0, wxALIGN_CENTER_VERTICAL | wxRight, 2 );
  GridSizer->Add( tcD, 1, wxEXPAND | wxALL, 0 );
  GridSizer->Add( stcC, 0, wxALIGN_CENTER_VERTICAL | wxRight, 2 );
  GridSizer->Add( tcC, 1, wxEXPAND | wxALL, 0 );

  GridSizer->Add( stcA, 0, wxALIGN_CENTER_VERTICAL | wxRight, 2 );
  GridSizer->Add( tcA, 1, wxEXPAND | wxALL, 0 );
  GridSizer->Add( stcB, 0, wxALIGN_CENTER_VERTICAL | wxRight, 2 );
  GridSizer->Add( tcB, 1, wxEXPAND | wxALL, 0 );

  GridSizer->AddGrowableCol(1);
  GridSizer->AddGrowableCol(3);

  wxBoxSizer *ButtonsSizer = new wxBoxSizer( wxHORIZONTAL );
  ButtonsSizer->Add( new wxButton( this, wxID_OK, wxT("OK") ), 0, wxALL, Border);
  ButtonsSizer->Add( new wxButton( this, wxID_CANCEL, wxT("Cancel") ), 0, wxALL, Border);
//  ButtonsSizer->Add( new wxButton( this, wxID_HELP, wxT("Help") ),     0, wxALL, Border );

  wxBoxSizer *TopSiser = new wxBoxSizer( wxVERTICAL );
  TopSiser->Add(new wxStaticText(this, -1, wxT("Gradient colour of the background of the structure window"), wxDefaultPosition), 0, wxEXPAND | wxALL, 10);
  TopSiser->Add(GridSizer, 0, wxEXPAND | wxALL, 5);
  TopSiser->Add(ButtonsSizer, 0, wxALL, 10);

  SetSizer( TopSiser );      // use the sizer for layout
  TopSiser->SetSizeHints( this );   // set size hints to honour minimum size

  Center();
  Init();
}
//..............................................................................
TdlgGradient::~TdlgGradient()  {
  tcA->OnClick.Clear();
  tcB->OnClick.Clear();
  tcC->OnClick.Clear();
  tcD->OnClick.Clear();
}
//..............................................................................
bool TdlgGradient::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Sender != 0 && Sender->Is<TTextEdit>()) {
    wxColourDialog *CD = new wxColourDialog(this);
    wxColor wc = dynamic_cast<const TTextEdit *>(Sender)->GetBackgroundColour();
    CD->GetColourData().SetColour(wc);
    if (CD->ShowModal() == wxID_OK) {
      wc = CD->GetColourData().GetColour();
      dynamic_cast<TTextEdit *>(const_cast<IOlxObject *>(Sender))->WI
        .SetColor(OLX_RGB(wc.Red(), wc.Green(), wc.Blue()));
    }
    delete CD;
  }
  return true;
}
//..............................................................................
void TdlgGradient::Init()  {
  tcA->WI.SetColor(TGXApp::GetInstance().GetRenderer().Background()->LT().GetRGB());
  tcB->WI.SetColor(TGXApp::GetInstance().GetRenderer().Background()->RT().GetRGB());
  tcC->WI.SetColor(TGXApp::GetInstance().GetRenderer().Background()->RB().GetRGB());
  tcD->WI.SetColor(TGXApp::GetInstance().GetRenderer().Background()->LB().GetRGB());
}
//..............................................................................
void TdlgGradient::OnOK(wxCommandEvent& event)  {
  TGlOption opt;
  opt = tcA->WI.GetColor();  TGXApp::GetInstance().GetRenderer().Background()->LT(opt);
  opt = tcB->WI.GetColor();  TGXApp::GetInstance().GetRenderer().Background()->RT(opt);
  opt = tcC->WI.GetColor();  TGXApp::GetInstance().GetRenderer().Background()->RB(opt);
  opt = tcD->WI.GetColor();  TGXApp::GetInstance().GetRenderer().Background()->LB(opt);
  EndModal(wxID_OK);
}
//..............................................................................

#endif // native display dialog / original wx
