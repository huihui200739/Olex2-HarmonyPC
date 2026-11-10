#include "harmony_glyph_raster.h"
int main() {
  olex2::harmony::HarmonyGlyphRasterizer raster({});
  const auto glyph=raster.Rasterize('A');
  return glyph.width && glyph.height && !glyph.rgb.empty()?0:1;
}
