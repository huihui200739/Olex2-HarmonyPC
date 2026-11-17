#pragma once
#include <cmath>
#include <stdexcept>
#include <string>
namespace olex2::harmony {
enum class PlatformFontKind { NativeRaster, OriginalHershey };
struct FontDescription {
  PlatformFontKind kind=PlatformFontKind::NativeRaster;
  unsigned points=0;
  bool bold=false,italic=false,fixed=false;
};
inline unsigned BoundedFontPoints(const std::string& text,size_t first,size_t end) {
  if(first==end)throw std::invalid_argument("Missing font point size");
  unsigned points=0;
  for(size_t i=first;i<end;++i) {
    if(text[i]<'0'||text[i]>'9'||points>128)throw std::invalid_argument("Invalid bounded font size");
    points=points*10+static_cast<unsigned>(text[i]-'0');
  }
  if(!points||points>128)throw std::invalid_argument("Font size must be in [1,128]");
  return points;
}
inline FontDescription ParseFontDescription(const std::string& text) {
  if(text.empty()||text.size()>80)throw std::invalid_argument("Empty or oversized font description");
  FontDescription result;
  if(text[0]=='@') {
    result.kind=PlatformFontKind::OriginalHershey;
    result.points=text.size()==1?15:BoundedFontPoints(text,1,text.size());
    return result;
  }
  const std::string prefix="ohos-native:";
  if(text.rfind(prefix,0)!=0)throw std::invalid_argument("Unsupported font description; expected explicit native or original @ vector format");
  const auto colon=text.find(':',prefix.size());
  if(colon==std::string::npos)throw std::invalid_argument("Missing native font style");
  result.points=BoundedFontPoints(text,prefix.size(),colon);
  auto style=text.substr(colon+1);const std::string suffix=":fixed";
  if(style.size()>suffix.size()&&style.compare(style.size()-suffix.size(),suffix.size(),suffix)==0) {
    result.fixed=true;style.resize(style.size()-suffix.size());
  }
  if(style!="r"&&style!="b"&&style!="i"&&style!="bi")throw std::invalid_argument("Unknown native font style");
  result.bold=style.find('b')!=std::string::npos;result.italic=style.find('i')!=std::string::npos;
  return result;
}
inline unsigned EffectiveFontPoints(const FontDescription& spec,bool half,double scale) {
  if(!std::isfinite(scale)||scale<=0||scale>8)throw std::invalid_argument("Invalid platform font scale");
  // Match the existing desktop scene: @ vector point sizes are intrinsic and
  // are not altered by raster surface scaling or half-size font creation.
  const double points=spec.kind==PlatformFontKind::OriginalHershey?spec.points:
    std::floor(spec.points*scale*(half?0.75:1.0));
  if(points<1||points>256)throw std::invalid_argument("Scaled font exceeds bounded size");
  return static_cast<unsigned>(points);
}
}
