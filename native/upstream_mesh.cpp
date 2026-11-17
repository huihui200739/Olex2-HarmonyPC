#include "upstream_mesh.h"
#include "primitive_batch.h"
#include "esphere.h"
#include "spheresubmit.h"
#include "exception.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace olex2::harmony {
namespace {
float Dot(const std::array<float, 3>& a, const std::array<float, 3>& b) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
// Converts a strict triangle stream into a real GLES-compatible indexed batch.
// An incomplete or inconsistent submission never becomes a drawable mesh.
struct TriangleBatchSink {
  MeshData& mesh;
  std::array<float,3> normal{};
  bool active=false, normalPending=false, ended=false;
  void BeginTriangles() {
    if (active || ended) throw std::runtime_error("Unexpected triangle batch start");
    active=true;
  }
  void Normal(const vec3f& value) {
    if (!active || normalPending) throw std::runtime_error("Unexpected triangle normal");
    normal={value[0],value[1],value[2]}; normalPending=true;
  }
  void Vertex(const vec3f& value) {
    if (!active || !normalPending) throw std::runtime_error("Triangle vertex has no normal");
    const auto index=static_cast<uint32_t>(mesh.submittedVertices.size());
    mesh.submittedVertices.push_back({{value[0],value[1],value[2]},normal});
    mesh.drawTriangles.push_back(index); normalPending=false;
    if (mesh.drawTriangles.size()%3==0)
      mesh.drawLines.insert(mesh.drawLines.end(),{index-2,index-1,index-1,index,index,index-2});
  }
  void End() {
    if (!active || normalPending || mesh.drawTriangles.size()%3!=0)
      throw std::runtime_error("Incomplete triangle batch");
    active=false; ended=true;
  }
};

}
void ValidateMesh(const MeshData& mesh) {
  if(mesh.kind!=MeshKind::Sphere) { ValidatePrimitiveMesh(mesh); return; }
  if (mesh.generation > 5 || !std::isfinite(mesh.radius) || mesh.radius < .01f || mesh.radius > 10.f)
    throw std::invalid_argument("Unsupported sphere parameters");
  const size_t faces = size_t(8) << (2*mesh.generation);
  const size_t vertices = faces - 2;
  if (mesh.vertices.size() != vertices || mesh.triangles.size() != faces*3 || mesh.lines.size() != faces*6)
    throw std::runtime_error("Upstream sphere counts do not match the octahedron subdivision contract");
  for (const auto& v : mesh.vertices) {
    for (size_t i=0; i<3; ++i) {
      if (!std::isfinite(v.position[i]) || !std::isfinite(v.normal[i]))
        throw std::runtime_error("Sphere contains non-finite vertex data");
      if (std::fabs(v.position[i] - v.normal[i]*mesh.radius) > 1.e-4f)
        throw std::runtime_error("Sphere position does not match its normal and radius");
    }
    if (std::fabs(Dot(v.position,v.position)-mesh.radius*mesh.radius) > 1.e-4f ||
        std::fabs(Dot(v.normal,v.normal)-1.f) > 1.e-4f)
      throw std::runtime_error("Sphere radius or unit normal check failed");
  }
  for (size_t i=0; i<mesh.triangles.size(); i+=3) {
    uint32_t index[3] = {mesh.triangles[i],mesh.triangles[i+1],mesh.triangles[i+2]};
    for (auto value : index) if (value >= mesh.vertices.size())
      throw std::runtime_error("Sphere triangle index is out of range");
    const auto& a=mesh.vertices[index[0]].position;
    const auto& b=mesh.vertices[index[1]].position;
    const auto& c=mesh.vertices[index[2]].position;
    std::array<float,3> ab{},ac{};
    for (size_t k=0;k<3;++k) { ab[k]=b[k]-a[k]; ac[k]=c[k]-a[k]; }
    const std::array<float,3> cross={ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]};
    if (!(Dot(cross,a)>1.e-9f)) throw std::runtime_error("Sphere triangle is degenerate or not outward-facing");
    const size_t line=i*2;
    const uint32_t expected[]={index[0],index[1],index[1],index[2],index[2],index[0]};
    for (size_t k=0;k<6;++k) if (mesh.lines[line+k] != expected[k])
      throw std::runtime_error("Wire indices do not correspond to sphere triangle edges");
  }
  if (mesh.submittedVertices.size()!=mesh.triangles.size() ||
      mesh.drawTriangles.size()!=mesh.triangles.size() || mesh.drawLines.size()!=mesh.lines.size())
    throw std::runtime_error("Shared GlSphereEx submission count mismatch");
  for (size_t i=0;i<mesh.triangles.size();++i) {
    const auto& original=mesh.vertices[mesh.triangles[i]];
    const auto& submitted=mesh.submittedVertices[i];
    if (original.position!=submitted.position || original.normal!=submitted.normal || mesh.drawTriangles[i]!=i)
      throw std::runtime_error("Shared GlSphereEx submission changed vertex or normal order");
  }
  for (size_t i=0;i<mesh.drawLines.size();i+=6) {
    const uint32_t base=static_cast<uint32_t>(i/2);
    const uint32_t expected[]={base,base+1,base+1,base+2,base+2,base};
    for (size_t k=0;k<6;++k) if (mesh.drawLines[i+k]!=expected[k])
      throw std::runtime_error("Submitted triangle edge mismatch");
  }

}
MeshData BuildUpstreamSphere(unsigned generation, float radius) {
  if (generation>5 || !std::isfinite(radius) || radius<.01f || radius>10.f)
    throw std::invalid_argument("Sphere generation must be 0..5 and radius 0.01..10");
  TTypeList<vec3f> vertices;
  TTypeList<IndexTriangle> triangles;
  TArrayList<vec3f> normals;
  try { OlxSphere<float,OctahedronFP<vec3f>>::Generate(radius,generation,vertices,triangles,normals); }
  catch (const TBasicException&) { throw std::runtime_error("Upstream sphere generation failed"); }
  if (vertices.Count()>std::numeric_limits<uint32_t>::max())
    throw std::runtime_error("Sphere exceeds 32-bit index range");
  MeshData mesh;
  mesh.generation=generation; mesh.radius=radius;
  mesh.source="upstream/sdl/esphere.h: OlxSphere<float, OctahedronFP<vec3f>>::Generate";
  mesh.vertices.reserve(vertices.Count()); mesh.triangles.reserve(triangles.Count()*3); mesh.lines.reserve(triangles.Count()*6);
  for (size_t i=0;i<vertices.Count();++i)
    mesh.vertices.push_back({{vertices[i][0],vertices[i][1],vertices[i][2]}, {normals[i][0],normals[i][1],normals[i][2]}});
  for (size_t i=0;i<triangles.Count();++i) {
    const uint32_t a=static_cast<uint32_t>(triangles[i][0]),b=static_cast<uint32_t>(triangles[i][1]),c=static_cast<uint32_t>(triangles[i][2]);
    mesh.triangles.insert(mesh.triangles.end(),{a,b,c});
    mesh.lines.insert(mesh.lines.end(),{a,b,b,c,c,a});
  }
  mesh.submissionSource="upstream/glib/spheresubmit.h: SphereTriangleSubmission::Render (shared with GlSphereEx)";
  mesh.submittedVertices.reserve(triangles.Count()*3);
  mesh.drawTriangles.reserve(triangles.Count()*3); mesh.drawLines.reserve(triangles.Count()*6);
  TriangleBatchSink sink{mesh};
  glObj::SphereTriangleSubmission<float>::Render(vertices,triangles,normals,sink);
  if (!sink.ended) throw std::runtime_error("Shared submission did not finish");
  mesh.sourceBatches=1;
  ValidateMesh(mesh); mesh.validated=true; mesh.submissionValidated=true;
  return mesh;
}
}
