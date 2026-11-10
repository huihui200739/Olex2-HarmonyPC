// Synthetic geometry/state only. CPU command evidence is not GPU color tracking,
// picking, the complete TGlPrimitive class, or the original application GUI.
#include "primitive_triangles.h"
#include "primitive_batch.h"
#include "trianglesubmit.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
unsigned checks=0,cases=0;
void Check(bool ok,const char* message) {
  if (!ok) throw std::runtime_error(message);
  ++checks;
}
template<class F> void Reject(F action,const char* message) {
  bool failed=false;
  try { action(); } catch(const std::exception&) { failed=true; }
  Check(failed,message);
}
using namespace olex2::harmony;
struct TraceSink {
  const TArrayList<vec3f>& vertices;
  const TArrayList<uint32_t>& colors;
  TriangleCommandBatch output;
  TraceSink(const TArrayList<vec3f>& v,const TArrayList<uint32_t>& c,TriangleCommandState state)
    :vertices(v),colors(c) { output.finalState=state; }
  bool IsSelecting() { ++output.selectionQueries;return output.finalState.selecting; }
  void SetNormal(const vec3f& value) {
    ++output.normalCommands; output.finalState.normal={value[0],value[1],value[2]};
  }
  void DrawVertex(const vec3f& value) {
    output.vertices.push_back({{{value[0],value[1],value[2]},output.finalState.normal},output.finalState.color});
  }
  void DrawVertex(const vec3f& value,const uint32_t& color) {
    ++output.colorRequests;
    if (!output.finalState.forcePlain) { ++output.colorCommands;output.finalState.color=color; }
    DrawVertex(value);
  }
  void DrawVertex3(size_t i) { for(int j=0;j<3;++j) DrawVertex(vertices[i+j]); }
  void DrawVertex3c(size_t i) { for(int j=0;j<3;++j) DrawVertex(vertices[i+j],colors[i+j]); }
};
void ExhaustiveOriginalBranches() {
  for(size_t vertexCount=0;vertexCount<=17;++vertexCount)
    for(size_t normalCount=0;normalCount<=20;++normalCount)
      for(size_t colorCount=0;colorCount<=20;++colorCount)
        for(bool selecting:{false,true}) for(bool plain:{false,true}) {
          TArrayList<vec3f> vertices(vertexCount),normals(normalCount);
          TArrayList<uint32_t> colors(colorCount);
          for(size_t i=0;i<vertexCount;++i) vertices[i]=vec3f(float(i)*.1f,float(i)*.02f,-float(i)*.03f);
          for(size_t i=0;i<normalCount;++i) normals[i]=vec3f(float(i)+.25f,float(i)+.5f,1.f);
          for(size_t i=0;i<colorCount;++i) colors[i]=0xff000000u+static_cast<uint32_t>(i*773u);
          TriangleCommandState initial;initial.normal={.25f,.5f,1};initial.color=0x12345678u;
          initial.selecting=selecting;initial.forcePlain=plain;
          TraceSink sink(vertices,colors,initial);
          glObj::SubmitPrimitiveTriangles(vertices,normals,colors,sink);
          const auto& result=sink.output;
          // Independent cardinality/attribute oracle, including legacy invalid
          // count no-output branches. It does not copy the upstream if/loops.
          const bool usableNormals=normalCount==0 || normalCount==vertexCount || normalCount*3==vertexCount;
          const bool usableColors=colorCount==0 || selecting || colorCount==vertexCount;
          const size_t expectedVertices=usableNormals && usableColors ? vertexCount:0;
          const bool useColors=!selecting && colorCount!=0 && colorCount==vertexCount;
          const unsigned expectedNormals=expectedVertices && normalCount ? static_cast<unsigned>(normalCount):0;
          const unsigned expectedRequests=useColors?static_cast<unsigned>(expectedVertices):0;
          auto state=initial;
          bool same=result.vertices.size()==expectedVertices && result.normalCommands==expectedNormals &&
            result.colorRequests==expectedRequests && result.colorCommands==(plain?0:expectedRequests) &&
            result.selectionQueries==(colorCount?1u:0u);
          for(size_t i=0;i<expectedVertices && same;++i) {
            if(normalCount) {
              const auto& n=normals[normalCount==vertexCount?i:i/3];
              state.normal={n[0],n[1],n[2]};
            }
            if(useColors && !plain) state.color=colors[i];
            const auto& actual=result.vertices[i];
            same=actual.geometry.position==std::array<float,3>{vertices[i][0],vertices[i][1],vertices[i][2]} &&
              actual.geometry.normal==state.normal && actual.color==state.color;
          }
          same=same && result.finalState.normal==state.normal && result.finalState.color==state.color &&
            result.finalState.selecting==selecting && result.finalState.forcePlain==plain;
          Check(same,"original triangle branch event order, cardinality or inherited attributes changed");++cases;
        }
}
void StrictBoundaryAndState() {
  TArrayList<vec3f> v(6),n(2),empty;
  TArrayList<uint32_t> colors(6),noColors;
  for(size_t i=0;i<6;++i) { v[i]=vec3f(float(i)*.1f,.25f,.5f); colors[i]=0x00332211u+static_cast<uint32_t>(i); }
  n[0]=vec3f(0,0,1);n[1]=vec3f(0,1,0);
  TriangleCommandState initial;initial.normal={1,0,0};initial.color=0xabcdef01u;
  auto inherited=CapturePrimitiveTriangles(v,empty,noColors,initial);
  Check(inherited.normalCommands==0 && inherited.colorCommands==0,"no implicit normal/color commands");
  Check(inherited.vertices.front().geometry.normal==initial.normal,"missing normals inherit caller state, not generated face normals");
  Check(inherited.finalState.color==initial.color,"uncolored draw retains incoming current color");
  auto faces=CapturePrimitiveTriangles(v,n,noColors,initial);
  Check(faces.normalCommands==2 && faces.vertices.size()==6,"one normal command per original face group");
  Check(faces.vertices[2].geometry.normal==std::array<float,3>{0,0,1} && faces.vertices[3].geometry.normal==std::array<float,3>{0,1,0},"face normal changes at original triple boundary");
  auto carried=CapturePrimitiveTriangles(v,empty,noColors,faces.finalState);
  Check(carried.vertices.front().geometry.normal==faces.finalState.normal,"normal state carries across primitive batches");
  auto colored=CapturePrimitiveTriangles(v,n,colors,initial);
  Check(colored.colorRequests==6 && colored.colorCommands==6,"per-vertex color command count");
  Check(colored.finalState.color==colors[5],"last original color becomes current color");
  Check(colored.vertices[2].color==colors[2] && colored.vertices[3].color==colors[3],"face grouped path retains per-vertex colors");
  bool sameFacePositions=true;
  for(size_t i=0;i<6;++i) sameFacePositions=sameFacePositions && colored.vertices[i].geometry.position==std::array<float,3>{v[i][0],v[i][1],v[i][2]};
  Check(sameFacePositions,"native grouped helper preserves original vertex order");
  Reject([&]{UncoloredTriangleVertices(colored);},"GPU consumer must not silently discard color tracking commands");
  initial.forcePlain=true;auto plain=CapturePrimitiveTriangles(v,n,colors,initial);
  Check(plain.colorRequests==6 && plain.colorCommands==0,"ForcePlain suppresses SetColor effects, not branch requests");
  Check(plain.vertices.back().color==initial.color && UncoloredTriangleVertices(plain).size()==6,"plain batch can enter explicitly uncolored consumer");
  initial.forcePlain=false;initial.selecting=true;
  TArrayList<uint32_t> mismatchedColors(2);
  auto selected=CapturePrimitiveTriangles(v,n,mismatchedColors,initial);
  Check(selected.vertices.size()==6 && selected.colorRequests==0,"original selecting branch ignores color count and colors");
  auto emptyBatch=CapturePrimitiveTriangles(empty,empty,noColors,initial);
  Check(emptyBatch.vertices.empty() && emptyBatch.finalState.normal==initial.normal,"empty complete batch preserves state");
  TArrayList<vec3f> tail(4),badNormals(4);
  Reject([&]{CapturePrimitiveTriangles(tail,empty,noColors);},"target boundary rejects incomplete triangles");
  Reject([&]{CapturePrimitiveTriangles(v,badNormals,noColors);},"target boundary rejects unsupported normal count");
  Reject([&]{CapturePrimitiveTriangles(v,n,mismatchedColors);},"target boundary rejects unsupported non-selection color count");
  auto badV=v;badV[1][0]=std::numeric_limits<float>::quiet_NaN();
  Reject([&]{CapturePrimitiveTriangles(badV,n,noColors);},"target boundary rejects nonfinite vertex");
  auto badN=n;badN[0]=vec3f(0,0,0);
  Reject([&]{CapturePrimitiveTriangles(v,badN,noColors);},"target boundary rejects undefined normal direction");
  badN[0]=vec3f(0,std::numeric_limits<float>::infinity(),0);
  Reject([&]{CapturePrimitiveTriangles(v,badN,noColors);},"target boundary rejects nonfinite normal");
  initial.normal={0,0,0};
  Reject([&]{CapturePrimitiveTriangles(v,n,noColors,initial);},"target boundary requires defined inherited state");
  TArrayList<vec3f> oversized(200001);
  Reject([&]{CapturePrimitiveTriangles(oversized,empty,noColors);},"target boundary bounds allocation before submission");
  auto forged=inherited;forged.vertices.pop_back();
  Reject([&]{UncoloredTriangleVertices(forged);},"uncolored output independently rejects incomplete geometry");
  forged=inherited;forged.vertices[0].geometry.normal={0,0,0};
  Reject([&]{UncoloredTriangleVertices(forged);},"uncolored output independently checks normal state");
  forged=inherited;forged.vertices[0].color^=1u;
  Reject([&]{UncoloredTriangleVertices(forged);},"uncolored output rejects unexplained captured color changes");
  forged=inherited;forged.vertices[0].geometry.position[0]=std::numeric_limits<float>::infinity();
  Reject([&]{UncoloredTriangleVertices(forged);},"uncolored output independently checks vertex state");
}
void ActualMeshRouting() {
  for(auto kind:{MeshKind::Sphere,MeshKind::Torus,MeshKind::Disks,MeshKind::Rims})
    for(unsigned generation:{0u,3u,5u}) {
      auto mesh=BuildUpstreamPrimitive(kind,generation);
      auto result=RouteMeshThroughOriginalTriangles(mesh);
      Check(result.verified && result.normalCommands==mesh.submittedVertices.size() && result.colorCommands==0,"actual upstream mesh enters original triangle normal branch");
      bool same=result.vertices.size()==mesh.submittedVertices.size();
      for(size_t i=0;i<result.vertices.size() && same;++i)
        same=result.vertices[i].position==mesh.submittedVertices[i].position && result.vertices[i].normal==mesh.submittedVertices[i].normal;
      Check(same,"original source output preserves actual upload bytes and order");
    }
  MeshData empty;
  Reject([&]{RouteMeshThroughOriginalTriangles(empty);},"upload rejects unvalidated empty mesh");
  auto mesh=BuildUpstreamPrimitive(MeshKind::Sphere,0);mesh.submissionValidated=false;
  Reject([&]{RouteMeshThroughOriginalTriangles(mesh);},"upload requires previous shared geometry submission validation");
}
}
int main() {
  try {
    ExhaustiveOriginalBranches();StrictBoundaryAndState();ActualMeshRouting();
    std::cout<<"PASS original primitive triangle commands; "<<checks<<" assertions; "<<cases<<" branch-state cases; CPU color/selection traces only, GPU consumer explicitly uncolored; not complete renderer\n";
    return 0;
  } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
