#include "png_texture_image.h"
#include <memory>
#include <stdexcept>
namespace olex2::harmony {
PngTextureImage DecodePngTextureImage(const std::string& filename) {
  PngTextureImage result;
  if(filename.empty()||filename.find('\0')!=std::string::npos) {
    result.image.status=TextureImageStatus::FileError;result.image.error="Expected an image filename";return result;
  }
  ImageResourceAudit audit;
  try {
    AuditedImageBuffer encoded(filename,audit);
    Olex2PngResult raw{};
    auto release=[](Olex2PngResult* value){olex2_png_result_release(value);};
    std::unique_ptr<Olex2PngResult,decltype(release)> pixels(&raw,release);
    olex2_png_decode_memory(encoded.bytes.data(),encoded.bytes.size(),MaximumTextureImageDimension,&raw);
    result.sourceBitDepth=raw.source_bit_depth;result.sourceColorType=raw.source_color_type;result.sourceInterlace=raw.source_interlace;
    result.allocations=raw.allocations;result.releases=raw.releases;result.liveAllocations=raw.live_allocations;result.liveBytes=raw.live_bytes;result.peakBytes=raw.peak_bytes;result.warnings=raw.warnings;
    result.image.width=raw.width;result.image.height=raw.height;result.image.rowStride=static_cast<uint32_t>(raw.row_bytes);result.image.readBytes=raw.rgb_bytes;
    result.image.status=raw.status==0?TextureImageStatus::Decoded:(raw.status==1?TextureImageStatus::UnsupportedDimensions:TextureImageStatus::PlatformError);
    result.image.error=raw.error;
    if(raw.status==0)result.image.rgb.assign(raw.rgb,raw.rgb+raw.rgb_bytes);
  }catch(const std::exception& e) {
    result.image.status=audit.file.opened?TextureImageStatus::PlatformError:TextureImageStatus::FileError;result.image.error=e.what();result.image.rgb.clear();
  }
  result.image.resourceAudit=audit;
  if(!audit.OwnedScopeBalanced() || (result.allocations>0&&!result.DecoderAllocationsBalanced())) {
    result.image.status=TextureImageStatus::PlatformError;result.image.rgb.clear();result.image.error="Owned PNG decoder resource cleanup failed";
  }
  return result;
}
}
