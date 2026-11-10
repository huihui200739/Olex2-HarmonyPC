#pragma once
#include "glyph_bitmap.h"
#include "glfont.h"
#include "olex_font_resource.h"
#include "system_font_catalog.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony {
// Private native font syntax is explicit, not a silent reinterpretation of wx IDs:
// ohos-native:<point-size>:<r|b|i|bi>[:fixed]. Raster size uses 96 DPI.
// Original vector format @ or @<point-size> uses genuine CreateHershey.
// Original resource8..28 uses CreateGlyphs; smaller/larger sizes select8/72
// and the explicit opaque-RGB high-quality resampler before original RGB glyphs.
struct NativeFontRecord {
  TGlFont* font=nullptr;
  std::string description;
  unsigned effectivePoints=0;
  bool half=false, fixed=false, vector=false, resource=false;
  std::string resourcePath;
  OlexResourceFontData resourceData;
  uint64_t resourceReads=0,resourceResamplings=0;
  bool resourceUsesRGB=false;
  bool explicitSystemFamily=false,actualFaceBold=false,actualFaceItalic=false;
  std::string selectedFamily,selectedStyle;
  int familyIndex=-1,styleIndex=-1,weightCode=0,widthCode=0,slantCode=0;
  unsigned resourceEntryPoints=0;
  OriginalFontCells cells;
  uint64_t builds=0, releases=0, rasterizations=0;
};
class PlatformFontFactory {
  mutable std::vector<std::unique_ptr<NativeFontRecord>> records_;
  mutable std::unique_ptr<SystemFontCatalog> catalog_;
public:
  TGlFont& Create(TGlFont&,bool half,double scale) const;
  const SystemFontCatalog& FontCatalog() const;
  void Release() const; // Must run with the leased original context current.
  const NativeFontRecord& Record(const TGlFont&) const;
  bool AllReleased() const;
};
}
