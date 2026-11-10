#pragma once
#include "primitive_lines.h"
namespace olex2::harmony {
constexpr size_t MaximumOriginalLinePathVertices=200000;
enum class OriginalLinePathKind { Strip,Loop };
struct OriginalLinePathBatch {
  std::vector<LineCommandVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<float> widthCommands;
  LineCommandState finalState;
  unsigned widthQueries=0,selectionQueries=0,colorRequests=0,colorCommands=0;
};
// Explicit target boundary: strip 2..200000, loop 3..200000 finite vertices.
// Raw legacy empty/short/malformed-color semantics remain in the shared header.
// Color/width/selection traces are CPU behavior, not target GPU feature claims.
OriginalLinePathBatch CaptureOriginalLinePath(
  const std::vector<std::array<float,3>>& vertices,
  const std::vector<uint32_t>& colors,OriginalLinePathKind kind,
  double multiplier=1,const LineCommandState& initial={});
constexpr const char* OriginalLinePathSource(){
  return "upstream/glib/linepathsubmit.h: original TGlPrimitive::Draw sgloLineStrip/sgloLineLoop branches; strict target input and consecutive/closed edge topology";
}
}
