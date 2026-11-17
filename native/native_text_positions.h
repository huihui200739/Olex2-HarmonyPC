#pragma once
// ArkUI uses UTF-16 offsets; the original strings use the platform's wchar_t.
// Reject malformed Unicode and never place a native caret inside a surrogate pair.
#include "ebase.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
inline std::size_t TextUtf16Offset(const olxstr& text,std::size_t index) {
  if(index>text.Length())throw std::out_of_range("Text index exceeds the original string");
  std::size_t units=0;
  for(std::size_t i=0;i<index;++i){
    const auto cp=static_cast<uint32_t>(text[i]);
    if constexpr(sizeof(wchar_t)==2){
      if(cp>=0xd800&&cp<=0xdbff){
        if(i+1>=text.Length()||static_cast<uint32_t>(text[i+1])<0xdc00||static_cast<uint32_t>(text[i+1])>0xdfff)throw std::invalid_argument("Unpaired text surrogate");
        if(i+1==index)throw std::invalid_argument("Text index splits a surrogate pair");
        ++i;units+=2;
      }else{if(cp>=0xdc00&&cp<=0xdfff)throw std::invalid_argument("Unpaired text surrogate");++units;}
    }else{
      if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))throw std::invalid_argument("Invalid text Unicode scalar");
      units+=cp>0xffff?2:1;
    }
    if(units>static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))throw std::length_error("Native text offset exceeds the SDK range");
  }
  return units;
}
inline std::size_t TextStringOffset(const olxstr& text,std::size_t units) {
  const auto maximum=TextUtf16Offset(text,text.Length());
  if(units>maximum)throw std::out_of_range("Native text offset exceeds its string");
  std::size_t offset=0;
  for(std::size_t i=0;i<text.Length();++i){
    const auto cp=static_cast<uint32_t>(text[i]);
    const std::size_t width=(sizeof(wchar_t)==2?(cp>=0xd800&&cp<=0xdbff):(cp>0xffff))?2:1;
    if(offset+width>units)return i;
    offset+=width;
    if constexpr(sizeof(wchar_t)==2){if(width==2)++i;}
    if(offset==units)return i+1;
  }
  return text.Length();
}
}
