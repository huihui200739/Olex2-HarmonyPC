#if defined(__OHOS__) && !defined(__WXWIDGETS__)
#include "xglcanv.h"
#include "glgroup.h"
#include <cmath>
#include <stdexcept>

TGlCanvas::TGlCanvas(TGXApp& app,TGlCanvasHost& host):app_(&app),host_(&host) {}
bool TGlCanvas::Send(TGlCanvasEvent event) const {
  if(host_==nullptr) return false;
  return host_->Dispatch(event);
}
void TGlCanvas::PublishMouseObject() {
  AGDrawObject* object=app_==nullptr?nullptr:app_->GetMouseObject();
  if(object!=nullptr) {
    if(TGlGroup* group=app_->FindObjectGroup(*object)) object=group;
  }
  TGlCanvasEvent event{TGlCanvasEventKind::ObjectChanged};event.object=object;
  Send(event);
}
void TGlCanvas::XApp(TGXApp& app) {
  if(app_!=&app) {
    if(app_!=nullptr&&hasPress_) app_->ResetMouseState(static_cast<short>(pressX_),static_cast<short>(pressY_));
    buttons_=0;hasPress_=false;app_=&app;
  }
}
uint64_t TGlCanvas::Render() {
  if(app_==nullptr) throw std::runtime_error("Native canvas has no original TGXApp");
  return app_->Draw();
}
uint64_t TGlCanvas::Resize(int width,int height) {
  if(app_==nullptr||width<=0||height<=0) throw std::invalid_argument("Native canvas requires a live TGXApp and positive surface dimensions");
  app_->Resize(width,height);
  TGlCanvasEvent event{TGlCanvasEventKind::Resize};event.x=width;event.y=height;Send(event);
  return app_->Draw();
}
bool TGlCanvas::PointerDown(int x,int y,short modifiers,short pressedButtons,int screenX,int screenY) {
  if(app_==nullptr) throw std::runtime_error("Native canvas has no original TGXApp");
  if(pressedButtons==0) throw std::invalid_argument("Native pointer-down requires at least one pressed mouse button");
  buttons_=pressedButtons;pressX_=x;pressY_=y;hasPress_=true;
  TGlCanvasEvent event{TGlCanvasEventKind::PointerDown};event.x=x;event.y=y;event.screenX=screenX;event.screenY=screenY;event.modifiers=modifiers;event.buttons=buttons_;
  const bool handled=Send(event);
  const bool appHandled=app_->MouseDown(x,y,modifiers,buttons_);
  PublishMouseObject();
  return handled||appHandled;
}
bool TGlCanvas::PointerUp(int x,int y,short modifiers,short remainingButtons,int screenX,int screenY) {
  if(app_==nullptr) throw std::runtime_error("Native canvas has no original TGXApp");
  const short released=static_cast<short>(buttons_^remainingButtons);
  AGDrawObject* pressedObject=app_->GetMouseObject();
  TGlCanvasEvent event{TGlCanvasEventKind::PointerUp};event.x=x;event.y=y;event.screenX=screenX;event.screenY=screenY;event.modifiers=modifiers;event.buttons=released;
  bool handled=Send(event);
  if(!handled) handled=app_->MouseUp(x,y,modifiers,released);
  const bool contextClick=hasPress_&&std::abs(x-pressX_)<=4&&std::abs(y-pressY_)<=4&&released==smbRight&&modifiers==0;
  if(!handled&&contextClick) {
    if(pressedObject!=nullptr) if(TGlGroup* group=app_->FindObjectGroup(*pressedObject)) pressedObject=group;
    TGlCanvasEvent context{TGlCanvasEventKind::ContextMenu};context.x=pressX_;context.y=pressY_;context.screenX=screenX;context.screenY=screenY;context.modifiers=modifiers;context.buttons=released;context.object=pressedObject;
    handled=Send(context);
  }
  app_->Draw();
  app_->ResetMouseState(static_cast<short>(x),static_cast<short>(y),modifiers,remainingButtons);
  buttons_=remainingButtons;hasPress_=remainingButtons!=0;
  if(!hasPress_) PublishMouseObject();
  return handled;
}
bool TGlCanvas::PointerMove(int x,int y,short modifiers,int screenX,int screenY) {
  if(app_==nullptr) throw std::runtime_error("Native canvas has no original TGXApp");
  TGlCanvasEvent event{TGlCanvasEventKind::PointerMove};event.x=x;event.y=y;event.screenX=screenX;event.screenY=screenY;event.modifiers=modifiers;event.buttons=buttons_;
  Send(event);
  const bool changed=app_->MouseMove(x,y,modifiers);
  if(changed) app_->Draw();
  return changed;
}
void TGlCanvas::PointerCancel(int x,int y,short modifiers) {
  if(app_==nullptr) return;
  TGlCanvasEvent event{TGlCanvasEventKind::PointerCancel};event.x=x;event.y=y;event.modifiers=modifiers;event.buttons=buttons_;Send(event);
  app_->ResetMouseState(static_cast<short>(x),static_cast<short>(y),modifiers,0);
  buttons_=0;hasPress_=false;PublishMouseObject();
}
bool TGlCanvas::Wheel(int x,int y,double delta,short modifiers) {
  if(!std::isfinite(delta)) throw std::invalid_argument("Native wheel delta must be finite");
  TGlCanvasEvent event{TGlCanvasEventKind::Wheel};event.x=x;event.y=y;event.modifiers=modifiers;event.wheelDelta=delta;
  return Send(event);
}
bool TGlCanvas::DoubleClick(int x,int y,short modifiers) {
  if(app_==nullptr) throw std::runtime_error("Native canvas has no original TGXApp");
  if(app_->DblClick()) return true;
  TGlCanvasEvent event{TGlCanvasEventKind::DoubleClick};event.x=x;event.y=y;event.modifiers=modifiers;event.buttons=buttons_;
  return Send(event);
}
bool TGlCanvas::Key(uint32_t keyCode,uint32_t unicode,bool down,bool character,
    bool shift,bool control,bool alt,bool meta) {
  TGlCanvasEvent event{character?TGlCanvasEventKind::Character:(down?TGlCanvasEventKind::KeyDown:TGlCanvasEventKind::KeyUp)};
  event.keyCode=keyCode;event.unicode=unicode;event.shift=shift;event.control=control;event.alt=alt;event.meta=meta;
  if(control) event.modifiers|=sssCtrl;if(shift) event.modifiers|=sssShift;if(alt) event.modifiers|=sssAlt;
  return Send(event);
}
#else
#include "xglcanv.h"
#include "mainform.h"
#include "glgroup.h"
#include "wxglscene.h"

