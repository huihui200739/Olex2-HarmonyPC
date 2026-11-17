// Deterministic generic graphics state checks; no real structure data or GL context.
#include "material_commands.h"
#include "upstream_material.h"
#include "upstream_color.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
namespace {
using namespace olex2::harmony;
using glObj::TGlMaterial;
unsigned long long assertions=0,cases=0;
void Check(bool passed,const char* name) {
  ++assertions;if(!passed) throw std::runtime_error(name);
}
bool Same(const MaterialCommands& a,const MaterialCommands& b) {
  if(a.enabled!=b.enabled || a.operations.size()!=b.operations.size()) return false;
  for(size_t i=0;i<a.operations.size();++i) {
    const auto& x=a.operations[i]; const auto& y=b.operations[i];
    if(x.operation!=y.operation || x.capability!=y.capability || x.face!=y.face ||
        x.field!=y.field || x.values!=y.values) return false;
  }
  return true;
}
// Independent expected trace: numeric source flag contract and explicit order.
// Do not invoke ApplyMaterialCommands or copy its source to make this oracle.
MaterialCommands Expected(const TGlMaterial& m,const std::array<bool,6>& initial,bool skip) {
  MaterialCommands expected;expected.enabled=initial;if(skip) return expected;
  const unsigned bits=m.GetFlags();
  MaterialCommand query{MaterialOperation::Query};query.values[0]=initial[0]?1.f:0.f;
  expected.operations.push_back(query);
  const bool transparent=(bits&0x0400)!=0;
  if(transparent!=initial[0]) for(unsigned k=0;k<4;++k) {
    MaterialCommand c{transparent?MaterialOperation::Enable:MaterialOperation::Disable};
    c.capability=static_cast<MaterialCapability>(k);expected.operations.push_back(c);expected.enabled[k]=transparent;
  }
  auto toggle=[&](bool enabled) {
    MaterialCommand c{enabled?MaterialOperation::Enable:MaterialOperation::Disable};
    c.capability=MaterialCapability::ColorMaterial;expected.operations.push_back(c);expected.enabled[4]=enabled;
  };
  auto parameter=[&](unsigned face,MaterialField field,const TGlOption& color,bool present) {
    MaterialCommand c{MaterialOperation::Parameter};c.face=static_cast<MaterialFace>(face);c.field=field;
    c.values={0,0,0,1};if(present) std::copy(color.Data(),color.Data()+4,c.values.begin());expected.operations.push_back(c);
  };
  auto shine=[&](unsigned face,bool present) {
    MaterialCommand c{MaterialOperation::Parameter};c.face=static_cast<MaterialFace>(face);c.field=MaterialField::Shininess;
    c.values[0]=present?(face?m.ShininessB:m.ShininessF):0;expected.operations.push_back(c);
  };
  auto colorMode=[&](unsigned face,MaterialField field) {
    MaterialCommand c{MaterialOperation::ColorMode};c.face=static_cast<MaterialFace>(face);c.field=field;expected.operations.push_back(c);
  };
  auto color=[&](const TGlOption& value) {
    MaterialCommand c{MaterialOperation::Color};std::copy(value.Data(),value.Data()+4,c.values.begin());expected.operations.push_back(c);
  };
  if(!(bits&0x4000)) {
    toggle(false);
    parameter(0,MaterialField::Diffuse,m.DiffuseF,bits&4);parameter(1,MaterialField::Diffuse,m.DiffuseB,bits&8);
    parameter(0,MaterialField::Ambient,m.AmbientF,bits&1);parameter(1,MaterialField::Ambient,m.AmbientB,bits&2);
    parameter(0,MaterialField::Specular,m.SpecularF,bits&16);parameter(1,MaterialField::Specular,m.SpecularB,bits&32);
    parameter(0,MaterialField::Emission,m.EmissionF,bits&256);parameter(1,MaterialField::Emission,m.EmissionB,bits&512);
    shine(0,bits&64);shine(1,bits&128);
  } else {
    toggle(true);
    if(bits&1) { colorMode(0,MaterialField::Ambient);color(m.AmbientF); }
    if(bits&16) { colorMode(0,MaterialField::Specular);color(m.SpecularF); }
    if(bits&32) { colorMode(1,MaterialField::Specular);color(m.SpecularB); }
    if(bits&64) shine(0,true);
    if(bits&128) shine(1,true);
    colorMode(2,MaterialField::AmbientAndDiffuse);
  }
  return expected;
}
}
int main() {
  try {
    using namespace olex2::harmony;
    TGlMaterial material;
    material.AmbientF=TGlOption(.11f,.12f,.13f,.14f);material.AmbientB=TGlOption(.21f,.22f,.23f,.24f);
    material.DiffuseF=TGlOption(.31f,.32f,.33f,.34f);material.DiffuseB=TGlOption(.41f,.42f,.43f,.44f);
    material.SpecularF=TGlOption(.51f,.52f,.53f,.54f);material.SpecularB=TGlOption(.61f,.62f,.63f,.64f);
    material.EmissionF=TGlOption(.71f,.72f,.73f,.74f);material.EmissionB=TGlOption(.81f,.82f,.83f,.84f);
    material.ShininessF=37;material.ShininessB=93;
    const std::array<bool,6> states[]={{},{true,true,true,true,true,true},{true,false,false,false,false,false},{false,true,true,true,true,true}};
    for(unsigned flags=0;flags<=65535;++flags) {
      material.SetFlags(static_cast<uint16_t>(flags));
      for(const auto& initial:states) for(bool skip:{false,true}) {
        Check(Same(BuildMaterialCommands(material,initial,skip),Expected(material,initial,skip)),"original material command ordering/data/state differs from independent flag contract");++cases;
      }
    }
    std::cout<<"PASS all uint16 flag combinations, four prior capability states and skip behavior\n";
    for(unsigned color=0;color<3;++color) for(unsigned preset=0;preset<3;++preset) {
      OpaqueColor base;base.SetPreset(color);OpaqueMaterial adapted;
      Check(adapted.SetPreset(preset,base.Rgba()),"preset through command stage");
      TGlMaterial value;value.FromString(olxstr(adapted.Canonical().c_str()));
      auto commands=BuildMaterialCommands(value);OpaqueMaterialParameters output;
      Check(commands.operations.size()==12 && adapted.CommandCount()==12,"real original Init query and eleven mutating commands");
      bool consumed=ConsumeOpaqueMaterial(commands,output);
      for(unsigned k=0;k<3;++k) consumed=consumed && output.ambient[k]==value.AmbientF[k] &&
        output.diffuse[k]==value.DiffuseF[k] && output.emission[k]==value.EmissionF[k] &&
        std::abs(output.ambient[k]-adapted.Ambient()[k])<=1.f/255.f &&
        std::abs(output.diffuse[k]-adapted.Diffuse()[k])<=1.f/255.f &&
        std::abs(output.emission[k]-adapted.Emission()[k])<=1.f/255.f;
      // Original ToString packs colors to uint32; reparsing that canonical text
      // legitimately quantizes values. The live adapter uses the prepacked fields.
      Check(consumed,"actual uniforms consumed from portable original command stream; canonical quantization bounded");
      const auto before=output;
      auto invalid=commands;invalid.operations.pop_back();
      Check(!ConsumeOpaqueMaterial(invalid,output) && output.ambient==before.ambient && output.diffuse==before.diffuse && output.emission==before.emission,"incomplete command stream preserves old uniforms");
      invalid=commands;invalid.enabled[2]=true;
      Check(!ConsumeOpaqueMaterial(invalid,output),"unimplemented blend state cannot be silently accepted");
      invalid=commands;invalid.operations.push_back(commands.operations.back());
      Check(!ConsumeOpaqueMaterial(invalid,output),"duplicate state command rejected");
      value.SetFlags(783|0x4000);Check(!ConsumeOpaqueMaterial(BuildMaterialCommands(value),output),"color-material commands recorded but not misrepresented as supported GPU state");
      value.SetFlags(783|0x0400);Check(!ConsumeOpaqueMaterial(BuildMaterialCommands(value),output),"transparency commands recorded but not misrepresented as supported GPU state");
    }
    std::cout<<"PASS nine color/material pairs use command consumer with explicit unsupported-state rejection\n";
    std::cout<<"PASS upstream material command flow; "<<assertions<<" assertions; "<<cases<<" flag-state cases; not full fixed-function rendering\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
