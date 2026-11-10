// Compile-only: the default desktop GL API and portable command API coexist.
// Function declarations below are not linked or executed, and are not GL stubs.
#include "materialcommands.h"
namespace desktop_material_compile {
struct DeclarationOnlySink {
  bool IsEnabled(glObj::MaterialCapability);
  void enable(glObj::MaterialCapability);
  void disable(glObj::MaterialCapability);
  void material(glObj::MaterialFace,glObj::MaterialField,const float*);
  void material(glObj::MaterialFace,glObj::MaterialField,float);
  void colorMaterial(glObj::MaterialFace,glObj::MaterialField);
  void color(const float*);
};
}
void CompileOriginalAndPortableMaterial(const glObj::TGlMaterial& value) {
  value.Init(false); // Original real desktop entry point remains declared.
  desktop_material_compile::DeclarationOnlySink sink;
  glObj::ApplyMaterialCommands(value,sink,false);
}
