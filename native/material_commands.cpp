#include "material_commands.h"
#include <cmath>
#include <utility>
namespace olex2::harmony {
namespace {
class CommandSink {
public:
  MaterialCommands result;
  bool IsEnabled(MaterialCapability capability) {
    const bool value=result.enabled[static_cast<unsigned>(capability)];
    MaterialCommand command{MaterialOperation::Query};
    command.capability=capability; command.values[0]=value?1.f:0.f;
    result.operations.push_back(command); return value;
  }
  void enable(MaterialCapability capability) { Toggle(capability,true); }
  void disable(MaterialCapability capability) { Toggle(capability,false); }
  void material(MaterialFace face,MaterialField field,const float* values) {
    MaterialCommand command{MaterialOperation::Parameter};
    command.face=face; command.field=field;
    for(unsigned i=0;i<4;++i) command.values[i]=values[i];
    result.operations.push_back(command);
  }
  void material(MaterialFace face,MaterialField field,float value) {
    MaterialCommand command{MaterialOperation::Parameter};
    command.face=face; command.field=field; command.values[0]=value;
    result.operations.push_back(command);
  }
  void colorMaterial(MaterialFace face,MaterialField field) {
    MaterialCommand command{MaterialOperation::ColorMode};
    command.face=face; command.field=field; result.operations.push_back(command);
  }
  void color(const float* values) {
    MaterialCommand command{MaterialOperation::Color};
    for(unsigned i=0;i<4;++i) command.values[i]=values[i];
    result.operations.push_back(command);
  }
private:
  void Toggle(MaterialCapability capability,bool value) {
    MaterialCommand command{value?MaterialOperation::Enable:MaterialOperation::Disable};
    command.capability=capability; result.operations.push_back(command);
    result.enabled[static_cast<unsigned>(capability)]=value;
  }
};
}
const char* MaterialCommands::Source() { return "upstream/glib/materialcommands.h: original TGlMaterial::Init command flow"; }
MaterialCommands BuildMaterialCommands(const glObj::TGlMaterial& material,
    const std::array<bool,6>& previousCapabilities,bool skip) {
  CommandSink sink; sink.result.enabled=previousCapabilities;
  glObj::ApplyMaterialCommands(material,sink,skip);
  return std::move(sink.result);
}
bool ConsumeLitMaterial(const MaterialCommands& commands,LitMaterialParameters& output) {
  if(commands.enabled[0] || commands.enabled[1] || commands.enabled[2] ||
      commands.enabled[3] || commands.enabled[4] || commands.enabled[5]) return false;
  std::array<std::array<std::array<float,4>,4>,2> colors{};
  bool seen[2][5]{};
  float shininess[2]{};
  bool colorDisabled=false;
  for(const auto& command:commands.operations) {
    if(command.operation==MaterialOperation::Query) continue;
    if(command.operation==MaterialOperation::Disable &&
        command.capability==MaterialCapability::ColorMaterial) { colorDisabled=true; continue; }
    if(command.operation==MaterialOperation::Disable &&
        static_cast<unsigned>(command.capability)<4) continue;
    if(command.operation!=MaterialOperation::Parameter || command.face==MaterialFace::Both) return false;
    const unsigned face=static_cast<unsigned>(command.face),field=static_cast<unsigned>(command.field);
    if(face>1 || field>4 || seen[face][field]) return false;
    seen[face][field]=true;
    if(field==4) { if(!std::isfinite(command.values[0]) || command.values[0]<0 || command.values[0]>128) return false; shininess[face]=command.values[0]; continue; }
    for(float c:command.values) if(!std::isfinite(c) || c<0 || c>1) return false;
    if(command.values[3]!=1) return false;
    colors[face][field]=command.values;
  }
  if(!colorDisabled) return false;
  for(unsigned f=0;f<2;++f) for(unsigned k=0;k<5;++k) if(!seen[f][k]) return false;
  if(colors[0]!=colors[1] || shininess[0]!=shininess[1]) return false;
  for(unsigned c=0;c<3;++c) if(colors[0][0][c]+colors[0][1][c]+colors[0][2][c]+colors[0][3][c]>1.000001f) return false;
  LitMaterialParameters candidate;
  candidate.shininess=shininess[0];
  for(unsigned c=0;c<3;++c) {
    candidate.ambient[c]=colors[0][0][c];candidate.diffuse[c]=colors[0][1][c];candidate.emission[c]=colors[0][3][c];candidate.specular[c]=colors[0][2][c];
  }
  output=candidate;return true;
}
bool ConsumeOpaqueMaterial(const MaterialCommands& commands,OpaqueMaterialParameters& output) {
  LitMaterialParameters candidate;
  if(!ConsumeLitMaterial(commands,candidate) || candidate.shininess!=0 || candidate.specular!=std::array<float,3>{}) return false;
  output={candidate.ambient,candidate.diffuse,candidate.emission};return true;
}
}
