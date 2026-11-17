// Synthetic generic colors/material strings only; no real structure data.
#include "upstream_material.h"
#include "upstream_color.h"
#include "glmaterial.h"
#include "dataitem.h"
#include "eutf8.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void Check(bool ok,const char* name) { if(!ok) throw std::runtime_error(name); ++checks; std::cout<<"PASS "<<name<<'\n'; }
bool Near(float a,float b,float eps=.00001f) { return std::abs(a-b)<eps; }
}
int main() {
  using namespace olex2::harmony;
  try {
    Check(OpaqueMaterial::Flags()==783,"supported original material flag mapping");
    TGlMaterial original;
    original.SetFlags(sglmAmbientFB|sglmDiffuseFB|sglmEmissionFB);
    original.AmbientF=original.AmbientB=TGlOption(.2f,.3f,.4f,1.f);
    original.DiffuseF=original.DiffuseB=TGlOption(.3f,.2f,.1f,1.f);
    original.EmissionF=original.EmissionB=TGlOption(0.f,0.f,0.f,1.f);
    TGlMaterial round; round.FromString(original.ToString());
    Check(round.GetFlags()==original.GetFlags() && round.AmbientF.GetRGB()==original.AmbientF.GetRGB(),"original material string parser/serializer round trip");
    TGlMaterial copied; copied=original;
    Check(copied==original && copied.Compare(original)==0,"original material assignment and comparison");
    TDataItem item(nullptr,"synthetic-material"); original.ToDataItem(item);
    TGlMaterial fromData; Check(fromData.FromDataItem(item) && olxstr(fromData.ToString())==olxstr(original.ToString()),"original material data-item representation");
    Check(!original.ToPOV().IsEmpty() && !original.ToWRL(false).IsEmpty() && !original.ToWRL(true).IsEmpty(),"original generic material text exporters remain callable");
    for(unsigned colorPreset=0;colorPreset<3;++colorPreset) {
      OpaqueColor color; color.SetPreset(colorPreset);
      for(unsigned preset=0;preset<3;++preset) {
        OpaqueMaterial m; Check(m.SetPreset(preset,color.Rgba()),"valid color/material preset pair");
        const float factors[3][3]={{.3f,.7f,0},{.6f,.4f,0},{0,0,1}};
        for(unsigned c=0;c<3;++c) Check(Near(m.Ambient()[c],color.Rgba()[c]*factors[preset][0]) && Near(m.Diffuse()[c],color.Rgba()[c]*factors[preset][1]) && Near(m.Emission()[c],color.Rgba()[c]*factors[preset][2]),"uniform values originate from original material fields");
        TGlMaterial parsed; parsed.FromString(olxstr(m.Canonical().c_str()));
        Check(parsed.GetFlags()==783 && parsed.AmbientF==parsed.AmbientB && parsed.DiffuseF==parsed.DiffuseB && parsed.EmissionF==parsed.EmissionB,"original canonical flags and front/back order");
        OpaqueMaterial restored; Check(restored.Parse(m.Canonical()) && restored.Canonical()==m.Canonical(),"canonical material representation is stable");
        const auto before=m.Canonical(); const auto ambient=m.Ambient(),diffuse=m.Diffuse(),emission=m.Emission();
        std::vector<std::string> invalid={"", "garbage", "783", "0;0;0;0;0;0;0", "783;0;0;0;0;0;0", "783;4278190080;4278190080;4278190080", m.Canonical()+";extra", "65535;4278190080;4278190080;4278190080;4278190080;4278190080;4278190080", "783;nan,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1", "783;1,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1;0,0,0,1", "783;1,0,0,1;1,0,0,1;1,0,0,1;1,0,0,1;0,0,0,1;0,0,0,1",std::string(1025,'1'),std::string("783\0bad",7)};
        for(const auto& value:invalid) Check(!m.Parse(value) && m.Canonical()==before && m.Ambient()==ambient && m.Diffuse()==diffuse && m.Emission()==emission,"unsupported material rejected transactionally");
        Check(!m.SetPreset(3,color.Rgba()) && m.Canonical()==before,"unknown material preset preserves active material");
        const unsigned char pixel[]={51,204,150,255};
        Check(colorPreset!=0 || Near(LitColorScale(pixel,color.Rgba()),1.f,.005f),"opaque framebuffer scale helper");
      }
    }
    OpaqueMaterial material;
    Check(!material.SetPreset(0,{.2f,.8f,.59f,.5f}) && !material.SetPreset(0,{2,0,0,1}),"invalid base color rejected before deriving material");
    Check(LitColorScale(nullptr,{.2f,.8f,.59f,1})<0,"invalid pixel cannot become material evidence");
    std::cout<<"PASS upstream material value adapter only; "<<checks<<" assertions; not legacy Init or complete renderer\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
