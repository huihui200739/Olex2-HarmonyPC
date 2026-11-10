#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace olex2::harmony {
struct RasterGlyph {
  int left=0, top=0; // Cell corner relative to the glyph baseline.
  uint32_t width=0, height=0;
  float advance=0;
  std::vector<unsigned char> rgb; // Top-down, opaque RGB; white background.
};
struct OriginalFontCells {
  uint16_t width=0,height=0;
  int baselineX=0,baselineY=0;
  std::array<std::vector<unsigned char>,256> rgb;
};
// Share one baseline and cell geometry for the original 256-character API.
// No rasterization, GL calls, invented glyphs or modification of source pixels.
OriginalFontCells PackOriginalFontCells(const std::array<RasterGlyph,256>& glyphs);
}
