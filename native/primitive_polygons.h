#pragma once
#include "primitive_triangles.h"
namespace olex2::harmony {
// New bounded single-contour planar tessellator, NOT GLU. Holes, intersections,
// nonplanar input and ill-conditioned/subprecision geometry are rejected.
struct PolygonTessellation {
  std::vector<uint32_t> triangles,outline;
  unsigned closingVertices=0;
  bool convex=false;
};
PolygonTessellation TessellateSimplePolygon(const std::vector<MeshVertex>& vertices,
                                          bool requireConvex=false);
TriangleCommandBatch CapturePrimitivePolygon(const TArrayList<vec3f>& vertices,
  const TArrayList<vec3f>& normals,const TArrayList<uint32_t>& colors,
  const TriangleCommandState& initial={});
struct OriginalPolygonTriangles {
  std::vector<MeshVertex> vertices; // expanded, then emitted by original sgloTriangles
  unsigned sourceVertices=0,triangles=0,normalCommands=0,colorCommands=0;
};
OriginalPolygonTriangles BuildUncoloredPolygonTriangles(const TArrayList<vec3f>& vertices,
  const TArrayList<vec3f>& normals,const TriangleCommandState& initial={});
constexpr const char* OriginalPolygonSource() {
  return "upstream/glib/polygonsubmit.h: original TGlPrimitive::Draw sgloPolygon branch";
}
}
