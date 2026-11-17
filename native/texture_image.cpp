#include "texture_image.h"
#include <limits>
namespace olex2::harmony {
const char* TextureImageStatusName(TextureImageStatus status) {
  switch(status) {
    case TextureImageStatus::Decoded:return "decoded";
    case TextureImageStatus::UnsupportedDimensions:return "unsupported-dimensions";
    case TextureImageStatus::UnsupportedTransparency:return "unsupported-transparency";
    case TextureImageStatus::InvalidPixels:return "invalid-pixels";
    case TextureImageStatus::FileError:return "file-error";
    case TextureImageStatus::PlatformError:return "platform-error";
  }
  return "unknown";
}
TextureImage PackOpaqueRgbaTexture(uint32_t width,uint32_t height,
    uint32_t stride,const uint8_t* pixels,size_t bytes) {
  TextureImage result;result.width=width;result.height=height;result.rowStride=stride;result.readBytes=bytes;
  if(!width || width!=height || width>MaximumTextureImageDimension || (width&(width-1))) {
    result.status=TextureImageStatus::UnsupportedDimensions;
    result.error="Original textures require a square power-of-two image, at most 4096 pixels";return result;
  }
  const size_t packedRow=size_t(width)*4;
  // Checked against the logical last row rather than copying alignment bytes.
  if(!pixels || stride<packedRow || size_t(height-1)>(std::numeric_limits<size_t>::max()-packedRow)/stride || bytes<size_t(height-1)*stride+packedRow) {
    result.status=TextureImageStatus::InvalidPixels;result.error="RGBA rows do not describe a complete image";return result;
  }
  for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x) {
    if(pixels[size_t(y)*stride+size_t(x)*4+3]!=255) {
      result.status=TextureImageStatus::UnsupportedTransparency;
      result.error="Transparent native pixel maps may lose original RGB; an exact PNG backend is still required";return result;
    }
  }
  result.rgb.resize(size_t(width)*height*3);
  for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x) {
    for(size_t c=0;c<3;++c)result.rgb[(size_t(y)*width+x)*3+c]=pixels[size_t(y)*stride+size_t(x)*4+c];
  }
  result.status=TextureImageStatus::Decoded;return result;
}
}
