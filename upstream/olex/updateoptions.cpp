/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "updateoptions.h"
#include "eutf8.h"
#include <algorithm>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {
constexpr int ResizeEvent=1;
std::string Text(const olxstr& value){auto bytes=TUtf8::Encode(value);return {bytes.c_str(),bytes.Length()};}
void SingleLine(const olxstr& value){for(size_t i=0;i<value.Length();++i)if(value.CharAt(i)=='\n'||value.CharAt(i)=='\r'||value.CharAt(i)==0)throw std::invalid_argument("Update setting fields must be single-line text");}
template<class List>int Find(const List& list,const olxstr& value){for(size_t i=0;i<list.GetCount();++i)if(list.GetString(i)==value)return static_cast<int>(i);return -1;}
}
struct TdlgUpdateOptions::Alive {TdlgUpdateOptions* owner=nullptr;};
TdlgUpdateOptions::TdlgUpdateOptions(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,std::shared_ptr<UpdateOptionsStore> store,std::shared_ptr<DialogPositionStore> positions)
 :TDialog(nodes,std::move(dialogs),std::move(scheduler),parent,"Update options","dlgAutoUpdate",DialogGeometry{Point{0,0},Size{540,520},true},std::move(positions)),alive_(std::make_shared<Alive>()),store_(std::move(store)){
  if(!store_)throw std::invalid_argument("Original update settings store is required");
  const auto snapshot=store_->Load();alive_->owner=this;std::weak_ptr<Alive> weak=alive_;
  auto retain=[&](const std::shared_ptr<Widget>& value){Retain(value);};
  scroll_=std::make_shared<Widget>(nodes,NodeKind::Scroll,&Body());retain(scroll_);
  column_=std::make_shared<Widget>(nodes,NodeKind::Column,scroll_.get());retain(column_);column_->SetNumber(Property::RowSpacing,8);
  auto label=[&](const std::string& text){auto value=std::make_shared<Widget>(nodes,NodeKind::Text,column_.get());retain(value);value->SetText(TextProperty::Content,text);value->SetNumber(Property::FontSize,15);value->SetHeight(32);labels_.push_back(value);return value;};
  label("Proxy [if required, URL or user:password@URL]");
  tcProxy=std::make_shared<TTextEdit>(nodes,column_.get(),-1,snapshot.values.proxy);retain(tcProxy);tcProxy->SetHeight(44);tcProxy->Widget::SetText(TextProperty::Accessibility,"Update proxy");
  label("Repository URL");
  cbRepository=std::make_shared<TComboBox>(nodes,column_.get(),-1,EmptyString(),Point{},Size{},InputReadOnly);retain(cbRepository);cbRepository->SetHeight(44);cbRepository->Widget::SetText(TextProperty::Accessibility,"Update repository");
  for(size_t i=0;i<snapshot.repositories.Count();++i)if(Find(*cbRepository,snapshot.repositories[i])<0)cbRepository->AddObject(snapshot.repositories[i]);
  if(!snapshot.values.repository.IsEmpty()&&Find(*cbRepository,snapshot.values.repository)<0)cbRepository->AddObject(snapshot.values.repository);
  cbRepository->SetSelection(Find(*cbRepository,snapshot.values.repository));
  label("Update Frequency");
  rbUpdateInterval=std::make_shared<TChoice>(nodes,column_.get());retain(rbUpdateInterval);rbUpdateInterval->SetHeight(40);rbUpdateInterval->Widget::SetText(TextProperty::Accessibility,"Update frequency");
  for(const char* value:{"Always","Daily","Weekly","Monthly","Never"})rbUpdateInterval->AddObject(olxstr(value));
  const int interval=Find(*rbUpdateInterval,snapshot.values.interval);rbUpdateInterval->SetSelection(interval<0?0:interval);
  cbQueryUpdate=std::make_shared<TCheckBox>(nodes,column_.get(),-1,"Download updates automatically");retain(cbQueryUpdate);cbQueryUpdate->SetHeight(40);cbQueryUpdate->SetChecked(!snapshot.values.askForUpdate);
  stLastUpdated=label(Text(snapshot.lastUpdatedCaption));
  status_=label("");status_->SetText(TextProperty::Accessibility,"Update settings save status");status_->SetHeight(48);
  buttons_=std::make_shared<Widget>(nodes,NodeKind::Row,column_.get());retain(buttons_);buttons_->SetNumber(Property::RowSpacing,8);buttons_->SetHeight(44);
  for(int action=0;action<3;++action){
    auto button=std::make_shared<EventWidget>(nodes,NodeKind::Button,buttons_.get(),std::vector<EventKind>{EventKind::Click},[weak,action](const NodeEvent&){auto state=weak.lock();if(!state||!state->owner)return;auto* owner=state->owner;if(action==0)owner->OnOK();else if(action==1)owner->OnCancel();else owner->ToggleHelp();});
    retain(button);button->SetText(TextProperty::Content,action==0?"OK":action==1?"Cancel":"Help");button->SetNumber(Property::Focusable,1);button->SetHeight(40);actionButtons_.push_back(button);
  }
  help_=label("Select a repository and update frequency. Automatic downloads are the inverse of asking before updates. OK saves settings only; this window never downloads or installs updates. Cancel leaves settings unchanged.");help_->SetHeight(112);help_->Hide();
  OnResize.Add(this,ResizeEvent,msiExecute);Layout();
}
TdlgUpdateOptions::~TdlgUpdateOptions(){alive_->owner=nullptr;OnResize.Remove(this);}
void TdlgUpdateOptions::Layout(){
  const int width=std::max(120,GetWidth()-24);scroll_->SetWidth(width);scroll_->SetHeight(std::max(40,GetHeight()-88));column_->SetWidth(width);column_->SetHeight(helpVisible_?640:520);
  for(const auto& label:labels_)label->SetWidth(width);tcProxy->SetWidth(width);cbRepository->SetWidth(width);rbUpdateInterval->SetWidth(width);cbQueryUpdate->SetWidth(width);buttons_->SetWidth(width);
  for(const auto& button:actionButtons_)button->SetWidth(std::max(24,(width-16)/3));
}
void TdlgUpdateOptions::ToggleHelp(){helpVisible_=!helpVisible_;help_->Show(helpVisible_);Layout();}
bool TdlgUpdateOptions::Dispatch(int event,short,const IOlxObject*,const IOlxObject*,TActionQueue*){if(event==ResizeEvent){Layout();return true;}return false;}
UpdateOptionsValues TdlgUpdateOptions::GetDraft()const{return {tcProxy->GetText(),cbRepository->GetValue(),rbUpdateInterval->GetValue(),!cbQueryUpdate->IsChecked()};}
void TdlgUpdateOptions::SetDraft(const UpdateOptionsValues& values){
  if(IsClosing()||saved_)throw std::logic_error("Update settings have already been saved or are closing");
  SingleLine(values.proxy);SingleLine(values.repository);SingleLine(values.interval);
  const int repository=Find(*cbRepository,values.repository),interval=Find(*rbUpdateInterval,values.interval);
  if(repository<0||interval<0)throw std::invalid_argument("Select an available repository and update frequency");
  tcProxy->SetText(values.proxy);cbRepository->SetSelection(repository);rbUpdateInterval->SetSelection(interval);cbQueryUpdate->SetChecked(!values.askForUpdate);
}
bool TdlgUpdateOptions::OnOK(){
  if(IsClosing())return false;
  if(!saved_){
    auto life=alive_;auto store=store_;
    try{
      const auto draft=GetDraft();SingleLine(draft.proxy);SingleLine(draft.repository);SingleLine(draft.interval);
      if(cbRepository->GetSelection()<0||rbUpdateInterval->GetSelection()<0)throw std::invalid_argument("Select the update options before saving");
      store->Save(draft);
    }catch(const std::exception& e){
      if(life->owner!=this)return false;
      saveError_=std::string("Could not save update settings: ")+e.what()+". Your edits are kept; retry or cancel.";
      status_->SetText(TextProperty::Content,saveError_);return false;
    }catch(...){
      if(life->owner!=this)return false;
      saveError_="Could not save update settings. Your edits are kept; retry or cancel.";
      status_->SetText(TextProperty::Content,saveError_);return false;
    }
    if(life->owner!=this)return true;
    saved_=true;saveError_.clear();status_->SetText(TextProperty::Content,"Settings saved; waiting for this window to close.");
  }
  EndModal(1);return true;
}
void TdlgUpdateOptions::OnCancel(){if(IsClosing())RetryClose();else EndModal(saved_?1:DialogCancelled);}

