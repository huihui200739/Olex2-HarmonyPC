#include "primitive_batch.h"
#include "primitive_polygons.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace olex2::harmony {
namespace {
constexpr size_t MaxSourceVertices=200000,MaxTriangles=200000;
float AreaSquared(const MeshVertex& a,const MeshVertex& b,const MeshVertex& c) {
  std::array<float,3> ab{},ac{},cross{};
  for (size_t k=0;k<3;++k) { ab[k]=b.position[k]-a.position[k]; ac[k]=c.position[k]-a.position[k]; }
  cross={ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]};
  return cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2];
}
}
const char* MeshKindName(MeshKind kind) {
  switch(kind) { case MeshKind::Sphere:return "sphere"; case MeshKind::Torus:return "torus";
    case MeshKind::Disks:return "disks"; case MeshKind::Rims:return "rims"; }
  throw std::invalid_argument("Unsupported primitive kind");
}
void PrimitiveBatchBuilder::Begin(glObj::PrimitiveTopology topology) {
  if (active_) throw std::runtime_error("Primitive batch already active");
  switch(topology) { case glObj::PrimitiveTopology::Triangles:case glObj::PrimitiveTopology::Quads:
    case glObj::PrimitiveTopology::QuadStrip:case glObj::PrimitiveTopology::ConvexPolygon:break;
    default:throw std::invalid_argument("Unsupported primitive topology"); }
  topology_=topology;start_=mesh_.vertices.size();active_=true;hasNormal_=false;
}
void PrimitiveBatchBuilder::Normal(double x,double y,double z) {
  if (!active_) throw std::runtime_error("Normal outside a primitive batch");
  const double length=x*x+y*y+z*z;
  if (!std::isfinite(length) || length<1.e-12 || length>100.) throw std::invalid_argument("Invalid primitive normal");
  normal_={static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)};hasNormal_=true;
}
void PrimitiveBatchBuilder::Vertex(double x,double y,double z) {
  if (!active_ || !hasNormal_) throw std::runtime_error("Primitive vertex requires an active batch and normal");
  if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::max({std::fabs(x),std::fabs(y),std::fabs(z)})>10.)
    throw std::invalid_argument("Invalid primitive position");
  if (mesh_.vertices.size()>=MaxSourceVertices) throw std::length_error("Primitive source budget exceeded");
  mesh_.vertices.push_back({{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)},normal_});
}
void PrimitiveBatchBuilder::Triangle(uint32_t a,uint32_t b,uint32_t c) {
  // Polygon helpers close their circles with a repeated vertex. Count, do not
  // submit, the corresponding zero-area fan triangle.
  if (AreaSquared(mesh_.vertices[a],mesh_.vertices[b],mesh_.vertices[c])<=1.e-12f) {
    ++mesh_.degenerateTrianglesSkipped;return;
  }
  if (mesh_.triangles.size()/3>=MaxTriangles) throw std::length_error("Primitive triangle budget exceeded");
  mesh_.triangles.insert(mesh_.triangles.end(),{a,b,c});
  for (auto index:{a,b,c}) {
    mesh_.submittedVertices.push_back(mesh_.vertices[index]);
    mesh_.drawTriangles.push_back(static_cast<uint32_t>(mesh_.drawTriangles.size()));
  }

}
void PrimitiveBatchBuilder::Quad(uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
  Triangle(a,b,c);Triangle(a,c,d);
  mesh_.lines.insert(mesh_.lines.end(),{a,b,b,c,c,d,d,a});
}
void PrimitiveBatchBuilder::End() {
  if (!active_) throw std::runtime_error("Primitive batch not active");
  const size_t count=mesh_.vertices.size()-start_;
  const auto at=[&](size_t i){return static_cast<uint32_t>(start_+i);};
  switch(topology_) {
    case glObj::PrimitiveTopology::Triangles:
      if(count%3) throw std::runtime_error("Incomplete triangle batch");
      for(size_t i=0;i<count;i+=3) {
        Triangle(at(i),at(i+1),at(i+2));
        mesh_.lines.insert(mesh_.lines.end(),{at(i),at(i+1),at(i+1),at(i+2),at(i+2),at(i)});
      }break;
    case glObj::PrimitiveTopology::Quads:
      if(count%4 && !preserveLegacyQuadTail_)throw std::runtime_error("Incomplete quad batch");
      mesh_.discardedTailVertices+=static_cast<unsigned>(count%4);
      for(size_t i=0;i+3<count;i+=4)Quad(at(i),at(i+1),at(i+2),at(i+3));break;
    case glObj::PrimitiveTopology::QuadStrip:
      if(count<4 || count%2)throw std::runtime_error("Incomplete quad strip");
      for(size_t i=0;i+3<count;i+=2)Quad(at(i),at(i+1),at(i+3),at(i+2));break;
    case glObj::PrimitiveTopology::ConvexPolygon:
      if(count<3)throw std::runtime_error("Incomplete convex polygon");
      {
        TArrayList<vec3f> vertices,normals;TArrayList<uint32_t> colors;
        const auto& first=mesh_.vertices[at(0)].normal;
        normals.Add(vec3f(first[0],first[1],first[2]));
        for(size_t i=0;i<count;++i) {
          const auto& v=mesh_.vertices[at(i)];
          if(v.normal!=first)throw std::invalid_argument("Original polygon branch requires one face/current normal");
          vertices.Add(vec3f(v.position[0],v.position[1],v.position[2]));
        }
        const auto original=CapturePrimitivePolygon(vertices,normals,colors);
        std::vector<MeshVertex> geometry;
        for(const auto& v:original.vertices)geometry.push_back(v.geometry);
        const auto tess=TessellateSimplePolygon(geometry,true);
        // Actual source-helper boundaries now pass through the same original
        // sgloPolygon branch used by desktop before any tessellation/upload.
        for(size_t i=0;i<count;++i)mesh_.vertices[at(i)]=geometry[i];
        for(size_t i=0;i<tess.outline.size();++i)
          mesh_.lines.insert(mesh_.lines.end(),{at(tess.outline[i]),at(tess.outline[(i+1)%tess.outline.size()])});
        for(size_t i=0;i<tess.triangles.size();i+=3)
          Triangle(at(tess.triangles[i]),at(tess.triangles[i+1]),at(tess.triangles[i+2]));
        mesh_.degenerateTrianglesSkipped+=tess.closingVertices;
        ++mesh_.originalPolygonBatches;mesh_.originalPolygonVertices+=count;
        mesh_.originalPolygonNormalCommands+=original.normalCommands;mesh_.originalPolygonColorCommands+=original.colorCommands;
        mesh_.originalPolygonVerified=true;
      }
      break;
  }
  ++mesh_.sourceBatches;active_=false;
}
void PrimitiveBatchBuilder::Finish() {
  if(active_ || !mesh_.sourceBatches)throw std::runtime_error("Primitive submission did not finish");
  // Wireframe follows the original quad/polygon boundaries. Triangulation
  // diagonals and polygon-fan spokes are not invented as visible edges.
  std::vector<uint32_t> rawToDraw(mesh_.vertices.size(),UINT32_MAX);
  for(size_t i=0;i<mesh_.triangles.size();++i)if(rawToDraw[mesh_.triangles[i]]==UINT32_MAX)
    rawToDraw[mesh_.triangles[i]]=static_cast<uint32_t>(i);
  mesh_.drawLines.clear();mesh_.drawLines.reserve(mesh_.lines.size());
  for(auto index:mesh_.lines) {
    if(index>=rawToDraw.size() || rawToDraw[index]==UINT32_MAX)
      throw std::runtime_error("Primitive boundary has no emitted vertex");
    mesh_.drawLines.push_back(rawToDraw[index]);
  }
}
void ValidatePrimitiveMesh(const MeshData& mesh) {
  if(mesh.kind==MeshKind::Sphere)throw std::invalid_argument("Use the sphere-specific mesh validator");
  MeshKindName(mesh.kind);
  if(mesh.generation>5 || mesh.vertices.empty() || mesh.vertices.size()>MaxSourceVertices ||
      mesh.triangles.empty() || mesh.triangles.size()%3 || mesh.triangles.size()/3>MaxTriangles ||
      mesh.submittedVertices.size()!=mesh.triangles.size() || mesh.drawTriangles.size()!=mesh.triangles.size() ||
      mesh.lines.empty() || mesh.lines.size()%2 || mesh.drawLines.size()!=mesh.lines.size() || !mesh.sourceBatches)
    throw std::runtime_error("Inconsistent primitive batch sizes");
  const unsigned expectedPolygonVertices=6*(8*(mesh.generation+1)+1);
  if(mesh.kind==MeshKind::Disks) {
    if(!mesh.originalPolygonVerified || mesh.originalPolygonBatches!=6 || mesh.originalPolygonVertices!=expectedPolygonVertices || mesh.originalPolygonNormalCommands!=6 || mesh.originalPolygonColorCommands)
      throw std::runtime_error("Original disk polygon submission was not verified");
  } else if(mesh.originalPolygonVerified || mesh.originalPolygonBatches || mesh.originalPolygonVertices || mesh.originalPolygonNormalCommands || mesh.originalPolygonColorCommands)
    throw std::runtime_error("Unexpected original polygon provenance on non-polygon helper");
  for(const auto& v:mesh.vertices) {
    float length=0;
    for(size_t k=0;k<3;++k) {
      if(!std::isfinite(v.position[k])||!std::isfinite(v.normal[k])||std::fabs(v.position[k])>10.f)
        throw std::runtime_error("Invalid primitive vertex");
      length+=v.normal[k]*v.normal[k];
    }
    if(length<1.e-12f || length>100.f)throw std::runtime_error("Invalid primitive normal length");
  }
  for(size_t i=0;i<mesh.triangles.size();i+=3) {
    for(size_t k=0;k<3;++k) {
      const auto index=mesh.triangles[i+k];
      if(index>=mesh.vertices.size())throw std::runtime_error("Primitive index outside source buffer");
      const auto& a=mesh.vertices[index];const auto& b=mesh.submittedVertices[i+k];
      if(a.position!=b.position||a.normal!=b.normal||mesh.drawTriangles[i+k]!=i+k)
        throw std::runtime_error("Primitive stream correspondence failed");
    }
    if(AreaSquared(mesh.vertices[mesh.triangles[i]],mesh.vertices[mesh.triangles[i+1]],mesh.vertices[mesh.triangles[i+2]])<=1.e-12f)
      throw std::runtime_error("Degenerate submitted primitive triangle");

  }
  for(size_t i=0;i<mesh.lines.size();++i) {
    if(mesh.lines[i]>=mesh.vertices.size() || mesh.drawLines[i]>=mesh.submittedVertices.size())
      throw std::runtime_error("Primitive boundary index outside vertex buffer");
    const auto& raw=mesh.vertices[mesh.lines[i]];const auto& draw=mesh.submittedVertices[mesh.drawLines[i]];
    if(raw.position!=draw.position||raw.normal!=draw.normal)
      throw std::runtime_error("Primitive boundary correspondence failed");
  }

}
MeshData BuildUpstreamPrimitive(MeshKind kind,unsigned generation) {
  if(generation>5)throw std::invalid_argument("Primitive detail must be 0..5");
  if(kind==MeshKind::Sphere)return BuildUpstreamSphere(generation);
  MeshKindName(kind);MeshData mesh;mesh.kind=kind;mesh.generation=generation;mesh.radius=.9f;
  PrimitiveBatchBuilder sink(mesh,true);const size_t segments=8*(generation+1);
  if(kind==MeshKind::Torus) {
    glObj::TorusPrimitiveSubmission::Render(.25,.65,segments/2,segments,sink);
    mesh.source="upstream/glib/primitivesubmit.h: GlTorus::Render extracted loop";
  } else if(kind==MeshKind::Disks) {
    glObj::DiskPrimitiveSubmission<float>::RenderDisks(.8f,segments,.45f,sink);
    mesh.source="upstream/glib/primitivesubmit.h: GlSphereEx::RenderDisks extracted loops";
  } else {
    glObj::DiskPrimitiveSubmission<float>::RenderRims(.8f,segments,.45f,sink);
    mesh.source="upstream/glib/primitivesubmit.h: GlSphereEx::RenderRims extracted loops";
  }
  sink.Finish();mesh.submissionSource=mesh.source;
  ValidatePrimitiveMesh(mesh);mesh.validated=true;mesh.submissionValidated=true;return mesh;
}
}
