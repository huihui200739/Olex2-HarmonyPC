#include "primitive_lines.h"
#include "linesubmit.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace olex2::harmony {
namespace {
constexpr size_t MaxVertices=200000;
// Tags are carried through the original submission, never inferred from
// coordinate equality (distinct source vertices may occupy the same point).
struct TaggedPosition { const vec3f& value; size_t index; };
struct Positions {
  const TArrayList<vec3f>& values;
  size_t Count() const { return values.Count(); }
  TaggedPosition operator[](size_t i) const { return {values[i],i}; }
};
struct CaptureSink {
  const Positions& positions;
  LineCommandBatch& output;
  bool IsSelecting() { ++output.selectionQueries;return output.finalState.selecting; }
  void GetLineWidth(float* value) { ++output.widthQueries;*value=output.finalState.lineWidth; }
  void SetLineWidth(float value) {
    output.widthCommands.push_back(value);output.finalState.lineWidth=value;
  }
  void SetColor(const uint32_t& value) {
    ++output.colorRequests;
    if (!output.finalState.forcePlain) { output.finalState.color=value;++output.colorCommands; }
  }
  void DrawVertex(const TaggedPosition& position) {
    output.vertices.push_back({{position.value[0],position.value[1],position.value[2]},output.finalState.color,position.index});
  }
  void DrawVertex(const TaggedPosition& position,const uint32_t& color) {
    SetColor(color);DrawVertex(position);
  }
  void DrawVertex2(size_t i) { DrawVertex(positions[i]);DrawVertex(positions[i+1]); }
};
}
LineCommandBatch CapturePrimitiveLines(const TArrayList<vec3f>& vertices,
    const TArrayList<uint32_t>& colors,double widthMultiplier,const LineCommandState& initial) {
  const size_t count=vertices.Count();
  if (count>MaxVertices || count%2) throw std::invalid_argument("Line batch requires bounded complete pairs");
  if (colors.Count()>MaxVertices || (!initial.selecting && colors.Count()!=0 && colors.Count()!=count && colors.Count()!=count/2))
    throw std::invalid_argument("Line colors must be absent, per vertex or per pair");
  if (!std::isfinite(initial.lineWidth) || initial.lineWidth<=0 || !std::isfinite(widthMultiplier) || widthMultiplier<=0 ||
      !std::isfinite(widthMultiplier*initial.lineWidth) || widthMultiplier*initial.lineWidth>std::numeric_limits<float>::max() ||
      static_cast<float>(widthMultiplier*initial.lineWidth)<=0)
    throw std::invalid_argument("Line width requires a finite positive representable product");
  for (size_t i=0;i<count;++i) for (size_t j=0;j<3;++j)
    if (!std::isfinite(vertices[i][j])) throw std::invalid_argument("Nonfinite line position");
  LineCommandBatch result;result.finalState=initial;result.vertices.reserve(count);
  const Positions positions{vertices};const double params[]={widthMultiplier};CaptureSink sink{positions,result};
  glObj::SubmitPrimitiveLines(positions,colors,params,sink);
  if (result.vertices.size()!=count || result.finalState.lineWidth!=initial.lineWidth)
    throw std::runtime_error("Original line branch did not emit/restore the complete batch");
  return result;
}
OriginalLineUpload RouteMeshThroughOriginalLines(const MeshData& mesh) {
  if (!mesh.validated || !mesh.submissionValidated || mesh.drawLines.empty())
    throw std::invalid_argument("Original line upload requires a validated mesh and source edges");
  ValidateMesh(mesh);
  if (mesh.drawLines.size()>MaxVertices) throw std::invalid_argument("Line upload exceeds vertex budget");
  TArrayList<vec3f> positions(mesh.drawLines.size());TArrayList<uint32_t> colors;
  for (size_t i=0;i<mesh.drawLines.size();++i) {
    const auto& p=mesh.submittedVertices.at(mesh.drawLines[i]).position;
    positions[i]=vec3f(p[0],p[1],p[2]);
  }
  const auto batch=CapturePrimitiveLines(positions,colors);
  OriginalLineUpload result;result.indices.reserve(batch.vertices.size());
  result.vertices=static_cast<unsigned>(batch.vertices.size());
  result.colorCommands=batch.colorCommands;result.widthQueries=batch.widthQueries;
  result.widthCommands=static_cast<unsigned>(batch.widthCommands.size());
  if (batch.colorRequests || batch.colorCommands || batch.widthQueries || !batch.widthCommands.empty())
    throw std::runtime_error("Unsupported line attributes reached the GPU boundary");
  for (size_t i=0;i<batch.vertices.size();++i) {
    const auto& vertex=batch.vertices[i];
    const uint32_t index=mesh.drawLines.at(vertex.inputVertex);
    if (vertex.inputVertex!=i || vertex.position!=mesh.submittedVertices.at(index).position || vertex.color!=0xffffffffu)
      throw std::runtime_error("Original line submission changed source edge order/positions");
    result.indices.push_back(index);
  }
  if (result.indices!=mesh.drawLines) throw std::runtime_error("Original lines changed source edges");
  result.verified=true;return result;
}
}