#else
#include "updateoptions.h"
#include "bapp.h"
#include "efile.h"
#include "etime.h"

//..............................................................................
TdlgUpdateOptions::TdlgUpdateOptions(TMainFrame *ParentFrame) :
  TDialog(ParentFrame, wxT("Update options"), wxT("dlgAutoUpdate"))
{
  short Border = 0;

  stProxy = new wxStaticText(this, -1,
    wxT("Proxy [if required, URL or user:password@URL]"),
    wxDefaultPosition, wxDefaultSize);
  stRepository = new wxStaticText(this, -1, wxT("Repository URL"),
    wxDefaultPosition, wxDefaultSize);
  stLastUpdated = new wxStaticText(this, -1, wxT("Last updated: unknown"),
    wxDefaultPosition, wxDefaultSize);

  if (uapi.GetSettings().last_updated != 0) {
    stLastUpdated->SetLabel(
      wxString(wxT("Last updated: ")) +
      TETime::FormatDateTime(uapi.GetSettings().last_updated).u_str());
  }
  else {
    stLastUpdated->SetLabel(wxT("Last updated: Never"));
  }
  TStrList repos;
  uapi.GetAvailableMirrors(repos);
  tcProxy = new wxTextCtrl(this, -1, uapi.GetSettings().proxy.u_str(),
    wxDefaultPosition, wxSize(400, -1), 0);
  cbRepository = new wxComboBox(this, -1, wxEmptyString, wxDefaultPosition,
    wxSize(400,-1), 0, NULL, wxTE_READONLY);
  for (size_t i=0; i < repos.Count(); i++)
    cbRepository->Append(uapi.AddTagPart(repos[i], true).u_str());
  cbRepository->SetValue(
    uapi.AddTagPart(uapi.GetSettings().repository, true).u_str());

  cbQueryUpdate = new wxCheckBox(this, -1,
    wxT("Download updates automatically"));
  cbQueryUpdate->SetValue(!uapi.GetSettings().ask_for_update);

  wxString options[] = {
    wxT("Always"), wxT("Daily"), wxT("Weekly"), wxT("Monthly"), wxT("Never")
  };
  rbUpdateInterval = new wxRadioBox(this, -1, wxT("Update Frequency"),
    wxDefaultPosition, wxDefaultSize, 5, options);
  int selIndex = rbUpdateInterval->FindString(
    uapi.GetSettings().update_interval.u_str());
  if (selIndex >= 0)
    rbUpdateInterval->SetSelection(selIndex);

#if !wxCHECK_VERSION(2,9,0)
  wxFlexGridSizer *ASizer = new wxFlexGridSizer(1, 7);
#else
  wxFlexGridSizer *ASizer = new wxFlexGridSizer(1, 7, 0);
#endif
  //wxBoxSizer *ASizer = new wxBoxSizer(wxVERTICAL);
  ASizer->Add(stProxy, 0, wxALL, Border);
  ASizer->Add(tcProxy, 1, wxALL, Border);
  ASizer->Add(stRepository, 0, wxALL, Border);
  ASizer->Add(cbRepository, 0, wxALL, Border);
  ASizer->Add(rbUpdateInterval, 0, wxALL, Border);
  ASizer->Add(cbQueryUpdate, 0, wxALL, Border);
  ASizer->Add(stLastUpdated, 0, wxALL, Border);
  wxBoxSizer *ButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  ButtonsSizer->Add(new wxButton(this, wxID_OK, wxT("OK")), 0, wxALL, Border);
  ButtonsSizer->Add(new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxALL, Border);
  ButtonsSizer->Add(new wxButton(this, wxID_HELP, wxT("Help")), 0, wxALL, Border);

  wxBoxSizer *TopSiser = new wxBoxSizer(wxVERTICAL);
  TopSiser->Add(ASizer, 0, wxALL, 0);
  TopSiser->Add(ButtonsSizer, 0, wxALL, 10);
  SetSizer(TopSiser); // use the sizer for layout
  TopSiser->SetSizeHints(this);   // set size hints to honour minimum size
  Center();
  Bind(wxEVT_BUTTON, &TdlgUpdateOptions::OnOK, this, wxID_OK);
}
//..............................................................................
TdlgUpdateOptions::~TdlgUpdateOptions() {}
//..............................................................................
void TdlgUpdateOptions::OnOK(wxCommandEvent& event)  {
  uapi.GetSettings().proxy = tcProxy->GetValue();
  uapi.GetSettings().repository = uapi.TrimTagPart(cbRepository->GetValue());
  uapi.GetSettings().update_interval = rbUpdateInterval->GetStringSelection();
  uapi.GetSettings().ask_for_update = !cbQueryUpdate->GetValue();
  uapi.GetSettings().Save();
  EndModal(wxID_OK);
}
//..............................................................................

#endif // native update options / original wx
