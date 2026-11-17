#include "ohos_texture_image.h"
#include <multimedia/image_framework/image/image_source_native.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace olex2::harmony {
namespace {
void Check(Image_ErrorCode status,const char* operation) {
  if(status!=IMAGE_SUCCESS)throw std::runtime_error(std::string(operation)+" error "+std::to_string(status));
}
}
static TextureImage DecodeBody(const std::string& filename,ImageResourceAudit& audit) {
  TextureImage result;
  if(filename.empty() || filename.find('\0')!=std::string::npos) {
    result.status=TextureImageStatus::FileError;result.error="Expected an image filename";return result;
  }
  try {
    std::unique_ptr<AuditedImageBuffer> encoded;
    try {encoded.reset(new AuditedImageBuffer(filename,audit));}
    catch(const std::exception& e) {result.status=TextureImageStatus::FileError;result.error=e.what();return result;}
    AuditedImageHandle<OH_ImageSourceNative,OH_ImageSourceNative_Release> source(audit,"source");
    AuditedImageHandle<OH_ImageSource_Info,OH_ImageSourceInfo_Release> sourceInfo(audit,"source-info");
    AuditedImageHandle<OH_DecodingOptions,OH_DecodingOptions_Release> options(audit,"options");
    AuditedImageHandle<OH_PixelmapNative,OH_PixelmapNative_Release> pixelmap(audit,"pixelmap");
    AuditedImageHandle<OH_Pixelmap_ImageInfo,OH_PixelmapImageInfo_Release> pixelInfo(audit,"pixel-info");
    Check(source.Created(OH_ImageSourceNative_CreateFromData(encoded->bytes.data(),encoded->bytes.size(),&source.value)),"Create image source");
    if(!source.value)throw std::runtime_error("Image API returned a null source");
    Check(sourceInfo.Created(OH_ImageSourceInfo_Create(&sourceInfo.value)),"Create source metadata");
    if(!sourceInfo.value)throw std::runtime_error("Image API returned a null sourceInfo");
    Check(OH_ImageSourceNative_GetImageInfo(source.value,0,sourceInfo.value),"Read source metadata");
    Check(OH_ImageSourceInfo_GetWidth(sourceInfo.value,&result.width),"Read source width");
    Check(OH_ImageSourceInfo_GetHeight(sourceInfo.value,&result.height),"Read source height");
    if(!result.width || result.width!=result.height || result.width>MaximumTextureImageDimension || (result.width&(result.width-1))) {
      result.status=TextureImageStatus::UnsupportedDimensions;
      result.error="Original textures require a square power-of-two image, at most 4096 pixels";return result;
    }
    Check(options.Created(OH_DecodingOptions_Create(&options.value)),"Create decode options");
    if(!options.value)throw std::runtime_error("Image API returned a null options");
    Check(OH_DecodingOptions_SetPixelFormat(options.value,PIXEL_FORMAT_RGBA_8888),"Select RGBA8888");
    Check(pixelmap.Created(OH_ImageSourceNative_CreatePixelmapUsingAllocator(source.value,options.value,IMAGE_ALLOCATOR_TYPE_SHARE_MEMORY,&pixelmap.value)),"Decode image");
    if(!pixelmap.value)throw std::runtime_error("Image API returned a null pixelmap");
    Check(pixelInfo.Created(OH_PixelmapImageInfo_Create(&pixelInfo.value)),"Create pixel metadata");
    if(!pixelInfo.value)throw std::runtime_error("Image API returned a null pixelInfo");
    Check(OH_PixelmapNative_GetImageInfo(pixelmap.value,pixelInfo.value),"Read pixel metadata");
    uint32_t width=0,height=0;
    Check(OH_PixelmapImageInfo_GetWidth(pixelInfo.value,&width),"Read pixel width");
    Check(OH_PixelmapImageInfo_GetHeight(pixelInfo.value,&height),"Read pixel height");
    Check(OH_PixelmapImageInfo_GetRowStride(pixelInfo.value,&result.rowStride),"Read pixel stride");
    Check(OH_PixelmapImageInfo_GetPixelFormat(pixelInfo.value,&result.pixelFormat),"Read pixel format");
    Check(OH_PixelmapImageInfo_GetAlphaType(pixelInfo.value,&result.alphaType),"Read alpha metadata");
    if(width!=result.width || height!=result.height || result.pixelFormat!=PIXEL_FORMAT_RGBA_8888 || result.rowStride<size_t(width)*4 || size_t(result.rowStride)*height>128u*1024u*1024u) {
      result.status=TextureImageStatus::InvalidPixels;result.error="Unexpected native pixel-map layout";return result;
    }
    std::vector<uint8_t> bytes(size_t(result.rowStride)*height);
    result.readBytes=bytes.size();Check(OH_PixelmapNative_ReadPixels(pixelmap.value,bytes.data(),&result.readBytes),"Read native pixels");
    if(result.readBytes>bytes.size()) {
      result.status=TextureImageStatus::InvalidPixels;result.error="Native read size differs from allocated storage";return result;
    }
    // Inspect actual alpha bytes. The tested target returns UNKNOWN metadata
    // even when its output has premultiplied RGB; alphaType is not a guarantee.
    TextureImage packed=PackOpaqueRgbaTexture(width,height,result.rowStride,bytes.data(),result.readBytes);
    packed.pixelFormat=result.pixelFormat;packed.alphaType=result.alphaType;return packed;
  }catch(const std::exception& error) {
    result.status=TextureImageStatus::PlatformError;result.error=error.what();result.rgb.clear();return result;
  }
}
TextureImage DecodeOhosOpaqueTextureImage(const std::string& filename) {
  ImageResourceAudit audit;
  TextureImage result=DecodeBody(filename,audit); // All native owners leave scope first.
  result.resourceAudit=audit;
  if(!audit.OwnedScopeBalanced()) {
    result.status=TextureImageStatus::PlatformError;result.rgb.clear();
    result.error="Owned image resource cleanup failed";
  }
  return result;
}
}
