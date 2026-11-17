/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "dgenopt.h"
#include "generation_options.h"
#include <algorithm>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {constexpr int ResizeEvent=1,RangeAEvent=2,RangeBEvent=3,RangeCEvent=4;const char* AxisName(size_t i){return i==0?"A":i==1?"B":"C";}}
struct TdlgGenerate::Alive {TdlgGenerate* owner=nullptr;};
TdlgGenerate::TdlgGenerate(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,"Generation options","dlgGenerate",DialogGeometry{Point{0,0},Size{640,340},true},std::move(positions)),alive_(std::make_shared<Alive>()){
  alive_->owner=this;std::weak_ptr<Alive> weak=alive_;auto retain=[&](const std::shared_ptr<Widget>& value){Retain(value);};
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());retain(scroll_);rows_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());retain(rows_);rows_->SetNumber(Property::RowSpacing,8);
  for(size_t i=0;i<axes_.size();++i){auto& a=axes_[i];a.row=std::make_shared<Widget>(nodes,NodeKind::Row,rows_.get());retain(a.row);a.row->SetNumber(Property::RowSpacing,6);a.row->SetHeight(52);
    a.labelFrom=std::make_shared<Widget>(nodes,NodeKind::Text,a.row.get());retain(a.labelFrom);a.labelFrom->SetText(TextProperty::Content,std::string(AxisName(i))+" from");a.labelFrom->SetHeight(42);
    a.from=std::make_shared<TTextEdit>(nodes,a.row.get(),-1,"-1");retain(a.from);a.from->SetHeight(42);a.from->Widget::SetText(TextProperty::Accessibility,std::string(AxisName(i))+" generation lower bound");
    a.labelTo=std::make_shared<Widget>(nodes,NodeKind::Text,a.row.get());retain(a.labelTo);a.labelTo->SetText(TextProperty::Content,"to");a.labelTo->SetHeight(42);
    a.to=std::make_shared<TTextEdit>(nodes,a.row.get(),-1,"1");retain(a.to);a.to->SetHeight(42);a.to->Widget::SetText(TextProperty::Accessibility,std::string(AxisName(i))+" generation upper bound");
    a.range=std::make_shared<TComboBox>(nodes,a.row.get());retain(a.range);a.range->SetHeight(42);a.range->Widget::SetText(TextProperty::Accessibility,std::string(AxisName(i))+" generation range selector");
    for(int n=1;n<=8;++n)a.range->AddObject(olxstr(n));a.range->SetText("2");a.range->OnChange.Add(this,static_cast<short>(RangeAEvent+i),msiExecute);
  }
  status_=std::make_shared<Widget>(nodes,NodeKind::Text,rows_.get());retain(status_);status_->SetText(TextProperty::Accessibility,"Generation options help");status_->SetHeight(42);status_->Hide();
  buttons_=std::make_shared<Widget>(nodes,NodeKind::Row,&Body());retain(buttons_);buttons_->SetNumber(Property::RowSpacing,8);buttons_->SetHeight(46);
  for(int action=0;action<3;++action){auto button=std::make_shared<EventWidget>(nodes,NodeKind::Button,buttons_.get(),std::vector<EventKind>{EventKind::Click},[weak,action](const NodeEvent&){auto life=weak.lock();if(!life||!life->owner)return;if(action==0)life->owner->OnOK();else if(action==1)life->owner->OnCancel();else life->owner->ToggleHelp();});retain(button);button->SetText(TextProperty::Content,action==0?"OK":action==1?"Cancel":"Help");button->SetHeight(40);button->SetNumber(Property::Focusable,1);actionButtons_.push_back(button);}
  OnResize.Add(this,ResizeEvent,msiExecute);Layout();
}
TdlgGenerate::~TdlgGenerate(){alive_->owner=nullptr;OnResize.Remove(this);for(auto& axis:axes_)axis.range->OnChange.Clear();}
void TdlgGenerate::Layout(){const int width=std::max(200,GetWidth()-24);scroll_->SetWidth(width);scroll_->SetHeight(std::max(40,GetHeight()-110));rows_->SetWidth(width);rows_->SetHeight(190);buttons_->SetWidth(width);status_->SetWidth(width);for(auto& a:axes_){a.row->SetWidth(width);a.labelFrom->SetWidth(std::max(42,width/8));a.from->SetWidth(std::max(48,width/4));a.labelTo->SetWidth(32);a.to->SetWidth(std::max(48,width/4));a.range->SetWidth(std::max(64,width/5));}for(auto& b:actionButtons_)b->SetWidth(std::max(32,(width-16)/3));}
void TdlgGenerate::UpdateRange(size_t i){const double value=ParseOriginalGenerationValue(GetRangeText(i),2);SetFromText(i,olxstr(-value/2));SetToText(i,olxstr(value/2));}
bool TdlgGenerate::Dispatch(int event,short,const IOlxObject* sender,const IOlxObject*,TActionQueue*){if(event==ResizeEvent){Layout();return true;}if(event>=RangeAEvent&&event<=RangeCEvent){const size_t i=static_cast<size_t>(event-RangeAEvent);if(sender==axes_[i].range.get())UpdateRange(i);return true;}return false;}
void TdlgGenerate::ToggleHelp(){helpVisible_=!helpVisible_;status_->SetText(TextProperty::Content,helpVisible_?"Each A, B and C row sets a numeric generation interval. Selecting an integer from 1 to 8 centers that interval at zero; the endpoints remain editable.":"");status_->Show(helpVisible_);}
void TdlgGenerate::SetFromText(size_t i,const olxstr& value){axes_.at(i).from->SetText(value);}void TdlgGenerate::SetToText(size_t i,const olxstr& value){axes_.at(i).to->SetText(value);}
olxstr TdlgGenerate::GetFromText(size_t i)const{return axes_.at(i).from->GetText();}olxstr TdlgGenerate::GetToText(size_t i)const{return axes_.at(i).to->GetText();}olxstr TdlgGenerate::GetRangeText(size_t i)const{return axes_.at(i).range->GetValue();}
void TdlgGenerate::OnCancel(){if(IsClosing()){RetryClose();return;}EndModal(DialogCancelled);}
bool TdlgGenerate::OnOK(){if(IsClosing())return false;if(!accepted_){double value=2;value=ParseOriginalGenerationValue(GetFromText(0),value);AFrom=static_cast<float>(value);value=ParseOriginalGenerationValue(GetFromText(1),value);BFrom=static_cast<float>(value);value=ParseOriginalGenerationValue(GetFromText(2),value);CFrom=static_cast<float>(value);value=ParseOriginalGenerationValue(GetToText(0),value);ATo=static_cast<float>(value);value=ParseOriginalGenerationValue(GetToText(1),value);BTo=static_cast<float>(value);value=ParseOriginalGenerationValue(GetToText(2),value);CTo=static_cast<float>(value);accepted_=true;}EndModal(1);return true;}

