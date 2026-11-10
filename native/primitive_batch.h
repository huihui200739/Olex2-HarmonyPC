#pragma once
#include "upstream_mesh.h"
#include "primitivesubmit.h"
#include <cstddef>
namespace olex2::harmony {
// Bounded, explicitly supported subset. Not an implementation of desktop OpenGL.
class PrimitiveBatchBuilder {
public:
  explicit PrimitiveBatchBuilder(MeshData& mesh, bool preserveLegacyQuadTail=false)
    : mesh_(mesh), preserveLegacyQuadTail_(preserveLegacyQuadTail) {}
  void Begin(glObj::PrimitiveTopology topology);
  void BeginTriangles() { Begin(glObj::PrimitiveTopology::Triangles); }
  void Normal(double x, double y, double z);
  template<class V> void Normal(const V& v) { Normal(v[0],v[1],v[2]); }
  void Vertex(double x, double y, double z);
  template<class V> void Vertex(const V& v) { Vertex(v[0],v[1],v[2]); }
  void End();
  void Finish();
private:
  MeshData& mesh_;
  bool preserveLegacyQuadTail_=false, active_=false, hasNormal_=false;
  size_t start_=0;
  glObj::PrimitiveTopology topology_=glObj::PrimitiveTopology::Triangles;
  std::array<float,3> normal_{};
  void Triangle(uint32_t a,uint32_t b,uint32_t c);
  void Quad(uint32_t a,uint32_t b,uint32_t c,uint32_t d);
};
// Adapter-level geometry and expanded-batch checks; no GL context needed.
void ValidatePrimitiveMesh(const MeshData& mesh);
MeshData BuildUpstreamPrimitive(MeshKind kind,unsigned generation);
const char* MeshKindName(MeshKind kind);
}
