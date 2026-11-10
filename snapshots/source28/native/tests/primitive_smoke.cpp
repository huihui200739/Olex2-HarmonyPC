// Isolated synthetic geometry/unit-test data. No application model or GL context.
#include "primitive_batch.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
unsigned checks=0;
void Check(bool ok,const std::string& name) {
  if(!ok)throw std::runtime_error(name);
  ++checks;std::cout<<"PASS "<<name<<'\n';
}
template<class F> void Reject(F f,const char* name) {
  bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected,name);
}
bool Near(float a,float b){return std::fabs(a-b)<2.e-5f;}
}
int main() {
  using namespace olex2::harmony;using glObj::PrimitiveTopology;
  try {
    for(auto kind:{MeshKind::Torus,MeshKind::Disks,MeshKind::Rims})for(unsigned detail=0;detail<=5;++detail) {
      const auto mesh=BuildUpstreamPrimitive(kind,detail);const size_t t=8*(detail+1);
      const std::string label=std::string(MeshKindName(kind))+" detail "+std::to_string(detail);
      const size_t triangles=kind==MeshKind::Torus?t*t:kind==MeshKind::Disks?6*(t-2):6*t+2;
      const size_t lineIndices=kind==MeshKind::Disks?12*t:triangles*4;
      const size_t vertices=kind==MeshKind::Torus?t*(t+1):kind==MeshKind::Disks?6*(t+1):12*t+8;
      Check(mesh.validated&&mesh.submissionValidated,label+" validated upstream helper batch");
      Check(mesh.vertices.size()==vertices&&mesh.triangles.size()==triangles*3,label+" independent source/triangle counts");
      Check(mesh.submittedVertices.size()==triangles*3&&mesh.drawLines.size()==lineIndices,label+" expanded triangle and line counts");
      Check(mesh.sourceBatches==(kind==MeshKind::Torus?t/2:kind==MeshKind::Disks?6:3),label+" original batch boundaries");
      Check(mesh.discardedTailVertices==(kind==MeshKind::Rims?4:0)&&mesh.degenerateTrianglesSkipped==(kind==MeshKind::Disks?6:0),label+" explicit legacy tails and closing duplicates");
      bool analytical=true;
      for(const auto& v:mesh.vertices) {
        const float x=v.position[0],y=v.position[1],z=v.position[2];
        if(kind==MeshKind::Torus) {
          const float radial=std::sqrt(x*x+y*y);analytical=analytical&&Near((radial-.65f)*(radial-.65f)+z*z,.25f*.25f);
        } else {
          size_t axial=0;for(size_t k=1;k<3;++k)if(std::fabs(v.normal[k])>std::fabs(v.normal[axial]))axial=k;
          if(kind==MeshKind::Disks) {
            float radial=0;for(size_t k=0;k<3;++k)if(k!=axial)radial+=v.position[k]*v.position[k];
            analytical=analytical&&Near(std::fabs(v.position[axial]),.225f)&&Near(radial,.64f);
          } else {
            bool onCylinder=false;
            for(size_t axis=0;axis<3;++axis) {
              float radial=0;for(size_t k=0;k<3;++k)if(k!=axis)radial+=v.position[k]*v.position[k];
              onCylinder=onCylinder||(Near(v.normal[axis],0)&&Near(std::fabs(v.position[axis]),.225f)&&Near(radial,.64f));
            }
            analytical=analytical&&onCylinder;
          }
        }
      }
      Check(analytical,label+" analytical surface equations");
    }
    { MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Quads);b.Normal(0,0,1);
      b.Vertex(0,0,0);b.Vertex(1,0,0);b.Vertex(1,1,0);b.Vertex(0,1,0);b.End();b.Finish();
      Check(m.triangles==std::vector<uint32_t>({0,1,2,0,2,3}),"synthetic quad keeps winding and diagonal"); }
    { MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::QuadStrip);b.Normal(0,0,1);
      b.Vertex(0,0,0);b.Vertex(0,1,0);b.Vertex(1,0,0);b.Vertex(1,1,0);b.End();b.Finish();
      Check(m.triangles==std::vector<uint32_t>({0,1,3,0,3,2}),"synthetic quad strip uses original GL vertex order"); }
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Vertex(0,0,0);},"isolated synthetic vertex outside batch rejected");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Triangles);b.Vertex(0,0,0);},"isolated synthetic missing normal rejected");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Triangles);b.Normal(0,0,0);},"isolated synthetic zero normal rejected");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Triangles);b.Normal(0,0,1);b.Vertex(0,0,0);b.End();},"isolated synthetic incomplete triangles rejected");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Quads);b.Normal(0,0,1);b.Vertex(0,0,0);b.End();},"isolated synthetic quad tail rejected without explicit legacy policy");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::Triangles);b.Finish();},"isolated synthetic unfinished batch rejected");
    Reject([]{BuildUpstreamPrimitive(MeshKind::Torus,6);},"unsupported detail rejected before upstream helper");
    Reject([]{BuildUpstreamPrimitive(static_cast<MeshKind>(9),1);},"unsupported primitive rejected before upstream helper");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::ConvexPolygon);b.Normal(0,0,1);
      b.Vertex(0,0,0);b.Vertex(1,0,0);b.Vertex(.3,.3,0);b.Vertex(1,1,0);b.Vertex(0,1,0);b.End();},"isolated synthetic concave polygon requires a separate tessellator");
    Reject([]{MeshData m;PrimitiveBatchBuilder b(m);b.Begin(PrimitiveTopology::ConvexPolygon);b.Normal(0,0,1);
      b.Vertex(0,0,0);b.Vertex(1,0,0);b.Vertex(1,1,1);b.Vertex(0,1,0);b.End();},"isolated synthetic nonplanar polygon rejected");
    auto mesh=BuildUpstreamPrimitive(MeshKind::Torus,1);mesh.submittedVertices[0].normal[0]+=.5f;
    Reject([&]{ValidatePrimitiveMesh(mesh);},"isolated synthetic inconsistent expanded normal rejected");
    mesh=BuildUpstreamPrimitive(MeshKind::Disks,1);mesh.drawLines[0]=9999;
    Reject([&]{ValidatePrimitiveMesh(mesh);},"isolated synthetic inconsistent expanded edge rejected");
    std::cout<<"PASS upstream helper primitive adapter only; "<<checks<<" assertions; not the complete renderer\n";return 0;
  }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
