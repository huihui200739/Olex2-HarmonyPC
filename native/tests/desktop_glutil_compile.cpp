// Compile-only check: retain the original desktop API. Never creates a GL context.
#include "glutil.h"
#include "gloption.h"
#include "glmaterial.h"
void CompileOriginalSphereDesktopAPI() {
  glObj::GlSphereEx<float,OctahedronFP<vec3f>>::Render(.9f,1);
  glObj::GlSphereEx<float,OctahedronFP<vec3f>>::RenderEx(.9f,1,vec3f(-1,-1,0),vec3f(1,1,1));
  glObj::GlSphereEx<double,OctahedronFP<vec3d>>::Render(.9,1);
  glObj::GlSphereEx<double,OctahedronFP<vec3d>>::RenderEx(.9,1,vec3d(-1,-1,0),vec3d(1,1,1));
  glObj::GlSphereEx<float,OctahedronFP<vec3f>> helper;
  helper.RenderDisks(.8f,16,.45f); helper.RenderRims(.8f,16,.45f);
  glObj::GlSphereEx<double,OctahedronFP<vec3d>> helperDouble;
  helperDouble.RenderDisks(.8,16,.45); helperDouble.RenderRims(.8,16,.45);
  glObj::GlTorus::Render(.25,.65,8,16);
}
