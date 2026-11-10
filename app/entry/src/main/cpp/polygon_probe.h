#pragma once
#include <GLES3/gl3.h>
#include <array>
#include <string>
struct PolygonGpuProbe {
  bool verified=false,stateRestored=false,contextRestored=false;
  unsigned inputVertices=0,triangles=0,normalCommands=0,colorCommands=0;
  // x,y,expected-inside,r,g,b,a. These are isolated synthetic FBO pixels,
  // never user data or a claim of full-scene/picking/GLU qualification.
  std::array<std::array<unsigned,7>,6> samples{};
  std::string error;
};
constexpr const char* PolygonGpuProbeSource() {
  return "synthetic U polygon -> original sgloPolygon -> simple-planar tessellation -> original sgloTriangles -> actual main-context GLES3 RGBA8 FBO readback";
}
PolygonGpuProbe RunOriginalPolygonGpuProbe(GLuint program);
