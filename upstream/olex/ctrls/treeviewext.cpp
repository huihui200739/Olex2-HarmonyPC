/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "treeviewext.h"
#include "original_menu_state.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
namespace {
std::string TreeUtf8(const olxstr& value){auto t=TUtf8::Encode(value);std::string s(t.c_str(),t.Length());if(s.find('\0')!=std::string::npos)throw std::invalid_argument("Tree labels cannot contain NUL");return s;}
}
TTreeView::TTreeView(NodeBackend& backend,Widget* parent,int id,Point pos,Size size,long flags)
 :Widget(backend,NodeKind::Tree,parent,id,pos,size),AOlxCtrl(this),
 OnDblClick(ActionQueue::New(Actions,evt_on_dbl_click_id)),OnSelect(ActionQueue::New(Actions,evt_on_select_id)),OnEdit(ActionQueue::New(Actions,evt_change_id)) {
  if(flags&~(TreeHideRoot|TreeEditLabels))throw std::invalid_argument("Unsupported native tree style");RememberWindowStyle(flags);SetNumber(Property::Focusable,1);
  Listen({EventKind::TreeSelect,EventKind::TreeToggle,EventKind::TreeActivate,EventKind::TreeContext,EventKind::TreeEdit,EventKind::Key});Sync();
}
TTreeView::~TTreeView(){contextMenu.reset();}
TTreeView::Node& TTreeView::Find(TreeItemId id)const {AssertThread();auto it=nodes_.find(id.value);if(id.owner!=this||it==nodes_.end())throw std::out_of_range("Tree item does not belong to this tree");return *it->second;}
std::vector<size_t> TTreeView::All(size_t root)const {
  std::vector<size_t> result,pending;if(root)pending.push_back(root);
  while(!pending.empty()){auto id=pending.back();pending.pop_back();result.push_back(id);const auto& children=nodes_.at(id)->children;pending.insert(pending.end(),children.rbegin(),children.rend());}return result;
}
void TTreeView::Sync(){
  AssertThread();std::vector<TreeRow> rows;std::vector<size_t> visible;std::vector<std::pair<size_t,size_t>> pending;if(root_)pending.emplace_back(root_,0);
  while(!pending.empty()){auto [id,depth]=pending.back();pending.pop_back();auto& node=*nodes_.at(id);const bool hidden=id==root_&&(GetWindowStyle()&TreeHideRoot);
    if(!hidden){if(depth>32767||rows.size()>=static_cast<size_t>(std::numeric_limits<int>::max()))throw std::length_error("Tree exceeds native layout limits");visible.push_back(id);rows.push_back({TreeUtf8(node.label),depth, !node.children.empty(),node.expanded,id==selected_,id==editing_});}
    if(hidden||node.expanded)for(auto it=node.children.rbegin();it!=node.children.rend();++it)pending.emplace_back(*it,hidden?0:depth+1);
  }
  Backend().SetTreeRows(Handle(),rows);visible_.swap(visible);
}
TreeItemId TTreeView::AddRoot(const olxstr& label){
  AssertThread();if(root_)throw std::logic_error("Tree already has a root");if(next_==std::numeric_limits<size_t>::max())throw std::overflow_error("Tree ID space");TreeUtf8(label);auto node=std::make_unique<Node>();node->id=next_++;node->label=label;const size_t id=node->id;nodes_.emplace(id,std::move(node));root_=id;
  try{Sync();}catch(...){root_=0;nodes_.erase(id);throw;}return {id,this};
}
TreeItemId TTreeView::AppendItem(TreeItemId parent,const olxstr& label,TTreeNodeData* data){
  AssertThread();auto& p=Find(parent);TreeUtf8(label);if(next_==std::numeric_limits<size_t>::max())throw std::overflow_error("Tree ID space");p.children.reserve(p.children.size()+1);auto node=std::make_unique<Node>();node->id=next_++;node->parent=parent.value;node->label=label;const auto id=node->id;nodes_.emplace(id,std::move(node));p.children.push_back(id);
  try{Sync();}catch(...){p.children.pop_back();nodes_.erase(id);throw;}nodes_.at(id)->data.reset(data);return {id,this};
}
void TTreeView::DeleteAllItems(){AssertThread();Backend().SetTreeRows(Handle(),{});visible_.clear();nodes_.clear();root_=selected_=editing_=0;}
void TTreeView::Delete(TreeItemId id){
  auto& node=Find(id);if(id.value==root_){DeleteAllItems();return;}const auto ids=All(id.value);auto& parent=*nodes_.at(node.parent);auto previous=parent.children;const size_t selected=selected_,editing=editing_;parent.children.erase(std::remove(parent.children.begin(),parent.children.end(),id.value),parent.children.end());if(std::find(ids.begin(),ids.end(),selected_)!=ids.end())selected_=0;if(std::find(ids.begin(),ids.end(),editing_)!=ids.end())editing_=0;
  try{Sync();}catch(...){parent.children.swap(previous);selected_=selected;editing_=editing;throw;}for(auto child:ids)nodes_.erase(child);
}
size_t TTreeView::GetChildrenCount(TreeItemId id,bool recursive)const {const auto& node=Find(id);return recursive?All(id.value).size()-1:node.children.size();}
std::vector<TreeItemId> TTreeView::Children(TreeItemId id)const{std::vector<TreeItemId> out;for(auto child:Find(id).children)out.push_back({child,this});return out;}
void TTreeView::SetExpanded(TreeItemId id,bool value,bool descendants){
  Find(id);auto ids=descendants?All(id.value):std::vector<size_t>{id.value};std::vector<bool> old;for(auto child:ids){old.push_back(nodes_.at(child)->expanded);nodes_.at(child)->expanded=value&&!nodes_.at(child)->children.empty();}
  try{Sync();}catch(...){for(size_t i=0;i<ids.size();++i)nodes_.at(ids[i])->expanded=old[i];throw;}
}
void TTreeView::SelectItem(TreeItemId id,bool select){
  Find(id);const auto old=selected_;std::vector<std::pair<size_t,bool>> ancestors;selected_=select?id.value:0;
  if(select)for(size_t parent=nodes_.at(id.value)->parent;parent;parent=nodes_.at(parent)->parent){auto& n=*nodes_.at(parent);ancestors.emplace_back(parent,n.expanded);n.expanded=true;}
  try{Sync();}catch(...){selected_=old;for(auto [parent,expanded]:ancestors)nodes_.at(parent)->expanded=expanded;throw;}
  if(old!=selected_)OnSelect.Execute(this); // no member access after an original action
}
void TTreeView::SetItemText(TreeItemId id,const olxstr& label){auto& node=Find(id);TreeUtf8(label);auto old=node.label;node.label=label;try{Sync();}catch(...){node.label=old;throw;}}
void TTreeView::SelectByLabel(const olxstr& label){for(auto id:All(root_))if(nodes_.at(id)->label==label){const bool enabled=OnSelect.IsEnabled();OnSelect.SetEnabled(false);try{SelectItem({id,this});}catch(...){OnSelect.SetEnabled(enabled);throw;}OnSelect.SetEnabled(enabled);return;}}
void TTreeView::SelectByData(const olxstr& data){for(auto id:All(root_)){const auto& node=*nodes_.at(id);if(node.data&&node.data->GetData()&&data==node.data->GetData()->ToString()){const bool enabled=OnSelect.IsEnabled();OnSelect.SetEnabled(false);try{SelectItem({id,this});}catch(...){OnSelect.SetEnabled(enabled);throw;}OnSelect.SetEnabled(enabled);return;}}}
bool TTreeView::LoadFromStrings(const TStrList& strings){
  AssertThread();if(strings.Count()%2)return false;std::map<size_t,std::unique_ptr<Node>> next;size_t id=next_;if(id==std::numeric_limits<size_t>::max())throw std::overflow_error("Tree ID space");auto root=std::make_unique<Node>();root->id=id++;root->label="Root";const size_t newroot=root->id;next.emplace(newroot,std::move(root));std::vector<std::pair<size_t,size_t>> stack;
  for(size_t i=0;i<strings.Count();i+=2){if(id==std::numeric_limits<size_t>::max())throw std::overflow_error("Tree ID space");const size_t depth=strings[i].LeadingCharCount('\t');while(!stack.empty()&&stack.back().first>=depth)stack.pop_back();const size_t parent=stack.empty()?newroot:stack.back().second;auto node=std::make_unique<Node>();node->id=id++;node->parent=parent;node->label=olxstr(strings[i]).Trim('\t');TreeUtf8(node->label);if(!olxstr(strings[i+1]).Trim('\t').IsEmpty())node->data=std::make_unique<TTreeNodeData>(new olxstr(strings[i+1]));const size_t child=node->id;next.at(parent)->children.push_back(child);next.emplace(child,std::move(node));stack.emplace_back(depth,child);}
  const size_t oldroot=root_,oldselected=selected_,oldediting=editing_;nodes_.swap(next);root_=newroot;selected_=editing_=0;
  try{Sync();}catch(...){nodes_.swap(next);root_=oldroot;selected_=oldselected;editing_=oldediting;throw;}next_=id;return true;
}
olxstr TTreeView::SaveState()const {
  AssertThread();const auto ids=All(root_);TEBitArray bits(ids.size());size_t selected=InvalidIndex;for(size_t i=0;i<ids.size();++i){bits.Set(i,nodes_.at(ids[i])->expanded);if(ids[i]==selected_)selected=i;}
  olxstr value=bits.ToBase64String();return value.stream(';')<<selected<<';'<<static_cast<int>(GetNumber(Property::TreeScrollY))<<';'<<static_cast<int>(GetNumber(Property::TreeScrollX));
}
void TTreeView::RestoreState(const olxstr& value){
  AssertThread();TStrList parts(value,';');if(parts.Count()!=4)return;TEBitArray bits;bits.FromBase64String(parts[0]);const auto ids=All(root_);if(bits.Count()!=ids.size())return;const size_t selected=parts[1].ToSizeT();const int y=parts[2].ToInt(),x=parts[3].ToInt();if((selected!=InvalidIndex&&selected>=ids.size())||x<0||y<0)return;
  const size_t oldselected=selected_;const double oldx=GetNumber(Property::TreeScrollX),oldy=GetNumber(Property::TreeScrollY);std::vector<bool> previous;for(size_t i=0;i<ids.size();++i){previous.push_back(nodes_.at(ids[i])->expanded);nodes_.at(ids[i])->expanded=bits[i];}selected_=selected==InvalidIndex?0:ids[selected];
  try{Sync();SetNumber(Property::TreeScrollY,y);SetNumber(Property::TreeScrollX,x);}catch(...){selected_=oldselected;for(size_t i=0;i<ids.size();++i)nodes_.at(ids[i])->expanded=previous[i];try{Sync();SetNumber(Property::TreeScrollX,oldx);SetNumber(Property::TreeScrollY,oldy);}catch(...){throw std::runtime_error("Tree state restore and rollback both failed");}throw;}
}
void TTreeView::BeginLabelEdit(TreeItemId id){Find(id);if(!(GetWindowStyle()&TreeEditLabels))throw std::logic_error("Tree label editing is not enabled");std::weak_ptr<void> life=ObjectLifetime();SelectItem(id);if(life.expired())return;auto old=editing_;editing_=id.value;try{Sync();}catch(...){editing_=old;throw;}}
void TTreeView::CancelLabelEdit(){AssertThread();auto old=editing_;editing_=0;try{Sync();}catch(...){editing_=old;throw;}}
void TTreeView::SetWindowStyle(long value){AssertThread();if(value&~(TreeHideRoot|TreeEditLabels))throw std::invalid_argument("Unsupported native tree style");const long old=GetWindowStyle();const auto editing=editing_;RememberWindowStyle(value);if(!(value&TreeEditLabels))editing_=0;try{Sync();}catch(...){RememberWindowStyle(old);editing_=editing;throw;}}
void TTreeView::SetContextMenu(TMenu* menu){AssertThread();if(contextMenu.get()==menu)return;if(contextMenu)contextMenu->Close();contextMenu.reset(menu);}
TMenu* TTreeView::CreateDefaultContextMenu(){auto menu=std::make_unique<TMenu>();menu->Append(ID_TREE_ExpandAll,"Expand all");menu->Append(ID_TREE_CollapseAll,"Collapse all");return menu.release();}
TMenu* TTreeView::CreateContextMenu(const olxstr& definition){if(TStrList(definition,';').IsEmpty())return nullptr;std::unique_ptr<TMenu> menu(CreateDefaultContextMenu());TMenu::CreateMenu(definition,ID_TREE_LAST,menu.get());return menu.release();}
void TTreeView::ShowContextMenu(){AssertThread();if(!contextMenu||!IsEventAllowed())return;std::weak_ptr<void> weak=ObjectLifetime();contextMenu->Popup(this,[weak,this](int id){if(!weak.expired())ContextAction(id);});}
void TTreeView::ContextAction(int id){
  if(!IsEventAllowed())return;if(id==ID_TREE_ExpandAll){if(selected_)ExpandAllChildren({selected_,this});return;}if(id==ID_TREE_CollapseAll){if(selected_)CollapseAllChildren({selected_,this});return;}
  if(contextMenu&&contextMenu->GetClientObject()){const auto command=contextMenu->GetClientObject()->macros.Find(id,EmptyString());if(!command.IsEmpty())OriginalTreeCommand(command);}
}
void TTreeView::OnNativeEvent(const NodeEvent& event){
  if(event.kind==EventKind::Key){if(!event.key||!event.key->down||event.key->control||event.key->alt||event.key->meta)return;auto& key=*event.key;if(editing_&&key.logical!=LogicalKey::Escape)return;auto found=std::find(visible_.begin(),visible_.end(),selected_);size_t index=found==visible_.end()?0:static_cast<size_t>(found-visible_.begin());if(visible_.empty())return;
    size_t target=selected_;switch(key.logical){case LogicalKey::Up:target=visible_[index?index-1:0];break;case LogicalKey::Down:target=visible_[found==visible_.end()?0:std::min(index+1,visible_.size()-1)];break;case LogicalKey::Home:target=visible_.front();break;case LogicalKey::End:target=visible_.back();break;
      case LogicalKey::Left:if(selected_&&HasChildren({selected_,this})&&IsExpanded({selected_,this})){key.handled=true;Collapse({selected_,this});return;}if(selected_){target=nodes_.at(selected_)->parent;if((GetWindowStyle()&TreeHideRoot)&&target==root_)target=selected_;}break;
      case LogicalKey::Right:if(selected_&&!nodes_.at(selected_)->children.empty()){key.handled=true;if(!IsExpanded({selected_,this})){Expand({selected_,this});return;}target=nodes_.at(selected_)->children.front();}break;
      case LogicalKey::Enter:key.handled=true;OnDblClick.Execute(this);return;case LogicalKey::Space:if(selected_){key.handled=true;SetExpanded({selected_,this},!IsExpanded({selected_,this}));}return;case LogicalKey::Escape:if(editing_){key.handled=true;CancelLabelEdit();}return;default:return;}
    key.handled=true;if(target&&target!=selected_)SelectItem({target,this});return;
  }
  if(!std::isfinite(event.value)||event.value<0||std::floor(event.value)!=event.value||event.value>=visible_.size())return;const size_t id=visible_[static_cast<size_t>(event.value)];
  if(event.kind==EventKind::TreeToggle){SetExpanded({id,this},!IsExpanded({id,this}));return;}
  if(event.kind==EventKind::TreeSelect){SelectItem({id,this});return;}
  if(event.kind==EventKind::TreeActivate||event.kind==EventKind::TreeContext){std::weak_ptr<void> weak=ObjectLifetime();SelectItem({id,this});if(weak.expired())return;if(event.kind==EventKind::TreeActivate)OnDblClick.Execute(this);else ShowContextMenu();return;}
  if(event.kind==EventKind::TreeEdit&&editing_==id){auto label=TUtf8::Decode(event.text.c_str(),event.text.size());auto old=nodes_.at(id)->label;const size_t edit=editing_;editing_=0;nodes_.at(id)->label=label;try{Sync();}catch(...){nodes_.at(id)->label=old;editing_=edit;throw;}olxstr data=olxstr(OnEdit.data).Replace("~label~",label);OnEdit.Execute(this,&data);}
}

