// Read ordinary synthetic PNG files with the real platform ImageKit API.
// This is a capability observation, not a software-decoder replacement.
#include "image_resource_audit.h"
#include <multimedia/image_framework/image/image_source_native.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace olex2::harmony;
void Require(Image_ErrorCode code,const char* stage) {
  if(code!=IMAGE_SUCCESS)throw std::runtime_error(std::string(stage)+" error "+std::to_string(code));
}
std::string Quote(const std::string& text) {
  std::ostringstream out;out << '"';
  for(unsigned char c:text) {
    if(c=='"'||c=='\\')out << '\\' << c;
    else if(c>=32)out << c;
    else out << "\\u00" << "0123456789abcdef"[c>>4] << "0123456789abcdef"[c&15];
  }
  out << '"';return out.str();
}
static std::string ProbeBody(const char* path,const char* mode,ImageResourceAudit& audit) {
  std::ostringstream result;
  try {
    const int32_t desired=std::string(mode)=="rgb"?PIXEL_FORMAT_RGB_888:PIXEL_FORMAT_RGBA_8888;
    AuditedImageBuffer encoded(path,audit);
    AuditedImageHandle<OH_ImageSourceNative,OH_ImageSourceNative_Release> source(audit,"source");
    AuditedImageHandle<OH_ImageSource_Info,OH_ImageSourceInfo_Release> source_info(audit,"source-info");
    AuditedImageHandle<OH_DecodingOptions,OH_DecodingOptions_Release> options(audit,"options");
    AuditedImageHandle<OH_PixelmapNative,OH_PixelmapNative_Release> pixelmap(audit,"pixelmap");
    AuditedImageHandle<OH_Pixelmap_ImageInfo,OH_PixelmapImageInfo_Release> pixel_info(audit,"pixel-info");
    Require(source.Created(OH_ImageSourceNative_CreateFromData(encoded.bytes.data(),encoded.bytes.size(),&source.value)),"source");
    if(!source.value)throw std::runtime_error("Image API returned a null source");
    Require(source_info.Created(OH_ImageSourceInfo_Create(&source_info.value)),"source info create");
    if(!source_info.value)throw std::runtime_error("Image API returned a null source_info");
    Require(OH_ImageSourceNative_GetImageInfo(source.value,0,source_info.value),"source info");
    uint32_t sw=0,sh=0,w=0,h=0,stride=0;int32_t format=0,alpha=0;
    Require(OH_ImageSourceInfo_GetWidth(source_info.value,&sw),"source width");
    Require(OH_ImageSourceInfo_GetHeight(source_info.value,&sh),"source height");
    if(!sw||!sh||sw>4096||sh>4096)throw std::runtime_error("Image outside bounded decoder observation");
    Require(options.Created(OH_DecodingOptions_Create(&options.value)),"options create");
    if(!options.value)throw std::runtime_error("Image API returned a null options");
    Require(OH_DecodingOptions_SetPixelFormat(options.value,desired),"requested format");
    Require(pixelmap.Created(OH_ImageSourceNative_CreatePixelmapUsingAllocator(source.value,options.value,IMAGE_ALLOCATOR_TYPE_SHARE_MEMORY,&pixelmap.value)),"decode");
    if(!pixelmap.value)throw std::runtime_error("Image API returned a null pixelmap");
    Require(pixel_info.Created(OH_PixelmapImageInfo_Create(&pixel_info.value)),"pixel info create");
    if(!pixel_info.value)throw std::runtime_error("Image API returned a null pixel_info");
    Require(OH_PixelmapNative_GetImageInfo(pixelmap.value,pixel_info.value),"pixel info");
    Require(OH_PixelmapImageInfo_GetWidth(pixel_info.value,&w),"pixel width");
    Require(OH_PixelmapImageInfo_GetHeight(pixel_info.value,&h),"pixel height");
    Require(OH_PixelmapImageInfo_GetRowStride(pixel_info.value,&stride),"pixel stride");
    Require(OH_PixelmapImageInfo_GetPixelFormat(pixel_info.value,&format),"pixel format");
    Require(OH_PixelmapImageInfo_GetAlphaType(pixel_info.value,&alpha),"pixel alpha");
    if(w!=sw||h!=sh||stride<sw||size_t(stride)*h>128u*1024u*1024u)throw std::runtime_error("Unexpected decoded geometry");
    std::vector<uint8_t> bytes(size_t(stride)*h,0);size_t read_size=bytes.size();
    Require(OH_PixelmapNative_ReadPixels(pixelmap.value,bytes.data(),&read_size),"read pixels");
    if(read_size>bytes.size())throw std::runtime_error("Reported read size exceeds buffer");
    result << "{\"execution_completed\":true,\"source_width\":" << sw << ",\"source_height\":" << sh
       << ",\"width\":" << w << ",\"height\":" << h << ",\"stride\":" << stride
       << ",\"requested_format\":" << desired << ",\"actual_format\":" << format << ",\"alpha_type\":" << alpha
       << ",\"buffer_size\":" << bytes.size() << ",\"read_size\":" << read_size << ",\"bytes\":[";
    for(size_t i=0;i<read_size;++i){if(i)result<<',';result<<unsigned(bytes[i]);}
    result << "]";return result.str();
  }catch(const std::exception& e){result<<"{\"execution_completed\":false,\"error\":"<<Quote(e.what());return result.str();}
}

std::string ImageDecodeProbe(const char* path,const char* mode) {
  ImageResourceAudit audit;
  const std::string body=ProbeBody(path,mode,audit);
  if(!audit.OwnedScopeBalanced())return "{\"execution_completed\":false,\"error\":\"Owned image resource cleanup failed\",\"decode_observation\":"+body+"},\"resource_audit\":"+ImageResourceAuditJson(audit)+"}";
  return "{"+body.substr(1)+",\"resource_audit\":"+ImageResourceAuditJson(audit)+"}";
}