#else
#include "dgenopt.h"
#include "ctrls/frameext.h"

//..............................................................................
TdlgGenerate::TdlgGenerate(TMainFrame *ParentFrame) :
  TDialog(ParentFrame, wxT("Generation options"), wxT("dlgGenerate"))
{
  AActionHandler::SetToDelete(false);
  Bind(wxEVT_BUTTON, &TdlgGenerate::OnOK, this, wxID_OK);
  AFrom = BFrom = CFrom = -1;
  ATo = BTo = CTo = 1;
  short Border = 2, i;
  TStrList EL;
  for( i=1; i < 9; i++ )    EL.Add(i);

  stAFrom = new wxStaticText(this, -1, wxT("A from"), wxDefaultPosition);
  stBFrom = new wxStaticText(this, -1, wxT("B from"), wxDefaultPosition);
  stCFrom = new wxStaticText(this, -1, wxT("C from"), wxDefaultPosition);
  stATo   = new wxStaticText(this, -1, wxT("to"), wxDefaultPosition);
  stBTo   = new wxStaticText(this, -1, wxT("to"), wxDefaultPosition);
  stCTo   = new wxStaticText(this, -1, wxT("to"), wxDefaultPosition);

  tcAFrom = new wxTextCtrl(this, -1, wxT("-1"), wxDefaultPosition);
  tcBFrom = new wxTextCtrl(this, -1, wxT("-1"), wxDefaultPosition);
  tcCFrom = new wxTextCtrl(this, -1, wxT("-1"), wxDefaultPosition);
  tcATo   = new wxTextCtrl(this, -1, wxT("1"), wxDefaultPosition);
  tcBTo   = new wxTextCtrl(this, -1, wxT("1"), wxDefaultPosition);
  tcCTo   = new wxTextCtrl(this, -1, wxT("1"), wxDefaultPosition);

  cbA = new TComboBox(this);  cbA->SetText("2"); cbA->AddItems(EL);
  cbB = new TComboBox(this);  cbB->SetText("2"); cbB->AddItems(EL);
  cbC = new TComboBox(this);  cbC->SetText("2"); cbC->AddItems(EL);
  cbA->OnChange.Add(this);
  cbB->OnChange.Add(this);
  cbC->OnChange.Add(this);

  wxBoxSizer *TopSizer = new wxBoxSizer(wxVERTICAL );

  wxFlexGridSizer *GridSizer = new wxFlexGridSizer(3, 5, Border, Border);
  GridSizer->Add( stAFrom, 0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcAFrom, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( stATo,   0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcATo, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( cbA, 0, wxEXPAND | wxALL, Border );

  GridSizer->Add( stBFrom, 0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcBFrom, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( stBTo,   0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcBTo, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( cbB, 0, wxEXPAND | wxALL, Border );

  GridSizer->Add( stCFrom, 0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcCFrom, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( stCTo,   0, wxALIGN_CENTER_VERTICAL | wxALL, Border );
  GridSizer->Add( tcCTo, 0, wxEXPAND | wxALL, Border );
  GridSizer->Add( cbC, 0, wxEXPAND | wxALL, Border );
  GridSizer->AddGrowableCol(1);
  GridSizer->AddGrowableCol(3);
  GridSizer->AddGrowableCol(4);

  wxBoxSizer *ButtonsSizer = new wxBoxSizer( wxHORIZONTAL );

  ButtonsSizer->Add( new wxButton( this, wxID_OK, wxT("OK") ), 0, wxALL, Border);
  ButtonsSizer->Add( new wxButton( this, wxID_CANCEL, wxT("Cancel") ), 0, wxALL, Border);
  ButtonsSizer->Add( new wxButton( this, wxID_HELP, wxT("Help") ),     0, wxALL, Border );

  TopSizer->Add(GridSizer, 1, wxEXPAND | wxALL, 5);
  TopSizer->Add(ButtonsSizer, 0, wxALL, 5);
  SetSizer( TopSizer );      // use the sizer for layout

  TopSizer->SetSizeHints( this );   // set size hints to honour minimum size

  Center();
}
TdlgGenerate::~TdlgGenerate()  {
  cbA->OnChange.Clear();
  cbB->OnChange.Clear();
  cbC->OnChange.Clear();
}
bool TdlgGenerate::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Sender == cbA)  OnAChange();
  if (Sender == cbB)  OnBChange();
  if (Sender == cbC)  OnCChange();
  return true;
}
void TdlgGenerate::OnAChange()  {
  double v = 2;
  cbA->GetValue().ToDouble(&v);
  tcAFrom->SetValue(olxstr(-v/2).u_str());
  tcATo->SetValue(olxstr(v/2).u_str());
}
void TdlgGenerate::OnBChange()  {
  double v = 2;
  cbB->GetValue().ToDouble(&v);
  tcBFrom->SetValue(olxstr(-v/2).u_str());
  tcBTo->SetValue(olxstr(v/2).u_str());
}
void TdlgGenerate::OnCChange()  {
  double v = 2;
  cbC->GetValue().ToDouble(&v);
  tcCFrom->SetValue(olxstr(-v/2).u_str());
  tcCTo->SetValue(olxstr(v/2).u_str());
}
void TdlgGenerate::OnOK(wxCommandEvent& event)  {
  double v = 2;
  tcAFrom->GetValue().ToDouble(&v);  AFrom = v;
  tcBFrom->GetValue().ToDouble(&v);  BFrom = v;
  tcCFrom->GetValue().ToDouble(&v);  CFrom = v;

  tcATo->GetValue().ToDouble(&v);  ATo = v;
  tcBTo->GetValue().ToDouble(&v);  BTo = v;
  tcCTo->GetValue().ToDouble(&v);  CTo = v;
  EndModal(wxID_OK);
}

#endif // native generation options / original wx
