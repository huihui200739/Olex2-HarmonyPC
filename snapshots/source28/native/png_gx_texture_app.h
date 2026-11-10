#pragma once
#include "gxapp.h"
#include "eutf8.h"
#include "png_texture_image.h"
namespace olex2::harmony {
// Isolated evaluation owner only. It must not coexist with the production
// singleton; this is not a persistent renderer, font backend or full UI.
class PngTextureGXApp:public TGXApp {
  mutable PngTextureImage lastPng_;
public:
  using TGXApp::TGXApp;
  bool ReadPlatformTexture(const olxstr& filename,int& size,
                          std::vector<unsigned char>& rgb) const override {
    size=0;rgb.clear();const olxcstr utf8=TUtf8::Encode(filename);
    lastPng_=DecodePngTextureImage(std::string(utf8.c_str(),utf8.Length()));
    if(!lastPng_.image.Decoded())return false;
    size=static_cast<int>(lastPng_.image.width);rgb=lastPng_.image.rgb;return true;
  }
  const TextureImage& LastTextureImage() const {return lastPng_.image;}
  const PngTextureImage& LastPngTexture() const {return lastPng_;}
};
}
