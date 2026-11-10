#pragma once
#include "glscene.h"
#include "platform_font_factory.h"
#include <cmath>
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
  mutable PlatformFontFactory fonts_;
  double fontScale_=1;
  bool scaling_=false;
  TGlFont& DoCreateFont(TGlFont& font,bool half) const override {
    ++lifetime_.fontRequests;
    if(!bound_ || !IsEnabled() || !host_.make_current || !host_.make_current())throw std::runtime_error("No current leased context for font creation");
    return fonts_.Create(font,half,fontScale_);
  }
public:
  explicit PlatformWindowScene(PlatformSceneLifetime& lifetime):lifetime_(lifetime) {}
  ~PlatformWindowScene() override {++lifetime_.destroyed;}
  void Bind(const Olex2LegacyHost& host) {
    if(bound_ || IsEnabled())throw std::logic_error("A platform scene already has a surface lease");
    if(!host.make_current || !host.framebuffer_size || !host.resolve)
      throw std::invalid_argument("Incomplete platform scene surface lease");
    fonts_.SetDisplayMetrics(host.font_display_metrics);
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
  void ReleaseFontResources() {fonts_.Release();}
  bool FontsAllReleased()const {return fonts_.AllReleased();}
  const SystemFontCatalog& FontCatalog()const {return fonts_.FontCatalog();}
  const FontDisplayMetrics& DisplayMetrics()const {return fonts_.DisplayMetrics();}
  const NativeFontRecord& FontRecord(const TGlFont& font)const {return fonts_.Record(font);}
  void ScaleFonts(double scale) override {
    if(!bound_ || !std::isfinite(scale) || scale<=0 || scale>8 || scaling_)throw std::invalid_argument("Unbalanced or invalid platform font scale");
    fonts_.Release();fontScale_=scale;scaling_=true;
  }
  void RestoreFontScale() override {
    if(!scaling_)return;
    if(!bound_)throw std::logic_error("Font scale restoration needs a leased context");
    fonts_.Release();fontScale_=1;scaling_=false;
  }
  olxstr ShowFontDialog(TGlFont*,const olxstr&) override {throw std::runtime_error("Platform font dialog has not been implemented");}
};
}
