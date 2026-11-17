#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#pragma once
#include "gxbase.h"
#include "glscene.h"
#include "platform_font_factory.h"
#include "original_renderer_host.h"
#include <string>

BeginGxlNamespace()
// Harmony supplies a leased EGL surface instead of a wx canvas/context. The
// renderer still owns this original scene and all original TGlFont instances.
class TwxGlScene : public AGlScene {
  Olex2LegacyHost host_{};
  bool bound_=false;
  mutable olex2::harmony::PlatformFontFactory fonts_;
  std::string fontsFolder_;
  double fontScale_=1;
  bool scaling_=false;
protected:
  TGlFont& DoCreateFont(TGlFont&,bool half) const override;
  void RequireCurrent() const;
public:
  explicit TwxGlScene(const olxstr& fontsFolder=EmptyString());
  ~TwxGlScene() override;
  void Bind(const Olex2LegacyHost&);
  void Unbind() noexcept;
  bool IsBound() const {return bound_;}
  bool MakeCurrent() override;
  void ScaleFonts(double scale) override;
  void RestoreFontScale() override;
  void ReleaseFontResources();
  bool FontsAllReleased() const {return fonts_.AllReleased();}
  const olex2::harmony::SystemFontCatalog& FontCatalog() const {return fonts_.FontCatalog();}
  const olex2::harmony::FontDisplayMetrics& DisplayMetrics() const {return fonts_.DisplayMetrics();}
  const olex2::harmony::NativeFontRecord& FontRecord(const TGlFont& font) const {return fonts_.Record(font);}
  olxstr ShowFontDialog(TGlFont*,const olxstr&) override;
};
EndGxlNamespace()

#else
/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef _xl_wxglsceneH
#define _xl_wxglsceneH
#include "gxbase.h"

#if defined (__WXWIDGETS__)
#include "glfont.h"
#include "glscene.h"
#include "wx/glcanvas.h"
#include "wx/wx.h"
#include "wx/image.h"
BeginGxlNamespace()

class TwxGlScene: public AGlScene {
private:
  olxstr FontsFolder;
  TIntList FontSizes;
  wxGLCanvas *Canvas;
  wxGLContext *Context;
protected:
  //olxstr ComposeIdString();
  virtual TGlFont& DoCreateFont(TGlFont& fnt, bool half_size) const;
public:
  TwxGlScene(const olxstr& fontsFolder);
  virtual ~TwxGlScene();
  // font names are seperated by the '&' char
  void ExportFont(const olxstr& name, const olxstr& fileName) const;
  // imports font from a file, if the font Name exists, replaces it
  TGlFont& ImportFont(TGlFont& fnt) const;
  // used to scale font when drawing on a scaled surface
  virtual void ScaleFonts(double scale);
  // restores the font sizes after a call to the ScaleFonts
  virtual void RestoreFontScale();

  virtual olxstr ShowFontDialog(TGlFont* glf=NULL,
    const olxstr& fontDesc=EmptyString());

  virtual void Destroy()  { AGlScene::Destroy();  }
  /* the canvas must be set for this operation to succeed.
  */
  virtual bool MakeCurrent();
  DefPropP(wxGLCanvas *, Canvas)
  DefPropP(wxGLContext *, Context)
  // final object (at least the constructor calls only the SetIdString of THIS object
  class MetaFont : public AGlScene::MetaFont {
  public:
    MetaFont(const olxstr& fontId)  {  SetIdString(fontId);  }
    virtual olxstr GetIdString() const;
    virtual bool SetIdString(const olxstr& idstr);
  };
};

EndGxlNamespace()
#endif // wxwidgets
#endif // guard

#endif // Harmony native scene / original wx implementation
