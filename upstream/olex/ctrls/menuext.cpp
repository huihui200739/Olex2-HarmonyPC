/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "menuext.h"
#include "original_button_mode.h"
#include "original_menu_state.h"
#include "olx_control_state.h"
#include "eutf8.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
namespace {
std::string MenuUtf8(const olxstr& value) {auto s=TUtf8::Encode(value);return {s.c_str(),s.Length()};}
}
void TMenu::AssertThread()const {if(thread_!=std::this_thread::get_id())throw std::runtime_error("Menus require their owning UI thread");}
TMenu::TMenu(const olxstr& name,std::shared_ptr<MenuPopup> popup):name_(name),popup_(popup?std::move(popup):CreateArkUiMenuPopup()) {
  if(!popup_)throw std::invalid_argument("A real native menu presenter is required");
}
TMenu::~TMenu(){life_.reset();try{popup_->Close();}catch(...){}items_.clear();}
TMenuItem::TMenuItem(short type,int id,TMenu* parent,const olxstr& name):kind_(type),id_(id),parent_(parent),label_(name),OnModeChange(Actions.New(evt_on_mode_change_id)) {
  if(type!=mtSeparator&&type!=mtNormalItem&&type!=mtCheckItem&&type!=mtRadioItem)throw std::invalid_argument("Unknown menu item kind");SetToDelete(false);
}
TMenuItem::~TMenuItem(){if(ActionQueue)ActionQueue->Remove(this);}
TMenuItem* TMenu::Append(TMenuItem* item) {
  AssertThread();if(!item||item->parent_&&item->parent_!=this)throw std::invalid_argument("Menu item already belongs to another menu");
  for(const auto& old:items_)if(old.get()==item)throw std::invalid_argument("Menu item already appended");
  if(items_.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Menu too large");items_.reserve(items_.size()+1);Close();item->parent_=this;items_.emplace_back(item);return item;
}
TMenuItem* TMenu::Append(int id,const olxstr& label,const olxstr& help,short kind) {
  auto item=std::make_unique<TMenuItem>(kind,id,nullptr,label);item->help_=help;auto* result=Append(item.get());item.release();return result;
}
TMenuItem* TMenu::Append(int id,const olxstr& label,TMenu* submenu,const olxstr& help) {
  AssertThread();if(!submenu||submenu==this||submenu->parent_)throw std::invalid_argument("Submenu must be uniquely owned");
  for(auto* p=this;p;p=p->parent_)if(p==submenu)throw std::invalid_argument("Menu ownership cycle");
  auto item=std::make_unique<TMenuItem>(mtNormalItem,id,nullptr,label);item->help_=help;
  auto* result=Append(item.get());item.release();result->submenu_.reset(submenu);submenu->parent_=this;return result;
}
TMenuItem* TMenu::AppendSeparator(){return Append(-1,EmptyString(),EmptyString(),mtSeparator);}
void TMenu::Close(){AssertThread();if(parent_)parent_->Close();popup_->Close();session_.reset();owner_=nullptr;ownerLife_.reset();selected_={};}
void TMenu::Clear(){AssertThread();Close();items_.clear();data_.reset();}
bool TMenu::Destroy(TMenuItem* item) {
  AssertThread();auto it=std::find_if(items_.begin(),items_.end(),[&](const auto& value){return value.get()==item;});if(it==items_.end())return false;Close();items_.erase(it);return true;
}
TMenuItem* TMenu::FindItemByPosition(size_t i)const {AssertThread();if(i>=items_.size())throw std::out_of_range("Menu position");return items_[i].get();}
TMenuItem* TMenu::FindItem(int id)const {AssertThread();for(const auto& item:items_){if(item->id_==id&&!item->IsSeparator())return item.get();if(item->submenu_)if(auto* found=item->submenu_->FindItem(id))return found;}return nullptr;}
TMenu* TMenu::CopyMenu(const TMenu& source) {
  source.AssertThread();auto out=std::make_unique<TMenu>(source.name_);
  for(const auto& item:source.items_){std::unique_ptr<TMenu> child;if(item->submenu_)child.reset(CopyMenu(*item->submenu_));TMenuItem* copy=child?out->Append(item->id_,item->label_,child.get(),item->help_):out->Append(item->id_,item->label_,item->help_,item->kind_);if(child)child.release();copy->enabled_=item->enabled_;copy->checked_=item->checked_;copy->Command=item->Command;}
  if(source.data_){auto data=std::make_unique<MenuData>();data->macros=source.data_->macros;out->data_=std::move(data);}return out.release();
}
TMenu* TMenu::CreateMenu(const olxstr& definition,int first_id,TMenu* menu) {
  TStrList tokens(definition,';');if(tokens.IsEmpty())return nullptr;
  if(tokens.Count()>static_cast<size_t>(std::numeric_limits<int>::max())||first_id>std::numeric_limits<int>::max()-static_cast<int>(tokens.Count()))throw std::overflow_error("Menu ID range");
  std::unique_ptr<TMenu> owned;if(!menu){owned=std::make_unique<TMenu>();menu=owned.get();}
  auto data=std::make_unique<MenuData>();if(menu->data_)data->macros=menu->data_->macros;
  for(size_t i=0;i<tokens.Count();++i){if(tokens[i]=='#'){menu->AppendSeparator();continue;}auto arrow=tokens[i].IndexOf("<-");if(arrow==InvalidIndex)continue;const int id=first_id+static_cast<int>(i);menu->Append(id,tokens[i].SubStringTo(arrow));data->macros(id,tokens[i].SubStringFrom(arrow+2));}
  menu->data_=std::move(data);if(owned)return owned.release();return menu;
}
void TMenuItem::Check(bool value) {
  if(!IsCheckable())throw std::invalid_argument("Only check/radio menu items can be checked");
  if(value&&kind_==mtRadioItem&&parent_){size_t index=0;while(index<parent_->items_.size()&&parent_->items_[index].get()!=this)++index;
    size_t begin=index,end=index;while(begin>0&&parent_->items_[begin-1]->kind_==mtRadioItem)--begin;while(end<parent_->items_.size()&&parent_->items_[end]->kind_==mtRadioItem)++end;
    for(size_t i=begin;i<end;++i)parent_->items_[i]->checked_=false;
  }checked_=value;
}
void TMenuItem::SetActionQueue(TActionQueue& queue,const olxstr& dependency,short dependentOn) {
  if(dependentOn!=ModeDependent&&dependentOn!=StateDependent&&dependentOn!=0)throw std::invalid_argument("Unknown menu dependency");
  auto separator=dependency.FirstIndexOf(';');olxstr mode=separator==InvalidIndex?dependency:dependency.SubStringTo(separator);olxstr argument=separator==InvalidIndex?EmptyString():dependency.SubStringFrom(separator+1);
  if(ActionQueue)ActionQueue->Remove(this);ActionQueue=&queue;DependMode=mode;OnModeChangeCmd=argument;DependentOn=dependentOn;queue.Add(this);
}
void TMenuItem::ValidateState(){if(!IsCheckable()||DependMode.IsEmpty())return;if(DependentOn==ModeDependent)Check(OriginalButtonModeActive(DependMode));else if(DependentOn==StateDependent)Check(OriginalMenuStateActive(DependMode,OnModeChangeCmd));}
bool TMenuItem::Execute(const IOlxObject*,const IOlxObject* data,TActionQueue*) {
  if(data&&(data->Is<TModeChange>()&&DependentOn==ModeDependent||data->Is<TStateChange>()&&DependentOn==StateDependent))ValidateState();return true;
}
void TMenu::Popup(Widget* owner,std::function<void(int)> selected) {
  AssertThread();if(!owner||!owner->IsEventAllowed())throw std::invalid_argument("Menu owner is not interactable");Close();owner_=owner;ownerLife_=owner->ObjectLifetime();selected_=std::move(selected);error_.clear();OpenLevel(this);
}
void TMenu::OpenLevel(TMenu* level) {
  std::vector<MenuRow> rows;rows.reserve(level->items_.size()+1);
  if(level!=this)rows.push_back({-1,"‹ 返回","",false,true,false,false,false});
  for(size_t i=0;i<level->items_.size();++i){auto& item=*level->items_[i];item.ValidateState();rows.push_back({static_cast<int>(i),MenuUtf8(item.label_),MenuUtf8(item.help_),item.IsSeparator(),item.enabled_,item.IsCheckable(),item.checked_,bool(item.submenu_)});}
  session_=std::make_shared<int>(0);std::weak_ptr<int> session=session_;std::weak_ptr<int> weak=life_;popup_->Open(MenuUtf8(level->name_),rows,[weak,session,this,level](int position){if(weak.expired()||session.expired())return;try{Choose(level,position);}catch(const std::exception& e){if(!weak.expired())error_=e.what();}catch(...){if(!weak.expired())error_="Menu activation failed";}});
}
void TMenu::Choose(TMenu* level,int position) {
  AssertThread();if(position==std::numeric_limits<int>::min()){Close();return;}if(!owner_||ownerLife_.expired()||!owner_->IsEventAllowed()){Close();return;}if(position==-1&&level!=this){OpenLevel(level->parent_);return;}
  if(position<0||static_cast<size_t>(position)>=level->items_.size())return;auto& item=*level->items_[position];if(!item.enabled_||item.IsSeparator())return;
  if(item.submenu_){OpenLevel(item.submenu_.get());return;}
  const int id=item.id_;if(item.IsCheckable())item.Check(item.kind_==mtRadioItem||!item.checked_);auto callback=selected_;Close();if(callback)callback(id); // owner/menu may be destroyed by callback
}
bool TMenu::Activate(int id) {
  AssertThread();auto* item=FindItem(id);if(!item||!item->enabled_||item->IsSeparator()||item->submenu_)return false;
  for(auto* menu=item->parent_;menu&&menu!=this;menu=menu->parent_){auto* parent=menu->parent_;if(!parent)return false;auto found=std::find_if(parent->items_.begin(),parent->items_.end(),[&](const auto& i){return i->submenu_.get()==menu;});if(found==parent->items_.end()||!(*found)->enabled_)return false;}
  if(owner_&&(ownerLife_.expired()||!owner_->IsEventAllowed()))return false;if(item->IsCheckable())item->Check(item->kind_==mtRadioItem||!item->checked_);auto callback=selected_;Close();if(callback)callback(id);return true;
}

#else
#include "menuext.h"
#include "frameext.h"
#include "olxstate.h"

using namespace ctrl_ext;
IMPLEMENT_CLASS(TMenu, wxMenu)
IMPLEMENT_CLASS(TMenuItem, wxMenuItem)

TMenu* TMenu::CopyMenu(const wxMenu& menu) {
  TMenu* M = new TMenu;
  for (unsigned int i = 0; i < menu.GetMenuItemCount(); i++) {
    wxMenuItem* mi = menu.FindItemByPosition(i);
    if (mi->GetSubMenu() != NULL) {
      TMenu* nm = CopyMenu(*mi->GetSubMenu());
      int miId = mi->GetId();
      wxItemKind miKind = mi->GetKind();
      const wxString& miText = mi->GetItemLabel();
      const wxString& miHelpStr = mi->GetHelp();
      M->Append(miId, miText, nm, miHelpStr);
    }
    else {
      int miId = mi->GetId();
      wxItemKind miKind = mi->GetKind();
      const wxString& miText = mi->GetItemLabel();
      const wxString& miHelpStr = mi->GetHelp();
      wxMenuItem* nmi = new wxMenuItem(M, miId, miText, miHelpStr, miKind);
      M->Append(nmi);
    }
  }
  return M;
}
//..............................................................................
void TMenu::Clear() {
  TPtrList<wxMenuItem> menues;
  const size_t count = GetMenuItemCount();
  menues.SetCapacity(count + 1);
  for (size_t i = 0; i < count; i++)
    menues.Add(FindItemByPosition(i));
  for (size_t i = 0; i < count; i++) {
    Destroy(menues[i]);
  }
}
//..............................................................................
wxMenu* TMenu::CreateMenu(const olxstr& def, int first_id, wxMenu *menu) {
  TStrList items(def, ';');
  if (items.IsEmpty()) {
    return 0;
  }
  olx_object_ptr<wxMenu> menu_;
  if (menu == 0) {
    menu_ = menu = new wxMenu();
  }
  olx_object_ptr<MenuData> data = new MenuData();
  for (size_t i = 0; i < items.Count(); i++) {
    if (items[i] == '#') {
      menu->AppendSeparator();
    }
    size_t idx = items[i].IndexOf("<-");
    if (idx == InvalidIndex) {
      continue;
    }
    wxMenuItem* mi = menu->Append(first_id + i, items[i].SubStringTo(idx).u_str());
    data->macros(first_id + i, items[i].SubStringFrom(idx + 2));
  }
  menu->SetClientObject(data.release());
  menu_.release(); // if created
  return menu;
}

//----------------------------------------------------------------------------//
// TMenuItem implementation
//----------------------------------------------------------------------------//
TMenuItem::~TMenuItem()  {}
//..............................................................................
void TMenuItem::SetActionQueue(TActionQueue& q, const olxstr& dependMode,
  short dependentOn)
{
  const size_t cmdind = dependMode.FirstIndexOf(';');
  if (cmdind != InvalidIndex) {
    DependMode = dependMode.SubStringTo(cmdind);
    OnModeChangeCmd = dependMode.SubStringFrom(cmdind + 1);
  }
  else {
    DependMode = dependMode;
  }
  DependentOn = dependentOn;
  ActionQueue = &q;
  ActionQueue->Add(this);
}
//..............................................................................
void TMenuItem::ValidateState() {
  if (DependentOn != 0 && IsCheckable() && !DependMode.IsEmpty()) {
    if (DependentOn == ModeDependent) {
      Check(TModeRegistry::CheckMode(DependMode));
    }
    else if (DependentOn == StateDependent) {
      Check(TStateRegistry::GetInstance().CheckState(
        DependMode, OnModeChangeCmd));
    }
  }
}
//..............................................................................
bool TMenuItem::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Data != 0 && Data->Is<TModeChange>()) {
    const TModeChange* mc = (const TModeChange*)Data;
    if (this->IsCheckable())
      Check(TModeRegistry::CheckMode(DependMode));
  }
  if (Data != 0 && Data->Is<TStateChange>()) {
    TStateChange* sc = (TStateChange*)Data;
    if (this->IsCheckable()) {
      Check(
        TStateRegistry::GetInstance().CheckState(DependMode, OnModeChangeCmd));
    }
  }
  return true;
}

#endif // native ordinary navigation branch
