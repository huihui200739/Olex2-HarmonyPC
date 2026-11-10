#include "upstream_material.h"
#include "upstream_color.h"
#include "material_commands.h"
#include "glmaterial.h"
#include "eutf8.h"
#include <cmath>
#include <vector>
namespace olex2::harmony {
namespace {
constexpr unsigned supportedFlags=sglmAmbientFB|sglmDiffuseFB|sglmEmissionFB;
std::string ColorText(const std::array<float,4>& base,float factor) {
  return std::to_string(base[0]*factor)+","+std::to_string(base[1]*factor)+","+
    std::to_string(base[2]*factor)+",1";
}
}
unsigned OpaqueMaterial::Flags() { return supportedFlags; }
const char* OpaqueMaterial::Source() { return "upstream/glib/glmaterial.cpp: TGlMaterial value methods (legacy Init excluded)"; }
OpaqueMaterial::OpaqueMaterial() { SetPreset(0,OpaqueColor().Rgba()); }
bool OpaqueMaterial::SetPreset(unsigned preset,const std::array<float,4>& base) {
  if(preset>2 || base[3]!=1) return false;
  for(float v:base) if(!std::isfinite(v) || v<0 || v>1) return false;
  const float coefficients[3][3]={{.30f,.70f,0},{.60f,.40f,0},{0,0,1}};
  std::string text=std::to_string(supportedFlags);
  for(unsigned channel=0;channel<3;++channel) {
    const auto value=ColorText(base,coefficients[preset][channel]);
    text+=';'+value+';'+value; // Preserve the original front/back token order.
  }
  return Parse(text);
}
bool OpaqueMaterial::SetLitPreset(unsigned preset,const std::array<float,4>& base) {
  if(preset<3) return SetPreset(preset,base);
  if(preset!=3 || base[3]!=1) return false;
  for(float value:base) if(!std::isfinite(value) || value<0 || value>1) return false;
  try {
    glObj::TGlMaterial value;
    value.SetFlags(supportedFlags|sglmSpecularFB|sglmShininessFB);
    value.AmbientF=value.AmbientB=TGlOption(base[0]*.3f,base[1]*.3f,base[2]*.3f,1.f);
    value.DiffuseF=value.DiffuseB=TGlOption(base[0]*.4f,base[1]*.4f,base[2]*.4f,1.f);
    value.SpecularF=value.SpecularB=TGlOption(base[0]*.3f,base[1]*.3f,base[2]*.3f,1.f);
    value.EmissionF=value.EmissionB=TGlOption(0.f,0.f,0.f,1.f);
    value.ShininessF=value.ShininessB=32;
    const auto commands=BuildMaterialCommands(value);
    LitMaterialParameters parameters;
    if(!ConsumeLitMaterial(commands,parameters)) return false;
    const auto encoded=TUtf8::Encode(value.ToString());std::string canonical(encoded.c_str(),encoded.Length());
    canonical_.swap(canonical);ambient_=parameters.ambient;diffuse_=parameters.diffuse;
    specular_=parameters.specular;emission_=parameters.emission;shininess_=parameters.shininess;
    flags_=value.GetFlags();commandCount_=static_cast<unsigned>(commands.operations.size());return true;
  }catch(...){return false;}
}
bool OpaqueMaterial::Parse(const std::string& text) {
  if(text.empty() || text.size()>1024 || text.find('\0')!=std::string::npos) return false;
  // Do not let the original permissive parser silently accept missing, surplus
  // or failed color tokens. Unsupported legacy flags never enter this adapter.
  std::vector<std::string> parts;
  size_t start=0;
  for(;;) {
    const size_t end=text.find(';',start);
    parts.push_back(text.substr(start,end==std::string::npos?end:end-start));
    if(parts.size()>7) return false;
    if(end==std::string::npos) break;
    start=end+1;
  }
  if(parts.size()!=7 || parts[0]!=std::to_string(supportedFlags)) return false;
  for(size_t i=1;i<parts.size();++i) { OpaqueColor c; if(!c.Parse(parts[i])) return false; }
  try {
    glObj::TGlMaterial candidate;
    candidate.FromString(olxstr(text.c_str()));
    if(candidate.GetFlags()!=supportedFlags) return false;
    const TGlOption* front[]={&candidate.AmbientF,&candidate.DiffuseF,&candidate.EmissionF};
    const TGlOption* back[]={&candidate.AmbientB,&candidate.DiffuseB,&candidate.EmissionB};
    for(unsigned k=0;k<3;++k) for(unsigned c=0;c<4;++c)
      if(!std::isfinite((*front[k])[c]) || (*front[k])[c]<0 || (*front[k])[c]>1 ||
          (*front[k])[c]!=(*back[k])[c] || (c==3 && (*front[k])[c]!=1)) return false;
    for(unsigned c=0;c<3;++c) if(candidate.AmbientF[c]+candidate.DiffuseF[c]+candidate.EmissionF[c]>1.000001f) return false;
    const auto encoded=TUtf8::Encode(candidate.ToString());
    std::string canonical(encoded.c_str(),encoded.Length());
    const auto commands=BuildMaterialCommands(candidate);
    OpaqueMaterialParameters parameters;
    if(!ConsumeOpaqueMaterial(commands,parameters)) return false;
    canonical_.swap(canonical); ambient_=parameters.ambient; diffuse_=parameters.diffuse;
    emission_=parameters.emission; specular_={}; shininess_=0; flags_=supportedFlags; commandCount_=static_cast<unsigned>(commands.operations.size());
    return true;
  } catch(...) { return false; } // No partial update on original parser exceptions.
}
float LitColorScale(const unsigned char* pixel,const std::array<float,4>& base) {
  if(!MatchesLitColor(pixel,base)) return -1.f;
  float dot=0,norm=0;
  for(unsigned i=0;i<3;++i) { const float c=base[i]*255.f; dot+=c*pixel[i]; norm+=c*c; }
  return dot/norm;
}
}
