#pragma once
#include "threex3.h"
#include "talist.h"
#include "upstream_mesh.h"
#include <cstdint>
#include <vector>

namespace olex2::harmony {
// Explicit incoming/current attributes, not a hidden GL state or automatic
// face-normal generator. A caller may carry finalState into the next batch.
struct TriangleCommandState {
  std::array<float,3> normal{0,0,1};
  uint32_t color=0xffffffffu;
  bool forcePlain=false, selecting=false;
};
struct TriangleCommandVertex { MeshVertex geometry; uint32_t color; };
struct TriangleCommandBatch {
  std::vector<TriangleCommandVertex> vertices;
  TriangleCommandState finalState;
  unsigned normalCommands=0, colorRequests=0, colorCommands=0, selectionQueries=0;
};
// Strict complete-triangle input. The shared upstream header itself retains
// the legacy no-output branches for malformed attribute counts; this target
// boundary rejects such input instead of presenting a silent successful draw.
TriangleCommandBatch CapturePrimitiveTriangles(const TArrayList<vec3f>& vertices,
  const TArrayList<vec3f>& normals, const TArrayList<uint32_t>& colors,
  const TriangleCommandState& initial={});
// Current GLES material consumer does not implement color tracking. Preserve
// color commands in CPU captures, but never silently discard them on upload.
std::vector<MeshVertex> UncoloredTriangleVertices(const TriangleCommandBatch& batch);
struct OriginalTriangleUpload {
  std::vector<MeshVertex> vertices;
  unsigned normalCommands=0, colorCommands=0;
  bool verified=false;
};
OriginalTriangleUpload RouteMeshThroughOriginalTriangles(const MeshData& mesh);
constexpr const char* OriginalTriangleSource() {
  return "upstream/glib/trianglesubmit.h: original TGlPrimitive::Draw sgloTriangles branch";
}
}
