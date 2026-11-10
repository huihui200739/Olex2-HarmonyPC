#pragma once
#include "texture_image.h"
namespace olex2::harmony {
// Requires a normal OHOS application process with the public ImageKit runtime.
// It is not a command-line SDK stub or a renderer/complete application backend.
TextureImage DecodeOhosOpaqueTextureImage(const std::string& filename);
}
