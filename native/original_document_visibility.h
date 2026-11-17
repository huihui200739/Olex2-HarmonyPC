#pragma once
// Ordinary framebuffer coverage only; no model, document, or scientific operation.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace olex2::harmony::visibility {
constexpr int MaximumSide = 8192;
constexpr int TileSide = 512;
struct Observation {
  std::size_t width=0, height=0, readPixels=0, tileCount=0, nonBackgroundPixels=0;
  std::size_t centralWidth=0, centralHeight=0, centralNonBackgroundPixels=0;
};
// Read supplies only real framebuffer bytes in production. A bounded reusable
// tile covers every pixel exactly once, including strips outside the old crop.
template<class Read>
Observation Observe(int width, int height, const std::array<std::uint8_t,3>& background, Read read) {
  if(width<1 || height<1 || width>MaximumSide || height>MaximumSide)
    throw std::invalid_argument("Invalid complete-viewport observation dimensions");
  Observation out;
  out.width=static_cast<std::size_t>(width);out.height=static_cast<std::size_t>(height);
  out.centralWidth=static_cast<std::size_t>(std::min(width,TileSide));
  out.centralHeight=static_cast<std::size_t>(std::min(height,TileSide));
  const int centralX=(width-static_cast<int>(out.centralWidth))/2;
  const int centralY=(height-static_cast<int>(out.centralHeight))/2;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(std::min(width,TileSide))*std::min(height,TileSide)*4);
  for(int y=0;y<height;y+=TileSide) {
    const int h=std::min(TileSide,height-y);
    for(int x=0;x<width;x+=TileSide) {
      const int w=std::min(TileSide,width-x);
      read(x,y,w,h,pixels.data());
      ++out.tileCount;out.readPixels+=static_cast<std::size_t>(w)*h;
      for(int row=0;row<h;++row)for(int col=0;col<w;++col) {
        const auto offset=(static_cast<std::size_t>(row)*w+col)*4;
        bool differs=false;
        for(unsigned c=0;c<3;++c) {
          const int delta=static_cast<int>(pixels[offset+c])-static_cast<int>(background[c]);
          if(delta>12 || delta< -12)differs=true; // Existing RGB tolerance; alpha is not content.
        }
        if(differs) {
          ++out.nonBackgroundPixels;
          if(x+col>=centralX && x+col<centralX+static_cast<int>(out.centralWidth) &&
             y+row>=centralY && y+row<centralY+static_cast<int>(out.centralHeight))
            ++out.centralNonBackgroundPixels;
        }
      }
    }
  }
  if(out.readPixels!=out.width*out.height)
    throw std::logic_error("Incomplete framebuffer observation");
  return out;
}
}
