#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace olex2::harmony {
// A value adapter only: does not claim fixed-function materials/lighting support.
class OpaqueColor {
public:
  OpaqueColor();
  bool Parse(const std::string& text);
  bool SetPreset(unsigned preset);
  const std::array<float,4>& Rgba() const { return rgba_; }
  uint32_t Packed() const { return packed_; }
  const std::string& Canonical() const { return canonical_; }
  static const char* Source();
private:
  std::array<float,4> rgba_{};
  uint32_t packed_ = 0;
  std::string canonical_;
};
// Match an opaque framebuffer pixel under the renderer's scalar [0.30,1] light.
// Rejects the background and wrong-preset colors, not just "nonblack" pixels.
bool MatchesLitColor(const unsigned char* pixel, const std::array<float,4>& rgba);
}
