#pragma once
#include "glyph_bitmap.h"
#include "glfont.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony {
// Private native font syntax is explicit, not a silent reinterpretation of wx IDs:
// ohos-native:<point-size>:<r|b|i|bi>[:fixed]. Raster size uses 96 DPI.
struct NativeFontRecord {
  TGlFont* font=nullptr;
  std::string description;
  unsigned effectivePoints=0;
  bool half=false, fixed=false;
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
