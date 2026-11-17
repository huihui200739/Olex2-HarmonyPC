#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WIN32__) && !defined(__WXWIDGETS__)
#pragma once
#include "wxglscene.h"
#include "gles_offscreen_target.h"

// Harmony counterpart of the original bitmap/HDC scene. It borrows the current
// application's ES3 context and draws into a context-local framebuffer target.
class TWGlScene : public TwxGlScene {
  olex2::harmony::GlesOffscreenTarget framebuffer_;
public:
  TWGlScene()=default;
  ~TWGlScene() noexcept override;
  void InitialiseSurface(const Olex2LegacyHost&,int width,int height);
  bool MakeCurrent() override;
  void Destroy() override;
  int Width() const {return framebuffer_.Width();}
  int Height() const {return framebuffer_.Height();}
  std::vector<uint8_t> ReadRGBA();
};

#else
/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_gxl_wglscene_H
#define __olx_gxl_wglscene_H
#include "glscene.h"

#if defined(__WIN32__)
class TWGlScene : public AGlScene {
  HGLRC FGlContext;
  HDC   FWContext;
  HBITMAP FBitmap;
  void SetPixelFormatDescriptor(HDC hDc, uint8_t bits);
  void SetPixelFormatDescriptorX(HDC hDc, uint8_t bits);
  virtual TGlFont& DoCreateFont(TGlFont& fnt, bool half_size) const;
public:
  TWGlScene();
  virtual ~TWGlScene()  {  Destroy();  }
  void InitialiseBMP(HBITMAP Bmp, uint8_t bpp=24);
  void InitialiseHDC(HDC Dc);
  HDC GetDC() const { return FWContext; }
  void Destroy();
  virtual void ScaleFonts(double scale)  {
    throw TNotImplementedException(__OlxSourceInfo);
  }
  // restores the font sizes after a call to the ScaleFonts
  virtual void RestoreFontScale()  {
    throw TNotImplementedException(__OlxSourceInfo);
  }
  virtual olxstr ShowFontDialog(TGlFont* glf = NULL,
    const olxstr& fontDescription=EmptyString())
  {
    throw TNotImplementedException(__OlxSourceInfo);
  }
  virtual bool MakeCurrent();
};
#endif // __WIN32__
#endif

#endif // Harmony offscreen scene / original Windows implementation
