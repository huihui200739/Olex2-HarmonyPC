#pragma once
#include "glscene.h"
#include "original_renderer_host.h"
#include <cstdint>
#include <stdexcept>

namespace olex2::harmony {
// The genuine renderer owns this scene for the entire application lifetime.
// A surface lease is short-lived; no callback points into an expired ABI frame.
struct PlatformSceneLifetime {
  uint64_t bindings=0, detachments=0, makeCurrent=0, startDraw=0, endDraw=0;
  uint64_t destroyed=0, fontRequests=0;
  bool cleanupFailed=false;
};
class PlatformWindowScene final:public AGlScene {
  PlatformSceneLifetime& lifetime_;
  Olex2LegacyHost host_{};
  bool bound_=false;
  TGlFont& DoCreateFont(TGlFont&,bool) const override {
    ++lifetime_.fontRequests;
    throw std::runtime_error("Platform font backend has not been implemented");
  }
public:
  explicit PlatformWindowScene(PlatformSceneLifetime& lifetime):lifetime_(lifetime) {}
  ~PlatformWindowScene() override {++lifetime_.destroyed;}
  void Bind(const Olex2LegacyHost& host) {
    if(bound_ || IsEnabled())throw std::logic_error("A platform scene already has a surface lease");
    if(!host.make_current || !host.framebuffer_size || !host.resolve)
      throw std::invalid_argument("Incomplete platform scene surface lease");
    host_=host;bound_=true;SetEnabled(true);++lifetime_.bindings;
  }
  void Unbind() noexcept {
    if(bound_)++lifetime_.detachments;
    SetEnabled(false);bound_=false;host_=Olex2LegacyHost{};
  }
  bool IsBound()const {return bound_;}
  bool MakeCurrent() override {
    ++lifetime_.makeCurrent;
    return IsEnabled() && bound_ && host_.make_current && host_.make_current()!=0;
  }
  bool StartDraw() override {++lifetime_.startDraw;return AGlScene::StartDraw();}
  void EndDraw() override {++lifetime_.endDraw;AGlScene::EndDraw();}
  void ScaleFonts(double) override {throw std::runtime_error("Platform font scaling has not been implemented");}
  void RestoreFontScale() override {throw std::runtime_error("Platform font scaling has not been implemented");}
  olxstr ShowFontDialog(TGlFont*,const olxstr&) override {throw std::runtime_error("Platform font dialog has not been implemented");}
};
}
