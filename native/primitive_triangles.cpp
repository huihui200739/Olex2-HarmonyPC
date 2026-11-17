#include "primitive_triangles.h"
#include "trianglesubmit.h"
#include <cmath>
#include <stdexcept>
namespace olex2::harmony {
namespace {
constexpr size_t MaxVertices=200000;
void ValidateNormal(const std::array<float,3>& value) {
  double length=0;
  for (float item:value) {
    if (!std::isfinite(item)) throw std::invalid_argument("Nonfinite triangle normal");
    length+=static_cast<double>(item)*item;
  }
  if (length<1.e-12) throw std::invalid_argument("Triangle normal requires a defined direction");
}
struct CaptureSink {
  const TArrayList<vec3f>& vertices;
  const TArrayList<uint32_t>& colors;
  TriangleCommandBatch& output;
  bool IsSelecting() { ++output.selectionQueries; return output.finalState.selecting; }
  void SetNormal(const vec3f& value) {
    output.finalState.normal={value[0],value[1],value[2]};
    ++output.normalCommands;
  }
  void DrawVertex(const vec3f& value) {
    output.vertices.push_back({{{value[0],value[1],value[2]},output.finalState.normal},output.finalState.color});
  }
  void DrawVertex(const vec3f& value,const uint32_t& color) {
    ++output.colorRequests;
    if (!output.finalState.forcePlain) {
      output.finalState.color=color; ++output.colorCommands;
    }
    DrawVertex(value);
  }
  void DrawVertex3(size_t i) { for (int j=0;j<3;++j) DrawVertex(vertices[i+j]); }
  void DrawVertex3c(size_t i) { for (int j=0;j<3;++j) DrawVertex(vertices[i+j],colors[i+j]); }
};
}
TriangleCommandBatch CapturePrimitiveTriangles(const TArrayList<vec3f>& vertices,
    const TArrayList<vec3f>& normals,const TArrayList<uint32_t>& colors,
    const TriangleCommandState& initial) {
  const size_t count=vertices.Count();
  if (count>MaxVertices || count%3) throw std::invalid_argument("Triangle batch requires a bounded complete vertex count");
  if (normals.Count()!=0 && normals.Count()!=count && normals.Count()!=count/3)
    throw std::invalid_argument("Triangle normals must be absent, per vertex or per face");
  if (!initial.selecting && colors.Count()!=0 && colors.Count()!=count)
    throw std::invalid_argument("Triangle colors must be absent or per vertex");
  ValidateNormal(initial.normal);
  for (size_t i=0;i<count;++i) for (size_t k=0;k<3;++k)
    if (!std::isfinite(vertices[i][k])) throw std::invalid_argument("Nonfinite triangle position");
  for (size_t i=0;i<normals.Count();++i)
    ValidateNormal({normals[i][0],normals[i][1],normals[i][2]});
  TriangleCommandBatch result;
  result.finalState=initial; result.vertices.reserve(count);
  CaptureSink sink{vertices,colors,result};
  glObj::SubmitPrimitiveTriangles(vertices,normals,colors,sink);
  if (result.vertices.size()!=count) throw std::runtime_error("Original triangle branch did not emit the complete batch");
  return result;
}
std::vector<MeshVertex> UncoloredTriangleVertices(const TriangleCommandBatch& batch) {
  if (batch.colorCommands) throw std::invalid_argument("Vertex color tracking is not implemented by the GLES material consumer");
  if (batch.vertices.size()>MaxVertices || batch.vertices.size()%3)
    throw std::invalid_argument("Invalid uncolored triangle batch length");
  std::vector<MeshVertex> vertices; vertices.reserve(batch.vertices.size());
  for (const auto& vertex:batch.vertices) {
    if (vertex.color!=batch.finalState.color)
      throw std::invalid_argument("Uncolored triangle batch contains unexplained color changes");
    ValidateNormal(vertex.geometry.normal);
    for (float item:vertex.geometry.position)
      if (!std::isfinite(item)) throw std::invalid_argument("Nonfinite uncolored triangle position");
    vertices.push_back(vertex.geometry);
  }
  return vertices;
}
OriginalTriangleUpload RouteMeshThroughOriginalTriangles(const MeshData& mesh) {
  if (!mesh.validated || !mesh.submissionValidated || mesh.submittedVertices.empty() || mesh.submittedVertices.size()>MaxVertices)
    throw std::invalid_argument("Original triangle upload requires a validated mesh stream");
  const size_t count=mesh.submittedVertices.size();
  TArrayList<vec3f> vertices(count),normals(count);
  TArrayList<uint32_t> colors;
  for (size_t i=0;i<count;++i) {
    const auto& source=mesh.submittedVertices[i];
    vertices[i]=vec3f(source.position[0],source.position[1],source.position[2]);
    normals[i]=vec3f(source.normal[0],source.normal[1],source.normal[2]);
  }
  const auto batch=CapturePrimitiveTriangles(vertices,normals,colors);
  OriginalTriangleUpload output;
  output.vertices=UncoloredTriangleVertices(batch);
  output.normalCommands=batch.normalCommands; output.colorCommands=batch.colorCommands;
  bool same=output.vertices.size()==count && output.normalCommands==count && output.colorCommands==0;
  for (size_t i=0;i<count && same;++i)
    same=output.vertices[i].position==mesh.submittedVertices[i].position && output.vertices[i].normal==mesh.submittedVertices[i].normal;
  if (!same) throw std::runtime_error("Original triangle submission changed the actual mesh stream");
  output.verified=true; return output;
}
}
