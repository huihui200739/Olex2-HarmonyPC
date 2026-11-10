#pragma once
#include "materialcommands.h"
#include <array>
#include <vector>

namespace olex2::harmony {
using MaterialCapability = glObj::MaterialCapability;
using MaterialFace = glObj::MaterialFace;
using MaterialField = glObj::MaterialField;
enum class MaterialOperation { Query, Enable, Disable, Parameter, ColorMode, Color };
struct MaterialCommand {
  MaterialOperation operation;
  MaterialCapability capability = MaterialCapability::AlphaTest;
  MaterialFace face = MaterialFace::Front;
  MaterialField field = MaterialField::Ambient;
  std::array<float,4> values{};
};
// A real backend-neutral command stream, not a set of no-op GL replacements.
// This does NOT by itself emulate a fixed-function graphics pipeline.
struct MaterialCommands {
  std::vector<MaterialCommand> operations;
  std::array<bool,6> enabled{};
  static const char* Source();
};
MaterialCommands BuildMaterialCommands(const glObj::TGlMaterial& material,
    const std::array<bool,6>& previousCapabilities = {}, bool skip = false);

struct LitMaterialParameters {
  std::array<float,3> ambient{}, diffuse{}, specular{}, emission{};
  float shininess=0;
};
bool ConsumeLitMaterial(const MaterialCommands&, LitMaterialParameters&);

struct OpaqueMaterialParameters {
  std::array<float,3> ambient{}, diffuse{}, emission{};
};
// Consume the subset the current GLES renderer actually supports. Reject any
// other state instead of silently accepting unsupported rendering commands.
bool ConsumeOpaqueMaterial(const MaterialCommands&, OpaqueMaterialParameters&);
}
