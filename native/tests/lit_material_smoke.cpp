// Synthetic generic colors, states and triangles only. No scientific input data.
#include "upstream_material.h"
#include "upstream_color.h"
#include "material_commands.h"
#include "lit_row_reference.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace olex2::harmony;
unsigned assertions=0;
void Check(bool valid,const char* message){++assertions;if(!valid)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
}
int main(){
 try{
  for(unsigned color=0;color<3;++color)for(unsigned preset=0;preset<4;++preset){
    OpaqueColor base;base.SetPreset(color);OpaqueMaterial m;
    Check(m.SetLitPreset(preset,base.Rgba()),"supported lit material preset uses original fields and command flow");
    Check(m.ActiveFlags()==(preset==3?1023u:783u)&&m.CommandCount()==12,"actual property flags and twelve Init operations");
    const float factors[4][4]={{.3,.7,0,0},{.6,.4,0,0},{0,0,0,1},{.3,.4,.3,0}};
    for(unsigned c=0;c<3;++c)Check(Near(m.Ambient()[c],base.Rgba()[c]*factors[preset][0])&&Near(m.Diffuse()[c],base.Rgba()[c]*factors[preset][1])&&Near(m.Specular()[c],base.Rgba()[c]*factors[preset][2])&&Near(m.Emission()[c],base.Rgba()[c]*factors[preset][3]),"all shader terms consumed from original command parameters");
    Check(m.Shininess()==(preset==3?32:0),"original shininess command reaches shader exponent");
    const auto canonical=m.Canonical();const auto specular=m.Specular();const auto flags=m.ActiveFlags();
    Check(!m.SetLitPreset(4,base.Rgba())&&m.Canonical()==canonical&&m.Specular()==specular&&m.ActiveFlags()==flags,"unknown lit preset preserves current state");
    Check(!m.SetLitPreset(3,{0,0,0,.5f})&&m.Canonical()==canonical,"unsupported transparency rejected without partial state update");
    Check(!m.SetLitPreset(3,{std::numeric_limits<float>::quiet_NaN(),0,0,1})&&m.Canonical()==canonical,"nonfinite base color cannot reach uniforms");
    Check(m.SetLitPreset(0,base.Rgba())&&m.Specular()==std::array<float,3>{}&&m.Shininess()==0&&m.ActiveFlags()==783,"returning to classic clears old specular and shininess state");
  }
  glObj::TGlMaterial value;value.SetFlags(1023);
  value.AmbientF=value.AmbientB=TGlOption(.1f,.1f,.1f,1.f);value.DiffuseF=value.DiffuseB=TGlOption(.4f,.4f,.4f,1.f);
  value.SpecularF=value.SpecularB=TGlOption(.5f,.5f,.5f,1.f);value.EmissionF=value.EmissionB=TGlOption(0.f,0.f,0.f,1.f);
  for(short exponent:{short(0),short(32),short(128)}){
   value.ShininessF=value.ShininessB=exponent;LitMaterialParameters output;auto commands=BuildMaterialCommands(value);
   Check(ConsumeLitMaterial(commands,output)&&output.shininess==exponent&&output.specular[0]==.5f,"bounded OpenGL shininess endpoints and specular values accepted");
   OpaqueMaterialParameters old;Check(!ConsumeOpaqueMaterial(commands,old),"legacy strict consumer does not silently accept specular material");
  }
  LitMaterialParameters output;output.shininess=17;const auto before=output;
  value.ShininessF=value.ShininessB=129;Check(!ConsumeLitMaterial(BuildMaterialCommands(value),output)&&output.shininess==before.shininess,"out of supported exponent range preserves output");
  value.ShininessF=32;value.ShininessB=16;Check(!ConsumeLitMaterial(BuildMaterialCommands(value),output),"asymmetric shininess rejected explicitly");
  value.ShininessB=32;value.SpecularB=TGlOption(.4f,.4f,.4f,1.f);Check(!ConsumeLitMaterial(BuildMaterialCommands(value),output),"unsupported asymmetric faces rejected explicitly");
  value.SpecularB=value.SpecularF;value.SetFlags(1023|0x0400);Check(!ConsumeLitMaterial(BuildMaterialCommands(value),output),"transparent GL state not silently ignored");
  value.SetFlags(1023|0x4000);Check(!ConsumeLitMaterial(BuildMaterialCommands(value),output),"color tracking not silently ignored");
  const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  MeshData triangle;triangle.submittedVertices={{{-.8f,-.8f,0},{1,0,0}},{{.8f,-.8f,0},{0,1,0}},{{0,.8f,0},{0,0,1}}};triangle.drawTriangles={0,1,2};
  auto row=ReferenceLightingRow(triangle,identity,identity,100,100,32);
  Check(row.size()>30,"independent reference samples real interior triangle pixels");
  auto center=std::find_if(row.begin(),row.end(),[](const auto& p){return p.x==50;});
  const double length=std::sqrt(1+.35*.35+.5*.5),hz=std::sqrt((1+1/length)/2);
  const double expected=.240625*.35/length+.253125*.5/length+.50625/length;
  Check(center!=row.end()&&std::abs(center->diffuse-expected)<.00001&&std::abs(center->specular-.50625*std::pow(hz,32))<.00001,"hand-computed barycentric lighting and shininess reference");
  auto projective=identity;projective[3]=.25f;
  auto perspectiveRow=ReferenceLightingRow(triangle,projective,identity,100,100,32);
  auto perspectiveCenter=std::find_if(perspectiveRow.begin(),perspectiveRow.end(),[](const auto& p){return p.x==50;});
  const double perspectiveExpected=(32./133*.35+101./399*.5+202./399)/length;
  Check(perspectiveCenter!=perspectiveRow.end()&&std::abs(perspectiveCenter->diffuse-perspectiveExpected)<.00001,"hand-computed perspective-correct lighting weights");
  MeshData overlapping=triangle;
  for(auto& v:overlapping.submittedVertices){v.position[2]=.2f;v.normal={0,0,-1};}
  for(auto v:triangle.submittedVertices){v.position[2]=-.2f;v.normal={0,0,1};overlapping.submittedVertices.push_back(v);}
  overlapping.drawTriangles={0,1,2,3,4,5};
  auto depthRow=ReferenceLightingRow(overlapping,identity,identity,100,100,32);
  Check(!depthRow.empty()&&std::all_of(depthRow.begin(),depthRow.end(),[&](const auto& p){return std::abs(p.diffuse-1/length)<.00001;}),"independent depth test selects visible triangle rather than hidden back layer");
  for(auto& v:triangle.submittedVertices)v.normal={0,0,-1};
  row=ReferenceLightingRow(triangle,identity,identity,100,100,32);
  Check(!row.empty()&&std::all_of(row.begin(),row.end(),[](const auto& p){return p.diffuse==0&&p.specular==0;}),"back-lit vertices cannot generate a spurious highlight");
  unsigned rejected=0;
  for(unsigned variant=0;variant<4;++variant){
   try{auto mesh=triangle;auto matrix=identity;unsigned width=100;
    if(variant==0)width=16385;if(variant==1)mesh.drawTriangles[0]=99;if(variant==2)matrix[15]=-1;if(variant==3)mesh.submittedVertices[0].normal={0,0,0};
    ReferenceLightingRow(mesh,matrix,identity,width,100,32);
   }catch(const std::invalid_argument&){++rejected;}
  }
  Check(rejected==4,"unsupported CPU reference inputs are rejected explicitly");
  std::cout<<"PASS lit material and independent row reference; "<<assertions<<" assertions; opaque symmetric custom lighting only\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
