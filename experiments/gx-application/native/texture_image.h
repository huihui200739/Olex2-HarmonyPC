#pragma once
#include "image_resource_audit.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace olex2::harmony {
// Current native decoder scope is deliberately opaque images only. The original
// wx texture path keeps RGB even under alpha=0; a premultiplied pixel map cannot
// reconstruct those hidden colors. Never silently substitute altered RGB.
enum class TextureImageStatus {
  Decoded, UnsupportedDimensions, UnsupportedTransparency, InvalidPixels,
  FileError, PlatformError
};
struct TextureImage {
  TextureImageStatus status=TextureImageStatus::PlatformError;
  uint32_t width=0, height=0, rowStride=0;
  int32_t pixelFormat=0, alphaType=0;
  size_t readBytes=0;
  std::vector<uint8_t> rgb;
  std::string error;
  ImageResourceAudit resourceAudit;
  bool Decoded() const { return status==TextureImageStatus::Decoded; }
};
constexpr uint32_t MaximumTextureImageDimension=4096;
const char* TextureImageStatusName(TextureImageStatus status);
// Packed row-major RGB; the original TGXApp loader still applies its historical
// x/y convention. This conversion does not upload textures or change GL state.
TextureImage PackOpaqueRgbaTexture(uint32_t width,uint32_t height,
  uint32_t rowStride,const uint8_t* pixels,size_t bytes);
}
