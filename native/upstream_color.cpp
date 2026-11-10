#include "upstream_color.h"
#include "gloption.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace olex2::harmony {
namespace {
bool ValidSyntax(const std::string& text) {
  if (text.empty() || text.size()>128 || text.find('\0')!=std::string::npos) return false;
  if (text.find(',')==std::string::npos) {
    uint64_t packed=0;
    for (unsigned char c:text) {
      if (c<'0' || c>'9') return false;
      packed=packed*10+(c-'0');
      if (packed>UINT32_MAX) return false;
    }
    return true;
  }
  // Bound grammar before calling the original parser, whose numeric conversion
  // also accepts syntax not part of our application preference contract.
  size_t start=0;
  for (unsigned i=0;i<4;++i) {
    const size_t end=text.find(',',start);
    if ((i<3 && end==std::string::npos) || (i==3 && end!=std::string::npos)) return false;
    const auto part=text.substr(start,end==std::string::npos ? end : end-start);
    if(part.empty() || part.size()>30) return false;
    for(unsigned char c:part) if(!((c>='0' && c<='9') || c=='.' || c=='-' || c=='+' || c=='e' || c=='E')) return false;
    char* tail=nullptr;
    const float value=std::strtof(part.c_str(),&tail);
    if(tail!=part.c_str()+part.size() || !std::isfinite(value)) return false;
    start=end==std::string::npos ? text.size() : end+1;
  }
  return true;
}
}
OpaqueColor::OpaqueColor() { Parse("0.20,0.80,0.59,1"); }
bool OpaqueColor::Parse(const std::string& text) {
  if(!ValidSyntax(text)) return false;
  try {
    glObj::TGlOption candidate;
    if(!candidate.FromString(olxstr(text.c_str()))) return false;
    for(size_t i=0;i<4;++i) if(!std::isfinite(candidate[i]) || candidate[i]<0 || candidate[i]>1) return false;
    // Transparency is deliberately unsupported until blending/depth ordering is ported.
    if(candidate[3]!=1.f) return false;
    const uint32_t packed=candidate.GetRGB();
    std::string canonical=std::to_string(packed);
    std::array<float,4> rgba;
    std::copy(candidate.Data(),candidate.Data()+4,rgba.begin());
    canonical_.swap(canonical); rgba_=rgba; packed_=packed;
    return true;
  } catch (...) { return false; } // Keep the last valid value on any upstream conversion failure.
}
bool OpaqueColor::SetPreset(unsigned preset) {
  const char* values[]={"0.20,0.80,0.59,1","0.90,0.38,0.16,1","0.22,0.52,0.95,1"};
  return preset<3 && Parse(values[preset]);
}
const char* OpaqueColor::Source() { return "upstream/glib/gloption.cpp: TGlOption::FromString/GetRGB (unchanged implementation)"; }
bool MatchesLitColor(const unsigned char* pixel,const std::array<float,4>& rgba) {
  if(!pixel || pixel[3]!=255 || rgba[3]!=1.f) return false;
  float dot=0,norm=0;
  for(size_t i=0;i<3;++i) {
    if(!std::isfinite(rgba[i]) || rgba[i]<0 || rgba[i]>1) return false;
    const float c=rgba[i]*255.f; dot+=c*pixel[i]; norm+=c*c;
  }
  if(norm<1.f) return false;
  const float scale=dot/norm;
  if(scale<.29f || scale>1.02f) return false;
  for(size_t i=0;i<3;++i) if(std::abs(pixel[i]-rgba[i]*255.f*scale)>3.f) return false;
  return true;
}
}
