/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#pragma once
#include "actions.h"
#include "ctrls/olxctrlbase.h"
#include "native_dialog.h"
class olxCommandEvent:public IOlxObject {
  olxstr cmd;int id;
public:
  olxCommandEvent(const olxCommandEvent&)=default;
  explicit olxCommandEvent(const olxstr& command,int eventId=-1):cmd(command),id(eventId){}
  olxCommandEvent* Clone()const{return new olxCommandEvent(*this);}
  const olxstr& GetCommand()const{return cmd;}
  void SetCommand(const olxstr& command){cmd=command;}
  int GetId()const{return id;}
  void SetId(int value){id=value;}
};
class olxCommandAction:public IOlxAction {
  olxstr cmd;
public:
  explicit olxCommandAction(const olxstr& command):cmd(command){}
  bool Run()override;
};
namespace ctrl_ext {
class TDialog:public olex2::harmony::ui::DialogWindow,public AOlxCtrl {
  TActionQList DialogActions;
public:
  TDialog(olex2::harmony::ui::NodeBackend&,std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,olex2::harmony::ui::Widget* parent,
    const olxstr& title,const olxstr& className,olex2::harmony::ui::DialogGeometry geometry={},
    std::shared_ptr<olex2::harmony::ui::DialogPositionStore> positions={},bool resizable=true);
  ~TDialog()override;
  TActionQueue& OnResize;
};
class TTimer:public IOlxObject {
  struct State;std::shared_ptr<State> state_;
  TActionQList Actions;
public:
  explicit TTimer(std::shared_ptr<olex2::harmony::ui::UiScheduler>);
  ~TTimer()override;
  bool Start(int milliseconds=-1,bool oneShot=false);
  void Stop();
  bool IsRunning()const;
  bool IsOneShot()const;
  int GetInterval()const;
  TActionQueue& OnTimer;
};
}
using namespace ctrl_ext;

#else
#ifndef __olx_ctrl_H
#define __olx_ctrl_H
#include "actions.h"
#include "ctrls/olxctrlbase.h"
#include "wx/stc/stc.h"
#include "wx/wx.h"

class olxCommandEvent;
wxDECLARE_EVENT(OLX_COMMAND_EVT, olxCommandEvent);
class olxCommandEvent : public wxEvent {
  olxstr cmd;
public:
  olxCommandEvent(const olxCommandEvent &e)
    : wxEvent(e), cmd(e.cmd)
  {}
  olxCommandEvent(const olxstr &cmd, int id=-1)
    : wxEvent(id, OLX_COMMAND_EVT), cmd(cmd)
  {}
  wxEvent *Clone() const { return new olxCommandEvent(*this); }
  const olxstr &GetCommand() const { return cmd; }
  void SetCommand(const olxstr &c) { cmd = c; }
};

class olxCommandAction : public IOlxAction {
  olxstr cmd;
public:
  olxCommandAction(const olxstr &cmd)
    : cmd(cmd)
  {}
  bool Run();
};

namespace ctrl_ext  {

  class TDialog: public wxDialog, public AOlxCtrl {
    void OnSizeEvt(wxSizeEvent& event);
  protected:
    class TMainFrame *Parent;
    TActionQList Actions;
    bool manage_parent;
  public:
    TDialog(TMainFrame *Parent,
      const wxString &Title,
      const wxString &ClassName,
      const wxPoint& position = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      int style=wxRESIZE_BORDER|wxDEFAULT_DIALOG_STYLE);
    TDialog(wxWindow *Parent,
      const wxString &Title,
      const wxString &ClassName,
      const wxPoint& position = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      int style=wxRESIZE_BORDER|wxDEFAULT_DIALOG_STYLE);
    virtual ~TDialog();
    virtual bool Show(bool show=true);
    /* extended version, if manage_parent is true, the parent window is
    disabled before the execution and is enabled before hiding this window
    */
    int ShowModalEx(bool manage_parent);
    TActionQueue &OnResize;
    DECLARE_CLASS(TDialog)
  };

  class TTimer: public wxTimer, public IOlxObject  {
    void Notify()  {  OnTimer.Execute(this, NULL);  }
    TActionQList Actions;
  public:
    TTimer() : OnTimer(Actions.New("ONTIMER"))  {}
    virtual ~TTimer()  {}

    TActionQueue &OnTimer;
    DECLARE_CLASS(TTimer)
  };
};  // end of the _xl_Controls namespace
using namespace ctrl_ext;
#include "ctrls/frameext.h"
#include "ctrls/menuext.h"
#include "ctrls/labelext.h"
#include "ctrls/comboext.h"
#include "ctrls/choiceext.h"
#include "ctrls/texteditext.h"
#include "ctrls/treeviewext.h"
#include "ctrls/trackbarext.h"
#include "ctrls/btnext.h"
#include "ctrls/spinctrlext.h"
#include "ctrls/listboxext.h"
#include "ctrls/checkboxext.h"
#include "ctrls/datectrlext.h"
#include "ctrls/colorext.h"

#endif

#endif // native dialog/timer / original wx
