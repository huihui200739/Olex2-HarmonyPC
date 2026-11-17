/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/
#if defined(__OHOS__) && !defined(__WXWIDGETS__)
#ifndef __olx_xglCanvas_H
#define __olx_xglCanvas_H
#include "gxapp.h"
#include <cstdint>

// Native-surface event bridge for the original TGXApp. The caller owns the
// ArkUI/EGL surface and must make its context current before Render/Resize or
// pointer dispatch. The host translates menu, wheel, and key events to ArkUI.
enum class TGlCanvasEventKind {
  PointerDown, PointerUp, PointerMove, PointerCancel, ContextMenu,
  ObjectChanged, Wheel, DoubleClick, KeyDown, KeyUp, Character, Resize
};
struct TGlCanvasEvent {
  TGlCanvasEventKind kind;
  int x=0, y=0, screenX=0, screenY=0;
  short modifiers=0, buttons=0;
  double wheelDelta=0;
  uint32_t keyCode=0, unicode=0;
  bool shift=false, control=false, alt=false, meta=false;
  class AGDrawObject* object=nullptr; // borrowed until Dispatch returns
};
class TGlCanvasHost {
public:
  virtual ~TGlCanvasHost()=default;
  virtual bool Dispatch(const TGlCanvasEvent& event)=0;
};
class TGlCanvas {
  TGXApp* app_=nullptr;              // borrowed original application
  TGlCanvasHost* host_=nullptr;      // borrowed ArkUI event bridge
  int pressX_=0, pressY_=0;
  short buttons_=0;
  bool hasPress_=false;
  bool Send(TGlCanvasEvent event) const;
  void PublishMouseObject();
public:
  TGlCanvas(TGXApp& app,TGlCanvasHost& host);
  ~TGlCanvas()=default; // surface/context ownership remains with Harmony host
  TGlCanvas(const TGlCanvas&)=delete;
  TGlCanvas& operator=(const TGlCanvas&)=delete;
  TGXApp* GetXApp() const { return app_; }
  void XApp(TGXApp& app);
  uint64_t Render();
  uint64_t Resize(int width,int height);
  bool PointerDown(int x,int y,short modifiers,short pressedButtons,int screenX,int screenY);
  bool PointerUp(int x,int y,short modifiers,short remainingButtons,int screenX,int screenY);
  bool PointerMove(int x,int y,short modifiers,int screenX,int screenY);
  void PointerCancel(int x,int y,short modifiers=0);
  bool Wheel(int x,int y,double delta,short modifiers=0);
  bool DoubleClick(int x,int y,short modifiers=0);
  bool Key(uint32_t keyCode,uint32_t unicode,bool down,bool character,
    bool shift,bool control,bool alt,bool meta);
};
#endif // __olx_xglCanvas_H
#else
#ifndef __olx_xglCanvas_H
#define __olx_xglCanvas_H
#include "gxapp.h"
#include "wx/wx.h"
#include "wx/glcanvas.h"

class TGlCanvas: public wxGLCanvas  {
private:
  class TGXApp *FXApp;
  void OnMouseDown(wxMouseEvent& event);
  void OnMouseUp(wxMouseEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnMouseDblClick(wxMouseEvent& event);
  void OnMouseCaptureChange(wxMouseCaptureChangedEvent& event);
  void OnMouseWheel(wxMouseEvent& event);

  void OnKeyUp(wxKeyEvent& event);
  void OnKeyDown(wxKeyEvent& event);
  void OnChar(wxKeyEvent& event);

  int FMX, FMY; // mouse coordinates to process clicks
  short MouseButton;
  class TMainForm *FParent;
  wxGLContext* Context;
  short EncodeEvent(const wxMouseState &evt, bool update_button=true);
public:
  TGlCanvas(TMainForm *parent, int* gl_attr, const wxWindowID id = -1,
    const wxPoint& pos = wxDefaultPosition,
    const wxSize& size = wxDefaultSize, long style = 0,
    const wxString& name = wxT("TGlCanvas"));
  ~TGlCanvas();

  void OnPaint(wxPaintEvent& event);
  void OnEraseBackground(wxEraseEvent& event);
  void OnMouse(wxMouseEvent& event);
  void XApp(TGXApp *XA);
  TGXApp *GetXApp() { return FXApp; }

  void Render();
  /* If default is true - NULL is returned,
  else if stereo is true - Olex2 will try to initialise stereo buffers
  */
  static olx_array_ptr<int> GetGlAttributes(bool _default, bool stereo,
    bool multisampling, short depth_bits);
  DECLARE_CLASS(wxGLCanvas)
};
#endif
#endif // Harmony native surface bridge / original wxGLCanvas
