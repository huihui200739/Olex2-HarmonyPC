/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#ifndef __olx_ctrl_menu_H
#define __olx_ctrl_menu_H
#include "olxctrlbase.h"
#include "native_menu.h"
#include <memory>
#include <thread>
#include <vector>
namespace ctrl_ext {
const short mtSeparator=-1,mtNormalItem=0,mtCheckItem=1,mtRadioItem=2;
class TMenuItem;
class TMenu:public IOlxObject {
  std::vector<std::unique_ptr<TMenuItem>> items_;
  olxstr name_;
  std::shared_ptr<olex2::harmony::ui::MenuPopup> popup_;
  std::shared_ptr<int> session_;
  std::weak_ptr<void> ownerLife_;
  std::shared_ptr<int> life_=std::make_shared<int>(0);
  std::thread::id thread_=std::this_thread::get_id();
  TMenu* parent_=nullptr;
  olex2::harmony::ui::Widget* owner_=nullptr;
  std::function<void(int)> selected_;
  std::string error_;
  void OpenLevel(TMenu* level);
  void Choose(TMenu* level,int position);
  void AssertThread()const;
public:
  class MenuData:public IOlxObject {public:olx_pdict<int,olxstr> macros;};
private:
  std::unique_ptr<MenuData> data_;
public:
  explicit TMenu(const olxstr& name=EmptyString(),std::shared_ptr<olex2::harmony::ui::MenuPopup> popup={});
  ~TMenu()override;
  TMenu(const TMenu&)=delete;
  TMenu& operator=(const TMenu&)=delete;
  TMenu* Clone()const{return CopyMenu(*this);}
  static TMenu* CopyMenu(const TMenu&);
  static TMenu* CreateMenu(const olxstr&,int first_id=0,TMenu* menu=nullptr);
  TMenuItem* Append(int id,const olxstr& label,const olxstr& help=EmptyString(),short kind=mtNormalItem);
  TMenuItem* Append(int id,const olxstr& label,TMenu* submenu,const olxstr& help=EmptyString());
  TMenuItem* Append(TMenuItem*);
  TMenuItem* AppendSeparator();
  bool Destroy(TMenuItem*);
  void Clear();
  size_t GetMenuItemCount()const{return items_.size();}
  TMenuItem* FindItemByPosition(size_t index)const;
  TMenuItem* FindItem(int id)const;
  MenuData* GetClientObject()const{return data_.get();}
  void SetClientObject(MenuData* data){AssertThread();data_.reset(data);}
  void Popup(olex2::harmony::ui::Widget* owner,std::function<void(int)> selected);
  void Close();
  bool Activate(int id);
  const std::string& LastPopupError()const{return error_;}
  const olxstr& GetTitle()const{return name_;}
  friend class TMenuItem;
};
class TMenuItem:public AActionHandler {
  TActionQList Actions;
  TActionQueue* ActionQueue=nullptr;
  olxstr DependMode,Command,OnModeChangeCmd,label_,help_;
  short DependentOn=0,kind_;
  int id_;
  TMenu* parent_=nullptr;
  bool enabled_=true,checked_=false;
  std::unique_ptr<TMenu> submenu_;
public:
  TMenuItem(short type,int id,TMenu* parent=nullptr,const olxstr& name=EmptyString());
  ~TMenuItem()override;
  void SetActionQueue(TActionQueue&,const olxstr&,short);
  void OnRemove(TActionQueue* caller=nullptr)override {if(!caller||caller==ActionQueue)ActionQueue=nullptr;}
  bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;
  void ValidateState();
  int GetId()const{return id_;}
  short GetKind()const{return kind_;}
  bool IsSeparator()const{return kind_==mtSeparator;}
  bool IsCheckable()const{return kind_==mtCheckItem||kind_==mtRadioItem;}
  bool IsChecked()const{return checked_;}
  bool IsEnabled()const{return enabled_;}
  void Check(bool=true);
  void Enable(bool value=true){enabled_=value;}
  const olxstr& GetItemLabel()const{return label_;}
  void SetItemLabel(const olxstr& label){label_=label;}
  const olxstr& GetHelp()const{return help_;}
  void SetHelp(const olxstr& help){help_=help;}
  TMenu* GetSubMenu()const{return submenu_.get();}
  DefPropC(olxstr,Command)
  TActionQueue& OnModeChange;
  static const short ModeDependent=1,StateDependent=2;
  friend class TMenu;
};
}
#endif

#else
#ifndef __olx_ctrl_menu_H
#define __olx_ctrl_menu_H
#include "olxctrlbase.h"
#include "actions.h"
#include "wx/wx.h"

namespace ctrl_ext {

  const short mtSeparator = wxITEM_SEPARATOR,
    mtNormalItem = wxITEM_NORMAL,
    mtCheckItem = wxITEM_CHECK,
    mtRadioItem = wxITEM_RADIO;

  class TMenu : public wxMenu, public IOlxObject {
  public:
    TMenu(const olxstr& Name = EmptyString()) : wxMenu(Name.u_str()) {}
    // the function creates a new clone of wxMenu
    TMenu* Clone() const { return CopyMenu(*this); };
    // returns a new menu created with new
    static TMenu* CopyMenu(const wxMenu& menu);
    void Clear(); // empties the menu

    class MenuData : public wxClientData {
    public:
      olx_pdict<int, olxstr> macros;
    };
    /* build menu from ';' separated string.'#' - separator, items name<-macro
    * if a menu is given - items appended to is, otherwise a new menu is created.
    * Menu's GetClientObject wil return MenuData to access the macros
    */
    static wxMenu* CreateMenu(const olxstr& def,
      int first_id=0, wxMenu *menu=0);
    DECLARE_CLASS(TMenu);
  };

  class TMenuItem : public wxMenuItem, public AActionHandler {
    TActionQList Actions;
    TActionQueue* ActionQueue;
    olxstr DependMode, Command, OnModeChangeCmd;
    short DependentOn;
  public:
    TMenuItem(const short type, int id, TMenu* parent = 0,
      const olxstr& Name = EmptyString())
      : wxMenuItem(parent, id, Name.u_str(), wxString(),
        static_cast<wxItemKind>(type)),
      OnModeChange(Actions.New(evt_on_mode_change_id)),
      ActionQueue(0),
      DependentOn(0)
    {
      SetToDelete(false);
    }
    virtual ~TMenuItem();
    void SetActionQueue(TActionQueue& q, const olxstr& dependMode,
      short dependentOn);
    bool Execute(const IOlxObject* Sender, const IOlxObject* Data, TActionQueue*);
    // updates checked status
    void ValidateState();
    DefPropC(olxstr, Command)

      TActionQueue& OnModeChange;
    static const short ModeDependent = 1,
      StateDependent = 2;
    DECLARE_CLASS(TMenuItem)
  };
};  // end namespace ctrl_ext
#endif

#endif // native ordinary navigation branch
