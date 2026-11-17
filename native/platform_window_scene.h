#pragma once
#include "wxglscene.h"
#include <cstdint>

namespace olex2::harmony {
// Telemetry decorates the original Harmony scene; surface/font operations live
// in gxlib/wxglscene.cpp and are shared by the real application runtime.
struct PlatformSceneLifetime {
  uint64_t bindings=0, detachments=0, makeCurrent=0, startDraw=0, endDraw=0;
  uint64_t destroyed=0, fontRequests=0;
  bool cleanupFailed=false;
};
class PlatformWindowScene final:public TwxGlScene {
  PlatformSceneLifetime& lifetime_;
  TGlFont& DoCreateFont(TGlFont& font,bool half) const override {
    ++lifetime_.fontRequests;
    return TwxGlScene::DoCreateFont(font,half);
  }
public:
  explicit PlatformWindowScene(PlatformSceneLifetime& lifetime):lifetime_(lifetime) {}
  ~PlatformWindowScene() override {++lifetime_.destroyed;}
  void Bind(const Olex2LegacyHost& host) {TwxGlScene::Bind(host);++lifetime_.bindings;}
  void Unbind() noexcept {if(IsBound())++lifetime_.detachments;TwxGlScene::Unbind();}
  bool MakeCurrent() override {++lifetime_.makeCurrent;return TwxGlScene::MakeCurrent();}
  bool StartDraw() override {++lifetime_.startDraw;return AGlScene::StartDraw();}
  void EndDraw() override {++lifetime_.endDraw;AGlScene::EndDraw();}
};
}
