// Isolated synthetic geometry only. No application model or research dataset.
#include "primitive_polygons.h"
#include "polygonsubmit.h"
#include "primitive_batch.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
using namespace olex2::harmony;
unsigned checks=0,branches=0,poses=0;
void Check(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);++checks;}
template<class F>void Reject(F action,const char* text) {bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected,text);}
struct TraceSink {
  std::vector<std::string> trace;bool selecting=false,forcePlain=false;
  bool IsSelecting(){trace.push_back("select");return selecting;}
  void SetNormal(const vec3f& n){trace.push_back("normal:"+std::to_string(int(n[0])));}
  void DrawVertex(const vec3f& v){trace.push_back("vertex:"+std::to_string(int(v[0])));}
  void DrawVertex(const vec3f& v,const uint32_t& c){trace.push_back("color-request:"+std::to_string(c));if(!forcePlain)trace.push_back("color:"+std::to_string(c));DrawVertex(v);}
};
void OriginalBranches() {
  for(size_t nv=0;nv<=8;++nv)for(size_t nn=0;nn<=4;++nn)for(size_t nc=0;nc<=10;++nc)for(bool selecting:{false,true})for(bool plain:{false,true}) {
    TArrayList<vec3f> v,n;TArrayList<uint32_t> c;
    for(size_t i=0;i<nv;++i)v.Add(vec3f(i,0,0));for(size_t i=0;i<nn;++i)n.Add(vec3f(i+1,0,1));for(size_t i=0;i<nc;++i)c.Add(100+i);
    TraceSink sink;sink.selecting=selecting;sink.forcePlain=plain;glObj::SubmitPrimitivePolygon(v,n,c,sink);
    std::vector<std::string> expected;
    if(nn==1)expected.push_back("normal:1");if(nc)expected.push_back("select");
    if(nc==0 || selecting || nc==nv)for(size_t i=0;i<nv;++i) {
      if(nc && !selecting) {expected.push_back("color-request:"+std::to_string(100+i));if(!plain)expected.push_back("color:"+std::to_string(100+i));}
      expected.push_back("vertex:"+std::to_string(i));
    }
    Check(sink.trace==expected,"original polygon branch order/query/color trace mismatch");++branches;
  }
}
std::vector<std::array<double,2>> Shape() {return {{-.8,-.8},{.8,-.8},{.8,.8},{.2,.8},{.2,-.2},{-.2,-.2},{-.2,.8},{-.8,.8}};}
double Cross(const std::array<double,2>& a,const std::array<double,2>& b,const std::array<double,2>& c) {return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);}
bool Boundary(const std::array<double,2>& p,const std::array<double,2>& a,const std::array<double,2>& b) {
  return std::abs(Cross(a,b,p))<1.e-7 && p[0]>=std::min(a[0],b[0])-1.e-7 && p[0]<=std::max(a[0],b[0])+1.e-7 && p[1]>=std::min(a[1],b[1])-1.e-7 && p[1]<=std::max(a[1],b[1])+1.e-7;
}
bool Inside(const std::vector<std::array<double,2>>& polygon,const std::array<double,2>& p) {
  bool in=false;for(size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
    const auto& a=polygon[i];const auto& b=polygon[j];
    if((a[1]>p[1])!=(b[1]>p[1]) && p[0]<(b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0])in=!in;
  }return in;
}
void TessellationPoses() {
  for(unsigned plane=0;plane<4;++plane)for(double scale:{.001,1.,1000.})for(bool reverse:{false,true})for(bool closing:{false,true}) {
    auto shape=Shape();if(reverse)std::reverse(shape.begin(),shape.end());std::vector<MeshVertex> v;
    for(auto p:shape) {
      std::array<float,3> q{};
      if(plane<3) {q[(plane+1)%3]=float(p[0]*scale+3*scale);q[(plane+2)%3]=float(p[1]*scale-2*scale);q[plane]=float(.25*scale);}
      else q={float(p[0]*scale),float(p[1]*scale),float((p[0]+p[1])*.5*scale)};
      v.push_back({q,{0,0,1}});
    }
    if(closing)v.push_back(v.front());auto tess=TessellateSimplePolygon(v);
    bool valid=tess.triangles.size()==18 && tess.outline.size()==8 && tess.closingVertices==unsigned(closing) && !tess.convex;
    double polygonArea=0,triangleArea=0;
    for(size_t i=0;i<shape.size();++i)polygonArea+=shape[i][0]*shape[(i+1)%shape.size()][1]-shape[(i+1)%shape.size()][0]*shape[i][1];
    for(size_t i=0;i<tess.triangles.size();i+=3) {
      const auto a=tess.triangles[i],b=tess.triangles[i+1],c=tess.triangles[i+2];valid=valid && a<8 && b<8 && c<8;
      const double area=Cross(shape[a],shape[b],shape[c]);triangleArea+=area;valid=valid && area*polygonArea>0;
    }
    valid=valid && std::abs(polygonArea-triangleArea)<1.e-7;
    // Independent ray crossing versus actual triangle coverage. Ignore only
    // points on polygon/triangle boundaries; reject overlaps elsewhere.
    for(int x=-17;x<=17;++x)for(int y=-17;y<=17;++y) {
      std::array<double,2> p{double(x)/19,double(y)/19};bool edge=false;unsigned covered=0;
      for(size_t i=0;i<shape.size();++i)edge=edge || Boundary(p,shape[i],shape[(i+1)%shape.size()]);
      for(size_t i=0;i<tess.triangles.size();i+=3) {
        const auto& a=shape[tess.triangles[i]];const auto& b=shape[tess.triangles[i+1]];const auto& c=shape[tess.triangles[i+2]];
        edge=edge || Boundary(p,a,b) || Boundary(p,b,c) || Boundary(p,c,a);
        const double s=polygonArea>0?1:-1;
        if(s*Cross(a,b,p)>1.e-7 && s*Cross(b,c,p)>1.e-7 && s*Cross(c,a,p)>1.e-7)++covered;
      }
      if(!edge)valid=valid && covered==unsigned(Inside(shape,p));
    }
    Check(valid,"concave tessellation differs from independent area/winding/coverage reference");++poses;
  }
}
std::vector<MeshVertex> Geometry(std::initializer_list<std::array<float,3>> p) {std::vector<MeshVertex> out;for(auto v:p)out.push_back({v,{0,0,1}});return out;}
void BoundariesAndRouting() {
  auto square=Geometry({{0,0,0},{1,0,0},{1,1,0},{0,1,0}});auto t=TessellateSimplePolygon(square,true);
  Check(t.convex && t.triangles==std::vector<uint32_t>({0,1,2,0,2,3}),"convex helper preserves exact fan and winding");
  auto collinear=Geometry({{0,0,0},{.5f,0,0},{1,0,0},{1,1,0},{0,1,0}});
  Check(TessellateSimplePolygon(collinear).triangles.size()==9,"collinear boundary vertices retained without zero-area output");
  Reject([&]{TessellateSimplePolygon(Geometry({{0,0,0},{1,1,0},{0,1,0},{1,0,0}}));},"self-crossing polygon rejected");
  Reject([&]{TessellateSimplePolygon(Geometry({{0,0,0},{2,0,0},{1,0,0},{1,1,0},{0,1,0}}));},"overlapping boundary rejected");
  Reject([&]{auto bad=square;bad[2].position[2]=.1f;TessellateSimplePolygon(bad);},"nonplanar polygon rejected");
  Reject([&]{auto bad=square;bad[2].position[0]=std::numeric_limits<float>::infinity();TessellateSimplePolygon(bad);},"nonfinite polygon rejected");
  Reject([&]{auto bad=square;bad.insert(bad.begin()+1,bad.front());TessellateSimplePolygon(bad);},"adjacent duplicate rejected");
  Reject([&]{auto bad=square;bad.push_back(bad.front());bad.push_back(bad.front());TessellateSimplePolygon(bad);},"multiple closing duplicates rejected");
  Reject([&]{TessellateSimplePolygon(Geometry({{0,0,0},{1,0,0},{2,0,0}}));},"zero area polygon rejected");
  Reject([&]{std::vector<MeshVertex> bad(2049);TessellateSimplePolygon(bad);},"explicit polygon input budget");
  TArrayList<vec3f> v,n;TArrayList<uint32_t> colors;for(auto p:Shape())v.Add(vec3f(p[0],p[1],0));n.Add(vec3f(0,0,1));
  auto route=BuildUncoloredPolygonTriangles(v,n);
  Check(route.sourceVertices==8 && route.triangles==6 && route.vertices.size()==18 && route.normalCommands==1 && route.colorCommands==0,"original polygon/tessellation/original triangle chain");
  for(size_t i=0;i<v.Count();++i)colors.Add(0xff000000u+i);auto captured=CapturePrimitivePolygon(v,n,colors);
  Check(captured.colorCommands==8 && captured.colorRequests==8 && captured.normalCommands==1 && captured.finalState.color==colors[7],"original colored boundary commands retained on CPU only");
  TriangleCommandState plain;plain.forcePlain=true;captured=CapturePrimitivePolygon(v,n,colors,plain);
  Check(captured.colorRequests==8 && captured.colorCommands==0 && captured.finalState.color==plain.color,"original ForcePlain suppression");
  plain.selecting=true;colors.SetCount(2);captured=CapturePrimitivePolygon(v,n,colors,plain);
  Check(captured.colorRequests==0 && captured.vertices.size()==8,"original selecting bypasses malformed color count without implementing picking");
  plain.selecting=false;Reject([&]{CapturePrimitivePolygon(v,n,colors,plain);},"target rejects malformed nonselecting colors");
  colors.Clear();n.Add(vec3f(1,0,0));Reject([&]{CapturePrimitivePolygon(v,n,colors);},"target explicitly rejects unsupported polygon normal count");
  n.Clear();plain.normal={0,0,2};captured=CapturePrimitivePolygon(v,n,colors,plain);
  Check(captured.normalCommands==0 && captured.vertices[0].geometry.normal==plain.normal,"missing polygon normal inherits caller state instead of generating a face normal");
  plain.normal={0,0,0};Reject([&]{CapturePrimitivePolygon(v,n,colors,plain);},"undefined inherited normal rejected");
  for(unsigned detail=0;detail<=5;++detail) {
    const auto mesh=BuildUpstreamPrimitive(MeshKind::Disks,detail);
    Check(mesh.originalPolygonVerified && mesh.originalPolygonBatches==6 && mesh.originalPolygonVertices==6*(8*(detail+1)+1) && mesh.originalPolygonNormalCommands==6 && mesh.originalPolygonColorCommands==0,"actual disk helper boundaries flow through original polygon branch");
  }
}
}
int main() {try {OriginalBranches();TessellationPoses();BoundariesAndRouting();std::cout<<"PASS original polygon and simple-planar tessellation; "<<checks<<" assertions; "<<branches<<" branch-state cases; "<<poses<<" independent topology poses; not GLU/holes/full renderer\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
