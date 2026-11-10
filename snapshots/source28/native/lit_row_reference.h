#pragma once
#include "upstream_mesh.h"
#include <array>
#include <vector>
namespace olex2::harmony {
struct RowLightingSample {
  unsigned x=0;
  float diffuse=0,specular=0;
};
// Independent CPU reference for interior pixels of the actual submitted solid
// triangles. Perspective-correct interpolation matches the defined new shader,
// not a claim to implement the complete legacy OpenGL lighting pipeline.
std::vector<RowLightingSample> ReferenceLightingRow(const MeshData& mesh,
    const std::array<float,16>& mvp,const std::array<float,16>& rotation,
    unsigned width,unsigned height,float shininess);
}
