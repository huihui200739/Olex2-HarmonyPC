#pragma once
#include "gxapp.h"
#include "eutf8.h"
#include "ohos_texture_image.h"

namespace olex2::harmony {
// Candidate-only graphics application seam. This is not the production runtime
// owner, a second concurrent singleton, a font backend, or a window renderer.
class OhosTextureGXApp: public TGXApp {
  mutable TextureImage lastTexture_;
public:
  using TGXApp::TGXApp;
  bool ReadPlatformTexture(const olxstr& filename,int& size,
                          std::vector<unsigned char>& rgb) const override {
    size=0;rgb.clear();
    const olxcstr utf8=TUtf8::Encode(filename);
    lastTexture_=DecodeOhosOpaqueTextureImage(std::string(utf8.c_str(),utf8.Length()));
    if(!lastTexture_.Decoded())return false;
    size=static_cast<int>(lastTexture_.width);rgb=lastTexture_.rgb;return true;
  }
  const TextureImage& LastTextureImage() const {return lastTexture_;}
};
}