IMPLEMENT_CLASS(TGlCanvas, wxGLCanvas)
//..............................................................................
TGlCanvas::TGlCanvas(TMainForm *parent, int* gl_attr, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name):
#if wxCHECK_VERSION(2,9,0) || !(defined(__WXX11__) || defined(__MAC__))
  wxGLCanvas(parent, id, gl_attr, pos, size, style, name )  {
  Context = new wxGLContext(this, NULL);
  
// on GTK the context initialisation is delayed
#  if defined(__WIN32__) || defined(__MAC__)
  Context->SetCurrent(*this);
#  endif
#else
  wxGLCanvas(parent, (wxGLCanvas*)NULL, id, pos, size, style|wxFULL_REPAINT_ON_RESIZE, name )  {
  Context = NULL;
#endif
  FXApp = NULL;
  MouseButton = 0;
  FParent = parent;
  Bind(wxEVT_PAINT, &TGlCanvas::OnPaint, this);
  Bind(wxEVT_ERASE_BACKGROUND, &TGlCanvas::OnEraseBackground, this);

  Bind(wxEVT_LEFT_UP, &TGlCanvas::OnMouseUp, this);
  Bind(wxEVT_RIGHT_UP, &TGlCanvas::OnMouseUp, this);
  Bind(wxEVT_LEFT_DOWN, &TGlCanvas::OnMouseDown, this);
  Bind(wxEVT_RIGHT_DOWN, &TGlCanvas::OnMouseDown, this);
  Bind(wxEVT_MOTION, &TGlCanvas::OnMouseMove, this);
  Bind(wxEVT_MOUSE_CAPTURE_CHANGED, &TGlCanvas::OnMouseCaptureChange, this);
  Bind(wxEVT_MOUSEWHEEL, &TGlCanvas::OnMouseWheel, this);
  Bind(wxEVT_LEFT_DCLICK, &TGlCanvas::OnMouseDblClick, this);

  Bind(wxEVT_KEY_UP, &TGlCanvas::OnKeyUp, this);
  Bind(wxEVT_CHAR, &TGlCanvas::OnChar, this);
  Bind(wxEVT_KEY_DOWN, &TGlCanvas::OnKeyDown, this);
  }
//..............................................................................
TGlCanvas::~TGlCanvas() {
  if (TBasicApp::HasInstance()) {
    TwxGlScene *wgls = dynamic_cast<TwxGlScene*>(
      &FXApp->GetRenderer().GetScene());
    if (wgls != 0) {
      wgls->SetCanvas(0);
      wgls->SetContext(0);
    }
  }
  if (Context != 0) {
    delete Context;
  }
}
//..............................................................................
void TGlCanvas::XApp(TGXApp *XA) {
  FXApp = XA;
  TwxGlScene *wgls = dynamic_cast<TwxGlScene*>(&XA->GetRenderer().GetScene());
  if (wgls == NULL) {
    return;
  }
  wgls->SetCanvas(this);
#if wxCHECK_VERSION(2,9,0) || !(defined(__WXX11__) || defined(__MAC__))
  wgls->SetContext(Context);
#else
  wgls->SetContext(GetContext());
#endif
}
//..............................................................................
void TGlCanvas::Render() {
#if !defined(__WXMOTIF__) && !defined(__WIN32__) && !defined(__WXGTK__) && !wxCHECK_VERSION(3,1,0)
  if (GetContext() == 0) {
    return;
  }
#endif
  if (FXApp == 0 || !IsShown()) {
    return;
  }
#if (defined(__WXX11__) || defined(__MAC__))  && !wxCHECK_VERSION(3,1,0)
  SetCurrent();
#else
  Context->SetCurrent(*this);
#endif
  FXApp->Draw();
}
//..............................................................................
void TGlCanvas::OnPaint(wxPaintEvent& event)  {
  Render();
}
//..............................................................................
void TGlCanvas::OnEraseBackground(wxEraseEvent& event)  {
}
//..............................................................................
short TGlCanvas::EncodeEvent(const wxMouseState &evt, bool update_button)  {
  short Fl = 0;
  if (evt.m_altDown) {
    Fl |= sssAlt;
  }
  if (evt.m_shiftDown) {
    Fl |= sssShift;
  }
  if (evt.m_controlDown) {
    Fl |= sssCtrl;
  }
  if (update_button) {
    MouseButton = 0;
    if (evt.LeftIsDown()) {
      MouseButton = smbLeft;
    }
    if (evt.MiddleIsDown()) {
      MouseButton |= smbMiddle;
    }
    if (evt.RightIsDown()) {
      MouseButton |= smbRight;
    }
  }
  return Fl;
}
//..............................................................................
void TGlCanvas::OnMouseDown(wxMouseEvent& me) {
  //short mb = MouseButton;
  short Fl = EncodeEvent(me);
  //MouseButton |= mb;
  FParent->OnMouseDown(me.m_x, me.m_y, Fl, MouseButton);
  FXApp->MouseDown(me.m_x, me.m_y, Fl, MouseButton);
  //AGDrawObject* G = FXApp->SelectObject(me.m_x, me.m_y, false);
  AGDrawObject* G = FXApp->GetMouseObject();
  if (G != 0) {
    TGlGroup* GlG = FXApp->FindObjectGroup(*G);
    if (GlG != 0) {
      G = GlG;
    }
  }
  FParent->ObjectUnderMouse(G);
  FMX = me.m_x;
  FMY = me.m_y;
  SetFocus();
  me.Skip();
}
//..............................................................................
void TGlCanvas::OnMouseUp(wxMouseEvent& me)  {
  me.Skip();
  short mb = MouseButton;
  short Fl = EncodeEvent(me, true);
  short up = MouseButton ^ mb;
  int left = 0, top = 0;
#ifdef __MAC__  // a solution for MAC's single button
  int os_mask = sssCtrl;
#else
  int os_mask = 0;
#endif
  GetPosition(&left, &top);
  // MouseUp resets the previously set object - capture it before that
  AGDrawObject* G = FXApp->GetMouseObject();
  if (FParent->OnMouseUp(me.m_x, me.m_y, Fl, up)) {
  }
  else if (FXApp->MouseUp(me.m_x, me.m_y, Fl, up)) {
  }
  else if ((abs(me.m_x-FMX) <= 4) && (abs(me.m_y-FMY) <= 4) &&
    (up == smbRight) && (Fl == os_mask || Fl == 0))
  {
//    FMY += (wxSystemSettings::GetMetric(wxSYS_MENU_Y)*FParent->pmMenu->GetMenuItemCount());
//      FXApp->MouseUp(me.m_x, me.m_y, Fl, Btn);
    //AGDrawObject *G = FXApp->SelectObject(me.m_x, me.m_y, false);
    bool Handled = false;
    if (G != 0) {
      TGlGroup *GlG = FXApp->FindObjectGroup(*G);
      if (GlG == 0) {
        FParent->ObjectUnderMouse(G);
      }
      else {
        FParent->ObjectUnderMouse(GlG);
      }
      if (FParent->CurrentPopupMenu()) {
        FParent->PopupMenu(FParent->CurrentPopupMenu(), FMX+left, FMY+top);
        Handled = true;
      }
      if (!Handled) {
        FParent->PopupMenu(FParent->DefaultPopup(), FMX + left, FMY + top);
      }
    }
    else {
      FParent->PopupMenu(FParent->GeneralPopup(), FMX + left, FMY + top);
    }
    SetFocus();
    return;
  }
  else if (FParent->OnMouseUp(me.m_x, me.m_y, Fl, up)) {
    FXApp->ResetMouseState(me.m_x, me.m_y);
  }
  FXApp->Draw();
  FXApp->ResetMouseState(me.m_x, me.m_y, Fl, MouseButton);
  MouseButton = 0;
}
//..............................................................................
void TGlCanvas::OnMouseMove(wxMouseEvent& me) {
  short Fl = EncodeEvent(me, false);
  if (MouseButton == 0)
    FParent->OnMouseMove(me.m_x, me.m_y);
  else
    FParent->OnNonIdle();
  // check if a handler for the event is found
  if (FXApp != NULL && FXApp->MouseMove(me.m_x, me.m_y, Fl)) {
#ifdef __WIN32__
    /* this seems to help but not to solve! with the desktop composition on
    Vista/7
    */
    wxWindow::Refresh();
#else
    FXApp->Draw();
#endif
  }
}
//..............................................................................
void TGlCanvas::OnMouseDblClick(wxMouseEvent& me)  {
  short Fl = EncodeEvent(me);
  if (FXApp != NULL && !FXApp->DblClick())
    FParent->OnMouseDblClick(me.m_x, me.m_y, Fl, MouseButton);
}
//..............................................................................
void TGlCanvas::OnMouseWheel(wxMouseEvent& me) {
  if (me.GetWheelRotation() != 0) {
    FParent->OnMouseWheel(me.GetX(), me.GetY(),
    (double)me.GetWheelRotation()/me.GetWheelDelta());
  }
  me.Skip();
}
//..............................................................................
void TGlCanvas::OnMouseCaptureChange(wxMouseCaptureChangedEvent& me) {
  wxMouseState ms = wxGetMouseState();
  short fl = EncodeEvent(ms, true);
  FXApp->ResetMouseState(ms.m_x, ms.m_y, fl, MouseButton, true);
  me.Skip();
}
//..............................................................................
void TGlCanvas::OnKeyDown(wxKeyEvent& m)  {
  FParent->OnKeyDown(m);
}
//..............................................................................
void TGlCanvas::OnKeyUp(wxKeyEvent& m)  {
  FParent->OnKeyUp(m);
}
void TGlCanvas::OnChar(wxKeyEvent& m)  {
  FParent->OnChar(m);
}
//..............................................................................
olx_array_ptr<int> TGlCanvas::GetGlAttributes(bool _default, bool stereo,
  bool multisampling, short depth_bits)
{
  if (_default)  return (int*)NULL;
  size_t idx=0;
  olx_array_ptr<int> glAttrib(2*11+1);
  glAttrib[idx++] = WX_GL_RGBA;
  glAttrib[idx++] = WX_GL_DOUBLEBUFFER;
  //glAttrib[idx++] = WX_GL_MIN_ACCUM_RED;  glAttrib[idx++] = 16;
  //glAttrib[idx++] = WX_GL_MIN_ACCUM_GREEN;  glAttrib[idx++] = 16;
  //glAttrib[idx++] = WX_GL_MIN_ACCUM_BLUE;  glAttrib[idx++] = 16;
  //glAttrib[idx++] = WX_GL_MIN_ACCUM_ALPHA;  glAttrib[idx++] = 16;
  glAttrib[idx++] = WX_GL_DEPTH_SIZE;  glAttrib[idx++] = depth_bits;
  glAttrib[idx++] = WX_GL_STENCIL_SIZE;  glAttrib[idx++] = 8;
#if wxCHECK_VERSION(2,9,0)
  if (multisampling) {
    glAttrib[idx++] = WX_GL_SAMPLE_BUFFERS;  glAttrib[idx++] = 1;
    glAttrib[idx++] = WX_GL_SAMPLES;  glAttrib[idx++] = 4;
  }
#endif
  if (stereo)  {
    glAttrib[idx++] = WX_GL_STEREO;
  }
  glAttrib[idx++] = 0;
  return glAttrib;
}
#endif // Harmony native input bridge / original wxGLCanvas implementation
