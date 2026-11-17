#include "glyph_bitmap.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace olex2::harmony {
OriginalFontCells PackOriginalFontCells(const std::array<RasterGlyph,256>& glyphs) {
  int64_t left=0,top=0,right=0,bottom=0;
  for(const auto& glyph:glyphs) {
    if(!glyph.width || !glyph.height || glyph.width>4096 || glyph.height>4096 ||
       glyph.left < -4096 || glyph.left>4096 || glyph.top < -4096 || glyph.top>4096 ||
       !std::isfinite(glyph.advance) || glyph.advance<0 || glyph.advance>4096 ||
       glyph.rgb.size()!=static_cast<size_t>(glyph.width)*glyph.height*3)
      throw std::invalid_argument("Invalid bounded original-font source cell");
    left=std::min(left,static_cast<int64_t>(glyph.left));
    top=std::min(top,static_cast<int64_t>(glyph.top));
    right=std::max(right,static_cast<int64_t>(glyph.left)+glyph.width);
    bottom=std::max(bottom,static_cast<int64_t>(glyph.top)+glyph.height);
  }
  const auto width=right-left,height=bottom-top;
  if(width<=0 || height<=0 || width>4096 || height>4096 || width*height*3*256>128*1024*1024)
    throw std::length_error("Original-font atlas exceeds bounded allocation");
  OriginalFontCells result;result.width=static_cast<uint16_t>(width);result.height=static_cast<uint16_t>(height);
  result.baselineX=static_cast<int>(-left);result.baselineY=static_cast<int>(-top);
  for(size_t n=0;n<glyphs.size();++n) {
    const auto& source=glyphs[n];auto& target=result.rgb[n];target.assign(static_cast<size_t>(width*height*3),255);
    const auto x=static_cast<size_t>(static_cast<int64_t>(source.left)-left);
    const auto y=static_cast<size_t>(static_cast<int64_t>(source.top)-top);
    for(size_t row=0;row<source.height;++row)
      std::copy_n(source.rgb.data()+row*source.width*3,static_cast<size_t>(source.width)*3,
                  target.data()+((y+row)*result.width+x)*3);
  }
  return result;
}
}