#else
#include "treeviewext.h"
#include "frameext.h"
#include "bapp.h"
#include "integration.h"
#include "menuext.h"

using namespace ctrl_ext;

//..............................................................................
TTreeView::TTreeView(wxWindow* Parent, wxWindowID id,
const wxPoint& pos, const wxSize& size, long flags)
: wxTreeCtrl(Parent, id, pos, size, flags),
  AOlxCtrl(this),
  OnSelect(AOlxCtrl::ActionQueue::New(Actions, evt_on_select_id)),
  OnDblClick(AOlxCtrl::ActionQueue::New(Actions, evt_on_dbl_click_id)),
  OnEdit(AOlxCtrl::ActionQueue::New(Actions, evt_change_id))
{
  Bind(wxEVT_TREE_ITEM_ACTIVATED, &TTreeView::ItemActivateEvent, this);
  Bind(wxEVT_TREE_SEL_CHANGED, &TTreeView::SelectionEvent, this);
  Bind(wxEVT_TREE_END_LABEL_EDIT, &TTreeView::ItemEditEvent, this);
  Bind(wxEVT_TREE_ITEM_MENU, &TTreeView::ShowContextMenu, this);

  Bind(wxEVT_LEFT_UP, &TTreeView::OnMouseUp, this);
  Bind(wxEVT_RIGHT_UP, &TTreeView::OnMouseUp, this);
}
//..............................................................................
void TTreeView::ItemActivateEvent(wxTreeEvent& event) {
  event.Skip();
  OnDblClick.Execute(this);
}
//..............................................................................
void TTreeView::SelectionEvent(wxTreeEvent& event) {
  event.Skip();
  OnSelect.Execute(this);
}
//..............................................................................
void TTreeView::ItemEditEvent(wxTreeEvent& event) {
  event.Skip();
  olxstr d = olxstr(OnEdit.data).Replace("~label~", event.GetLabel());
  OnEdit.Execute(this, &d);
}
//..............................................................................
void TTreeView::ShowContextMenu(wxCommandEvent& event) {
  if (contextMenu.ok()) {
    wxTreeEvent* te = dynamic_cast<wxTreeEvent*>(&event);
    if (te != 0) {
      wxTreeItemId ti = te->GetItem();
      if (ti.IsOk()) {
        SelectItem(ti, true);
      }
      PopupMenu(&contextMenu);
    }
  }
}
//..............................................................................
size_t TTreeView::ReadStrings(size_t& index, const wxTreeItemId* thisCaller,
  const TStrList& strings)
{
  while ((index + 2) <= strings.Count()) {
    size_t level = strings[index].LeadingCharCount('\t');
    index++;  // now index is on data string
    wxTreeItemId item;
    if (strings[index].Trim('\t').IsEmpty()) {
      item = AppendItem(*thisCaller, olxstr(strings[index - 1]).Trim('\t').u_str());
    }
    else {
      item = AppendItem(*thisCaller, olxstr(strings[index - 1]).Trim('\t').u_str(), -1, -1,
        new TTreeNodeData(new olxstr(strings[index])));
    }
    index++;  // and now on the next item
    if (index < strings.Count()) {
      size_t nextlevel = strings[index].LeadingCharCount('\t');
      if (nextlevel > level) {
        size_t slevel = ReadStrings(index, &item, strings);
        if (slevel != level) {
          return slevel;
        }
      }
      if (nextlevel < level) {
        return  nextlevel;
      }
    }
  }
  return 0;
}
//..............................................................................
void TTreeView::ClearData()  {
  return;
}
//..............................................................................
bool TTreeView::LoadFromStrings(const TStrList &strings)  {
  ClearData();
  DeleteAllItems();
  wxTreeItemId Root = AddRoot(wxT("Root"));
  size_t index = 0;
  ReadStrings(index, &Root, strings);
  return true;
}
//..............................................................................
void TTreeView::OnMouseUp(wxMouseEvent& me)  {
  me.Skip();
}
//..............................................................................
void TTreeView::OnItemContextMenu(wxCommandEvent& evt) {
  if (evt.GetId() == ID_TREE_ExpandAll) {
    ExpandAllChildren(GetSelection());
  }
  else if (evt.GetId() == ID_TREE_CollapseAll) {
    CollapseAllChildren(GetSelection());
  }
  else if (contextMenu.ok()) {
    const TMenu::MenuData* md = dynamic_cast<TMenu::MenuData*>(
      contextMenu->GetClientObject());
    if (md != 0) {
      olxstr cmd = md->macros.Find(evt.GetId(), EmptyString());
      if (!cmd.IsEmpty()) {
        wxTreeItemId  x = GetSelection();
        if (x.IsOk()) {
          wxTreeItemData* td = this->GetItemData(x);
          TTreeNodeData* olx_td = dynamic_cast<TTreeNodeData*>(td);
          if (olx_td != 0 && olx_td->GetData() != 0) {
            TBasicApp::NewLogEntry() << olx_td->GetData()->ToString();
          }
        }
        olex2::IOlex2Processor::GetInstance()->processMacro(cmd);
      }
    }
  }
}
//..............................................................................
size_t TTreeView::_SaveState(TEBitArray& res, const wxTreeItemId& item,
  size_t& counter) const
{
  size_t selected = InvalidIndex;
  if (IsExpanded(item)) {
    res.SetTrue(counter);
  }
  if (IsSelected(item)) {
    selected = counter;
  }
  wxTreeItemIdValue cookie;
  wxTreeItemId ch_id = GetFirstChild(item, cookie);
  while (ch_id.IsOk()) {
    counter++;
    if (HasChildren(ch_id)) {
      size_t sel = _SaveState(res, ch_id, counter);
      if (sel != InvalidIndex) {
        selected = sel;
      }
    }
    else {
      res.SetFalse(counter);
      if (IsSelected(ch_id)) {
        selected = counter;
      }
    }
    ch_id = GetNextChild(item, cookie);
  }
  return selected;
}
//..............................................................................
olxstr TTreeView::SaveState() const {
  wxTreeItemId root = GetRootItem();
  if ((GetWindowStyle()&wxTR_HIDE_ROOT) != 0) {
    wxTreeItemIdValue cookie;
    root = GetFirstChild(root, cookie);
  }
  TEBitArray res(GetChildrenCount(root, true)+1);
  size_t counter = 0;
  size_t selected = _SaveState(res, root, counter);
  olxstr rv = res.ToBase64String();
  return (rv.stream(';') << selected << ';' << GetScrollPos(wxVERTICAL) <<
    GetScrollPos(wxHORIZONTAL));
}
//..............................................................................
void TTreeView::_RestoreState(const TEBitArray& res, const wxTreeItemId& item,
  size_t& counter, size_t selected)
{
  if (res[counter]) {
    Expand(item);
  }
  if (selected == counter) {
    SelectItem(item, true);
  }
  wxTreeItemIdValue cookie;
  wxTreeItemId ch_id = GetFirstChild(item, cookie);
  while (ch_id.IsOk()) {
    counter++;
    if (HasChildren(ch_id)) {
      _RestoreState(res, ch_id, counter, selected);
    }
    else {
      if (counter == selected) {
        SelectItem(ch_id, true);
      }
    }
    ch_id = GetNextChild(item, cookie);
  }
}
//..............................................................................
void TTreeView::RestoreState(const olxstr& state) {
  TStrList toks(state, ';');
  if (toks.Count() != 4) {
    return;
  }
  wxTreeItemId root = GetRootItem();
  if ((GetWindowStyle()&wxTR_HIDE_ROOT) != 0) {
    wxTreeItemIdValue cookie;
    root = GetFirstChild(root, cookie);
  }
  TEBitArray res;
  res.FromBase64String(toks[0]);
  if (res.Count() != GetChildrenCount(root, true) + 1) {
    return;
  }
  size_t counter = 0;
  size_t selected = toks[1].ToSizeT();
  Freeze();
  OnSelect.SetEnabled(false);
  _RestoreState(res, root, counter, selected);
  //SelectItem(GetSelection(), true);
  OnSelect.SetEnabled(true);
  SetScrollPos(wxVERTICAL, toks[2].ToInt(), false);
  Thaw();
  SetScrollPos(wxHORIZONTAL, toks[3].ToInt(), true);
}
//..............................................................................
wxTreeItemId TTreeView::_FindByLabel(const wxTreeItemId& root,
  const olxstr& label) const
{
  if (label == GetItemText(root)) {
    return root;
  }
  if (!HasChildren(root)) {
    return wxTreeItemId();
  }
  wxTreeItemIdValue cookie;
  wxTreeItemId ch_id = GetFirstChild(root, cookie);
  while (ch_id.IsOk()) {
    wxTreeItemId id = _FindByLabel(ch_id, label);
    if (id.IsOk()) {
      return id;
    }
    ch_id = GetNextChild(root, cookie);
  }
  return ch_id;
}
//..............................................................................
wxTreeItemId TTreeView::_FindByData(const wxTreeItemId& root,
  const olxstr& data) const
{
  wxTreeItemData* _dt = GetItemData(root);
  if (_dt != 0 && olx_type<TTreeNodeData>::check(*_dt)) {
    TTreeNodeData* dt = (TTreeNodeData*)_dt;
    if (dt->GetData() != 0 && data == dt->GetData()->ToString()) {
      return root;
    }
  }
  wxTreeItemIdValue cookie;
  wxTreeItemId ch_id = GetFirstChild(root, cookie);
  while (ch_id.IsOk()) {
    wxTreeItemId id = _FindByData(ch_id, data);
    if (id.IsOk()) {
      return id;
    }
    ch_id = GetNextChild(root, cookie);
  }
  return ch_id;
}
//..............................................................................
void TTreeView::SelectByLabel(const olxstr& label) {
  wxTreeItemId item = _FindByLabel(GetRootItem(), label);
  if (item.IsOk()) {
    OnSelect.SetEnabled(false);
    SelectItem(item);
    OnSelect.SetEnabled(true);
  }
}
//..............................................................................
void TTreeView::SelectByData(const olxstr& data) {
  wxTreeItemId item = _FindByData(GetRootItem(), data);
  if (item.IsOk()) {
    OnSelect.SetEnabled(false);
    SelectItem(item);
    OnSelect.SetEnabled(true);
  }
}
//..............................................................................
void TTreeView::SetContextMenu(wxMenu* menu) {
  contextMenu = menu;
  if (menu != 0) {
    wxMenuItemList& items = menu->GetMenuItems();
    for (size_t i = 0; i < items.size(); i++) {
      Bind(wxEVT_MENU, &TTreeView::OnItemContextMenu, this, items[i]->GetId());
    }
  }
}
//..............................................................................
//..............................................................................
//..............................................................................
wxMenu* TTreeView::CreateDefaultContextMenu() {
  wxMenu* menu = new wxMenu;
  menu->Append(ID_TREE_ExpandAll, wxT("Expand all"));
  menu->Append(ID_TREE_CollapseAll, wxT("Collapse all"));
  return menu;
}
//..............................................................................
wxMenu* TTreeView::CreateContextMenu(const olxstr& def) {
  TStrList items(def, ';');
  if (items.IsEmpty()) {
    return 0;
  }
  olx_object_ptr<wxMenu> menu = CreateDefaultContextMenu();
  return TMenu::CreateMenu(def, ID_TREE_LAST, menu.release());
}
//..............................................................................

#endif // native ordinary navigation branch
