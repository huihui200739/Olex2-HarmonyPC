/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#ifndef __olx_ctrl_frameext_H
#define __olx_ctrl_frameext_H
#include "olxctrlbase.h"
#include "native_dialog.h"
#include "frame_platform.h"
#include <map>
#include <vector>
namespace ctrl_ext {
// The Widget is window CONTENT in vp. FramePlatform addresses the real system
// window in physical pixels. They are intentionally not interchangeable.
class TMainFrame : public olex2::harmony::ui::Widget, public AOlxCtrl {
  struct State;
  std::shared_ptr<State> state_;
  static thread_local std::vector<TMainFrame*> MainFrames;
  std::map<olxstr, olex2::harmony::ui::FrameRectPx> WindowPos;
  std::map<olxstr, olex2::harmony::ui::DialogGeometry> DialogPos;
  void Begin(const std::function<void(olex2::harmony::ui::FrameCompletion)>&,
    olex2::harmony::ui::FrameCompletion);
protected:
  olxstr PortableFilter(const olxstr&);
public:
  TMainFrame(olex2::harmony::ui::NodeBackend&,
    std::shared_ptr<olex2::harmony::ui::FramePlatform>,
    std::shared_ptr<olex2::harmony::ui::DialogBackend>,
    std::shared_ptr<olex2::harmony::ui::UiScheduler>,
    const olxstr& title, const olxstr& className, olex2::harmony::ui::Size contentSizeVp);
  ~TMainFrame() override;
  olex2::harmony::ui::FrameRectPx ReadWindowRect() const;
  void SavePosition(TMainFrame&);
  void RestorePosition(TMainFrame&, olex2::harmony::ui::FrameCompletion);
  void SavePosition(const olxstr& name, olex2::harmony::ui::DialogWindow&);
  void RestorePosition(const olxstr& name, olex2::harmony::ui::DialogWindow&);
  void MoveWindow(int leftPx, int topPx, olex2::harmony::ui::FrameCompletion);
  void ResizeWindow(int widthPx, int heightPx, olex2::harmony::ui::FrameCompletion);
  void SetWindowTitle(const olxstr&, olex2::harmony::ui::FrameCompletion);
  void PickFile(const olxstr& caption, const olxstr& filter, const olxstr& folderUri,
    const olxstr& fileName, bool open, olex2::harmony::ui::FrameCompletion);
  bool HasPendingRequests() const;
  bool HasAlert() const;
  void CloseAlert(int result);
  void SilenceCompletions(); // Environment teardown only; no user callbacks.
  virtual void SetScenesFolder(const olxstr&) = 0;
  virtual const olxstr& GetScenesFolder() const = 0;
  static TMainFrame& GetMainFrameInstance();
  void Alert(const olxstr& message, const olxstr& title, uint32_t flags,
    olex2::harmony::ui::FrameCompletion);
  static void ShowAlert(const olxstr& message, const olxstr& title, uint32_t flags,
    olex2::harmony::ui::FrameCompletion);
  static void ShowAlert(const TExceptionBase&, const olxstr& message, bool log,
    olex2::harmony::ui::FrameCompletion);
};
}
#endif

#else
#ifndef __olx_ctrl_frameext_H
#define __olx_ctrl_frameext_H
#include "olxctrlbase.h"
#include "edict.h"
#include "dataitem.h"
#include "gllightmodel.h"

namespace ctrl_ext  {
  class TMainFrame: public wxFrame, public AOlxCtrl  {
    static TMainFrame* MainFrameInstance;
  protected:
    struct TWindowInfo  {  int x, y;  };
    olxstr_dict<TWindowInfo> WindowPos;
    // extends filter for case sensitive OS
    olxstr PortableFilter(const olxstr& filter);
  public:
    TMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size,
      const wxString &ClassName)
    : wxFrame((wxFrame*)NULL, wxID_ANY, title, pos, size, wxDEFAULT_FRAME_STYLE),
      AOlxCtrl(this)
    {
      MainFrameInstance = this;
    }

    virtual ~TMainFrame() {  MainFrameInstance = NULL;  }
    // restores previously saved position
    void RestorePosition(wxWindow *Window);
    //saves current position of the window on screen
    void SavePosition(wxWindow *Window);
    olxstr PickFile(const olxstr &Caption, const olxstr &Filter,
      const olxstr &DefFolder, const olxstr &DefFile, bool Open);
    virtual void SetScenesFolder(const olxstr& sf) = 0;
    virtual const olxstr& GetScenesFolder() const = 0;

    static TMainFrame& GetMainFrameInstance() {  return *MainFrameInstance;  }
    static int ShowAlert(const olxstr &msg, const olxstr &title, int flags);
    static void ShowAlert(const TExceptionBase &esc,
      const olxstr &msg=EmptyString(), bool log=true);
    DECLARE_CLASS(TMainFrame)
  };
};  // end namespace ctrl_ext
#endif

#endif // native main frame / original wx
