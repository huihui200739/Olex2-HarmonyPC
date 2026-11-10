#pragma once
#include "glyph_bitmap.h"
#include "olex_font_resource.h"
#include <string>
namespace olex2::harmony {
// Mirrors original ImportFont entry selection; no system-font fallback.
struct ResourceFontPlan {
  unsigned points=0, entryPoints=0;
  bool useRGB=false;
  std::string Entry(const OlexResourceFontDescription& description) const;
};
ResourceFontPlan PlanResourceFont(unsigned effectivePoints);
// RGB-only wx3.2.6 HIGH semantics: unchanged dimensions are copied, strict
// two-axis reduction uses box averaging, all other cases use cubic B-spline.
// Fonts are opaque red-on-black source cells; no alpha/mask/gamma conversion.
std::vector<unsigned char> ResizeResourceRGBCell(
  const std::vector<unsigned char>& source,unsigned oldWidth,unsigned oldHeight,
  unsigned width,unsigned height);
OriginalFontCells ResampleResourceFont(const OlexResourceFontData&,const ResourceFontPlan&);
}
