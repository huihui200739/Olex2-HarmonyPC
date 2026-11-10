#pragma once
#include "glyph_bitmap.h"
#include "glfont.h"
#include "olex_font_resource.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony {
// Private native font syntax is explicit, not a silent reinterpretation of wx IDs:
// ohos-native:<point-size>:<r|b|i|bi>[:fixed]. Raster size uses 96 DPI.
// Original vector format @ or @<point-size> uses genuine CreateHershey.
// Original #file:style-size resources use exact8..28 stock entries and CreateGlyphs.
// The legacy wxImage resampling path outside8..28 remains explicitly unsupported.
struct NativeFontRecord {
  TGlFont* font=nullptr;
  std::string description;
  unsigned effectivePoints=0;
  bool half=false, fixed=false, vector=false, resource=false;
  std::string resourcePath;
  OlexResourceFontData resourceData;
  uint64_t resourceReads=0;
  OriginalFontCells cells;
  uint64_t builds=0, releases=0, rasterizations=0;
};
class PlatformFontFactory {
  mutable std::vector<std::unique_ptr<NativeFontRecord>> records_;
public:
  TGlFont& Create(TGlFont&,bool half,double scale) const;
  void Release() const; // Must run with the leased original context current.
  const NativeFontRecord& Record(const TGlFont&) const;
  bool AllReleased() const;
};
}
