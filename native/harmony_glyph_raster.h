#pragma once
#include <native_drawing/drawing_types.h>
#include "glyph_bitmap.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace olex2::harmony {
// Pixel size is explicit: conversion from point size/DPI belongs to the scene.
struct GlyphFontSpec {
  float pixelSize=16;
  bool bold=false, italic=false;
  // Optional caller-selected face. Keep it alive until this rasterizer dies.
  OH_Drawing_Typeface* typeface=nullptr;
  float scaleX=1.0f; // Physical dpiX/dpiY; never virtual densityPixels.
};
class HarmonyGlyphRasterizer {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit HarmonyGlyphRasterizer(const GlyphFontSpec&);
  ~HarmonyGlyphRasterizer();
  HarmonyGlyphRasterizer(const HarmonyGlyphRasterizer&)=delete;
  HarmonyGlyphRasterizer& operator=(const HarmonyGlyphRasterizer&)=delete;
  RasterGlyph Rasterize(uint32_t unicodeScalar) const;
};
}
