/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#ifndef __olx_ctrl_treeview_H
#define __olx_ctrl_treeview_H
#include "olxctrlbase.h"
#include "estrlist.h"
#include "bitarray.h"
#include "menuext.h"
#include <map>
#include <memory>
#include <vector>
namespace ctrl_ext {
enum {ID_TREE_ExpandAll=1000,ID_TREE_CollapseAll,ID_TREE_LAST};
constexpr long TreeHideRoot=1,TreeEditLabels=2;
class TTreeView;
struct TreeItemId {
  size_t value=0;
  const TTreeView* owner=nullptr;
  bool IsOk()const{return value!=0;}
  bool operator==(TreeItemId other)const{return value==other.value&&owner==other.owner;}
  bool operator!=(TreeItemId other)const{return !(*this==other);}
};
class TTreeNodeData:public IOlxObject {
  std::unique_ptr<IOlxObject> Data;
public:
  explicit TTreeNodeData(IOlxObject* data):Data(data){}
  IOlxObject* GetData()const{return Data.get();}
};
class TTreeView:public olex2::harmony::ui::Widget,public AOlxCtrl {
  struct Node {
    size_t id=0,parent=0;
    olxstr label;
    bool expanded=false;
    std::vector<size_t> children;
    std::unique_ptr<TTreeNodeData> data;
  };
  std::map<size_t,std::unique_ptr<Node>> nodes_;
  size_t root_=0,selected_=0,next_=1,editing_=0;
  std::vector<size_t> visible_;
  olxstr Data;
  std::unique_ptr<TMenu> contextMenu;
  Node& Find(TreeItemId)const;
  std::vector<size_t> All(size_t root)const;
  void Sync();
  void SetExpanded(TreeItemId,bool,bool descendants=false);
  void ContextAction(int);
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
public:
  TTreeView(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget*,int id=-1,
    olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long flags=0);
  ~TTreeView()override;
  DefPropC(olxstr,Data)
  TreeItemId AddRoot(const olxstr& label);
  TreeItemId AppendItem(TreeItemId,const olxstr&,TTreeNodeData* data=nullptr);
  void DeleteAllItems();
  void Delete(TreeItemId);
  TreeItemId GetRootItem()const{return {root_,this};}
  TreeItemId GetSelection()const{return {selected_,this};}
  TreeItemId GetItemParent(TreeItemId id)const{return {Find(id).parent,this};}
  const olxstr& GetItemText(TreeItemId id)const{return Find(id).label;}
  void SetItemText(TreeItemId,const olxstr&);
  TTreeNodeData* GetItemData(TreeItemId id)const{return Find(id).data.get();}
  void SetItemData(TreeItemId id,TTreeNodeData* data){AssertThread();auto& stored=Find(id).data;if(stored.get()!=data)stored.reset(data);}
  bool HasChildren(TreeItemId id)const{return !Find(id).children.empty();}
  bool IsExpanded(TreeItemId id)const{return Find(id).expanded;}
  bool IsSelected(TreeItemId id)const{Find(id);return id.value==selected_;}
  size_t GetChildrenCount(TreeItemId,bool recursive=true)const;
  std::vector<TreeItemId> Children(TreeItemId)const;
  void Expand(TreeItemId id){SetExpanded(id,true);}
  void Collapse(TreeItemId id){SetExpanded(id,false);}
  void ExpandAllChildren(TreeItemId id){SetExpanded(id,true,true);}
  void CollapseAllChildren(TreeItemId id){SetExpanded(id,false,true);}
  void SelectItem(TreeItemId,bool select=true);
  void SelectByLabel(const olxstr&);
  void SelectByData(const olxstr&);
  bool LoadFromStrings(const TStrList&);
  olxstr SaveState()const;
  void RestoreState(const olxstr&);
  void BeginLabelEdit(TreeItemId);
  void CancelLabelEdit();
  void SetContextMenu(TMenu*);
  void ShowContextMenu();
  static TMenu* CreateDefaultContextMenu();
  static TMenu* CreateContextMenu(const olxstr&);
  void SetWindowStyle(long)override;
  ActionQueue &OnDblClick,&OnSelect,&OnEdit;
};
}
#endif

#else
#ifndef __olx_ctrl_treeview_H
#define __olx_ctrl_treeview_H
#include "olxctrlbase.h"
#include "estrlist.h"
#include "bitarray.h"
#include "wx/treectrl.h"
#include "wx/generic/treectlg.h"

namespace ctrl_ext  {
  enum {
    ID_TREE_ExpandAll = 1000,
    ID_TREE_CollapseAll,
    ID_TREE_LAST, // custom menu ids start here
  };

  class TTreeNodeData : public wxTreeItemData, public IOlxObject {
    IOlxObject* Data;
  public:
    TTreeNodeData(IOlxObject* obj) {  Data = obj;  }
    virtual ~TTreeNodeData()  { delete Data;  }
    inline IOlxObject* GetData() const {  return Data;  }
  };

  class TTreeView: public wxTreeCtrl, public AOlxCtrl  {
    olxstr Data;
  protected:
    void SelectionEvent(wxTreeEvent& event);
    void ItemActivateEvent(wxTreeEvent& event);
    void ItemEditEvent(wxTreeEvent& event);
    void ShowContextMenu(wxCommandEvent& event);
    void OnItemContextMenu(wxCommandEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    size_t ReadStrings(size_t& index, const wxTreeItemId* thisCaller,
      const TStrList& strings);
    void ClearData();
    olx_object_ptr<wxMenu> contextMenu;
    // returns index of the selected item...
    size_t _SaveState(TEBitArray& res, const wxTreeItemId& item,
      size_t& counter) const;
    void _RestoreState(const TEBitArray& res, const wxTreeItemId& item,
      size_t& counter, size_t selected);
    wxTreeItemId _FindByLabel(const wxTreeItemId& root,
      const olxstr& label) const;
    wxTreeItemId _FindByData(const wxTreeItemId& root,
      const olxstr& data) const;
  public:
    TTreeView(wxWindow* Parent, wxWindowID id = -1,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      long flags = (wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT));

    virtual ~TTreeView()  {
      ClearData();
    }

    DefPropC(olxstr, Data);
    void SetContextMenu(wxMenu*);

    void SelectByLabel(const olxstr& label);
    void SelectByData(const olxstr& data);

    bool LoadFromStrings(const TStrList& strings);
    olxstr SaveState() const;
    void RestoreState(const olxstr& state);

    AOlxCtrl::ActionQueue &OnDblClick, &OnSelect, &OnEdit;

    static wxMenu* CreateDefaultContextMenu();
    static wxMenu* CreateContextMenu(const olxstr& def);
  };
}; // end namespace ctrl_ext
#endif

#endif // native ordinary navigation branch
