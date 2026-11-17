/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef _WinInterfaceH
#define _WinInterfaceH
#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "native_widget.h"
#include <cstdint>
class TWindowInterface {
  olex2::harmony::ui::Widget* Window;
public:
  explicit TWindowInterface(olex2::harmony::ui::Widget* window):Window(window){}
  int GetWidth()const{return Window->GetWidth();} void SetWidth(int v){Window->SetWidth(v);}
  int GetHeight()const{return Window->GetHeight();} void SetHeight(int v){Window->SetHeight(v);}
  int GetLeft()const{return Window->GetLeft();} void SetLeft(int v){Window->SetLeft(v);}
  int GetTop()const{return Window->GetTop();} void SetTop(int v){Window->SetTop(v);}
  uint32_t GetColor()const{uint32_t c=Window->GetBackgroundColor();return ((c>>16)&255)|(c&0xff00)|((c&255)<<16);}
  void SetColor(uint32_t c){Window->SetBackgroundColor(0xff000000|((c&255)<<16)|(c&0xff00)|((c>>16)&255));}
  void SetColor(olex2::harmony::ui::Color c){Window->SetBackgroundColor((uint32_t(c.alpha)<<24)|(uint32_t(c.red)<<16)|(uint32_t(c.green)<<8)|c.blue);}
  bool IsVisible()const{return Window->IsShown();} void SetVisible(bool v){Window->Show(v);}
  bool IsEnabled()const{return Window->IsEnabled();} void SetEnabled(bool v){Window->Enable(v);}
  long GetWindowStyle()const{return Window->GetWindowStyle();} void SetWindowStyle(long v){Window->SetWindowStyle(v);}
  void AddWindowStyle(long v){SetWindowStyle(GetWindowStyle()|v);} void DelWindowStyle(long v){SetWindowStyle(GetWindowStyle()&~v);}
  bool HasWindowStyle(long v)const{return (GetWindowStyle()&v)!=0;}
  long GetExtraStyle()const{return Window->GetExtraStyle();} void SetExtraStyle(long v){Window->SetExtraStyle(v);}
  void AddExtraStyle(long v){SetExtraStyle(GetExtraStyle()|v);} void DelExtraStyle(long v){SetExtraStyle(GetExtraStyle()&~v);}
  bool HasExtraStyle(long v)const{return (GetExtraStyle()&v)!=0;}
};
#else
#include <wx/wx.h>
#include "ebase.h"
#include "gldefs.h"

class TWindowInterface  {
  wxWindow *Window;
public:
  TWindowInterface(wxWindow *window) : Window(window) {}

  int GetWidth()   const { return Window->GetRect().width; };
  void SetWidth(int w)   { Window->SetSize(-1, -1, w, -1, wxSIZE_USE_EXISTING); };
  int GetHeight()  const { return Window->GetRect().height; };
  void SetHeight(int h)  { Window->SetSize(-1, -1, -1, h, wxSIZE_USE_EXISTING); };

  int GetLeft()    const { return Window->GetRect().x; };
  void SetLeft(int l)    { Window->SetSize(l, -1, -1, -1, wxSIZE_USE_EXISTING); };
  int GetTop()     const { return Window->GetRect().y; };
  void SetTop(int r)     { Window->SetSize(-1, r, -1, -1, wxSIZE_USE_EXISTING); };
  uint32_t GetColor() const {
    wxColour c = Window->GetBackgroundColour();
    return OLX_RGB(c.Red(), c.Green(), c.Blue());
  }
  void SetColor(uint32_t c)  {
    Window->SetBackgroundColour(wxColour(OLX_GetRValue(c), OLX_GetGValue(c),
      OLX_GetBValue(c)));
    Window->Refresh();
  }
  void SetColor(wxColor c)  {  Window->SetBackgroundColour(c);  Window->Refresh();  }
  bool IsVisible()    const {  return Window->IsShown();  }
  void SetVisible(bool v)   {
    if( v )
      Window->Show();
    else
      Window->Hide();
  }

  bool IsEnabled() const { return Window->IsEnabled(); }
  void SetEnabled(bool v) { Window->Enable(v); }


  long GetWindowStyle()  const {   return Window->GetWindowStyleFlag();  }
  void SetWindowStyle(long v)  { Window->SetWindowStyleFlag(v);  }
  void AddWindowStyle(long v)  { Window->SetWindowStyleFlag(GetWindowStyle() | v);  }
  void DelWindowStyle(long v)  {
    Window->SetWindowStyleFlag(GetWindowStyle() & ~v);
  }
  bool HasWindowStyle(long v) const {  return (GetWindowStyle() & v) != 0;  }

  long GetExtraStyle()   const {   return Window->GetExtraStyle();  }
  void SetExtraStyle(long v)   { Window->SetExtraStyle(v);  }
  void AddExtraStyle(long v)   { Window->SetExtraStyle(GetWindowStyle() | v);  }
  void DelExtraStyle(long v)   {
    SetExtraStyle(GetExtraStyle() & ~v);
  }
  bool HasExtraStyle(long v) const {  return (GetExtraStyle() & v) != 0;  }
};

#endif // native widgets / original wx
#endif
