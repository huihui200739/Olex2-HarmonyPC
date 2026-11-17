#pragma once
#include "threex3.h"
#include "talist.h"
#include "upstream_mesh.h"
#include <cstdint>
#include <vector>

namespace olex2::harmony {
// CPU command capture of sgloLines only. Normals/materials/begin-end are not
// part of this branch and are not synthesized here. Width changes are traces,
// not a claim that arbitrary desktop widths are supported on the target GPU.
struct LineCommandState {
  uint32_t color=0xffffffffu;
  float lineWidth=1;
  bool forcePlain=false, selecting=false;
};
struct LineCommandVertex {
  std::array<float,3> position;
  uint32_t color;
  size_t inputVertex;
};
struct LineCommandBatch {
  std::vector<LineCommandVertex> vertices;
  LineCommandState finalState;
  std::vector<float> widthCommands;
  unsigned widthQueries=0, colorRequests=0, colorCommands=0, selectionQueries=0;
};
LineCommandBatch CapturePrimitiveLines(const TArrayList<vec3f>& vertices,
  const TArrayList<uint32_t>& colors, double widthMultiplier=1,
  const LineCommandState& initial={});
struct OriginalLineUpload {
  std::vector<uint32_t> indices;
  unsigned vertices=0, colorCommands=0, widthQueries=0, widthCommands=0;
  bool verified=false;
};
// Uncolored multiplier-one path only. The existing verified triangle stream
// remains the attribute buffer; original line output determines the index
// order. Existing per-vertex normals/shader lighting are retained, not claimed
// equivalent to fixed-function sgloLines incoming-normal state.
OriginalLineUpload RouteMeshThroughOriginalLines(const MeshData& mesh);
constexpr const char* OriginalLineSource() {
  return "upstream/glib/linesubmit.h: original TGlPrimitive::Draw sgloLines branch";
}
}
