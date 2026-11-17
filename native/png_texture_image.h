#pragma once
#include "texture_image.h"
#include "png_texture_decoder.h"
namespace olex2::harmony {
struct PngTextureImage {
  TextureImage image;
  int sourceBitDepth=0,sourceColorType=0,sourceInterlace=0;
  size_t allocations=0,releases=0,liveAllocations=0,liveBytes=0,peakBytes=0;
  unsigned warnings=0;
  bool DecoderAllocationsBalanced() const {return allocations>0&&allocations==releases&&liveAllocations==0&&liveBytes==0;}
};
// Real libpng CPU decoding, independent of ImageKit pixel maps. Ordinary PNG
// samples are converted exactly, including hidden RGB under alpha=0.
PngTextureImage DecodePngTextureImage(const std::string& filename);
}
