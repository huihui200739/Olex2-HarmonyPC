#include "primitive_polygons.h"
#include "polygonsubmit.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
namespace olex2::harmony {
namespace {
constexpr size_t MaxVertices=2048,WorkBudget=12000000;
constexpr double CoordinateTolerance=1.e-5,OrientationTolerance=1.e-11;
using P2=std::array<double,2>;
double Turn(const P2& a,const P2& b,const P2& c) {
  return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
}
bool OnSegment(const P2& p,const P2& a,const P2& b) {
  return std::abs(Turn(a,b,p))<=OrientationTolerance &&
    p[0]>=std::min(a[0],b[0])-OrientationTolerance && p[0]<=std::max(a[0],b[0])+OrientationTolerance &&
    p[1]>=std::min(a[1],b[1])-OrientationTolerance && p[1]<=std::max(a[1],b[1])+OrientationTolerance;
}
bool Intersects(const P2& a,const P2& b,const P2& c,const P2& d) {
  const double abC=Turn(a,b,c),abD=Turn(a,b,d),cdA=Turn(c,d,a),cdB=Turn(c,d,b);
  return ((abC>OrientationTolerance && abD<-OrientationTolerance || abC<-OrientationTolerance && abD>OrientationTolerance) &&
          (cdA>OrientationTolerance && cdB<-OrientationTolerance || cdA<-OrientationTolerance && cdB>OrientationTolerance)) ||
         OnSegment(c,a,b) || OnSegment(d,a,b) || OnSegment(a,c,d) || OnSegment(b,c,d);
}
void CheckNormal(const std::array<float,3>& n) {
  double length=0;
  for(float v:n) {if(!std::isfinite(v))throw std::invalid_argument("Nonfinite polygon normal");length+=double(v)*v;}
  if(length<1.e-12)throw std::invalid_argument("Undefined polygon normal");
}
struct PolygonSink {
  TriangleCommandBatch& output;
  bool IsSelecting() {++output.selectionQueries;return output.finalState.selecting;}
  void SetNormal(const vec3f& n) {output.finalState.normal={n[0],n[1],n[2]};++output.normalCommands;}
  void DrawVertex(const vec3f& v) {output.vertices.push_back({{{v[0],v[1],v[2]},output.finalState.normal},output.finalState.color});}
  void DrawVertex(const vec3f& v,const uint32_t& c) {
    ++output.colorRequests;
    if(!output.finalState.forcePlain) {output.finalState.color=c;++output.colorCommands;}
    DrawVertex(v);
  }
};
}
PolygonTessellation TessellateSimplePolygon(const std::vector<MeshVertex>& input,bool requireConvex) {
  if(input.size()<3 || input.size()>MaxVertices)throw std::invalid_argument("Polygon requires 3..2048 bounded vertices");
  std::array<double,3> low{},high{};
  for(size_t k=0;k<3;++k)low[k]=high[k]=input.front().position[k];
  for(const auto& v:input)for(size_t k=0;k<3;++k) {
    if(!std::isfinite(v.position[k]))throw std::invalid_argument("Nonfinite polygon position");
    low[k]=std::min(low[k],double(v.position[k]));high[k]=std::max(high[k],double(v.position[k]));
  }
  const double span=std::max({high[0]-low[0],high[1]-low[1],high[2]-low[2]});
  if(!std::isfinite(span) || span<=0)throw std::invalid_argument("Degenerate polygon extent");
  std::vector<std::array<double,3>> points;
  for(const auto& v:input) {
    std::array<double,3> p{};for(size_t k=0;k<3;++k)p[k]=(double(v.position[k])-low[k])/span;
    points.push_back(p);
  }
  const auto close=[&](size_t a,size_t b) {double d=0;for(size_t k=0;k<3;++k)d+=std::pow(points[a][k]-points[b][k],2);return d<CoordinateTolerance*CoordinateTolerance;};
  PolygonTessellation out;size_t count=points.size();
  if(close(0,count-1)) {--count;out.closingVertices=1;}
  if(count<3)throw std::invalid_argument("Polygon has fewer than three distinct boundary vertices");
  for(size_t i=0;i<count;++i)if(close(i,(i+1)%count))throw std::invalid_argument("Subprecision or repeated polygon edge");
  std::array<double,3> normal{};
  for(size_t i=0;i<count;++i)for(size_t k=0;k<3;++k)
    normal[k]+=points[i][(k+1)%3]*points[(i+1)%count][(k+2)%3]-points[i][(k+2)%3]*points[(i+1)%count][(k+1)%3];
  const double length=std::hypot(normal[0],normal[1],normal[2]);
  if(length<1.e-10)throw std::invalid_argument("Degenerate or crossing polygon plane");
  for(double& v:normal)v/=length;
  for(const auto& p:points) {
    double distance=0;for(size_t k=0;k<3;++k)distance+=(p[k]-points[0][k])*normal[k];
    if(std::abs(distance)>2.e-6)throw std::invalid_argument("Only planar polygons are supported");
  }
  size_t drop=0;for(size_t k=1;k<3;++k)if(std::abs(normal[k])>std::abs(normal[drop]))drop=k;
  std::vector<P2> p;for(size_t i=0;i<count;++i)p.push_back({points[i][(drop+1)%3],points[i][(drop+2)%3]});
  size_t work=0;
  for(size_t i=0;i<count;++i)for(size_t j=i+1;j<count;++j) {
    if(++work>WorkBudget)throw std::length_error("Polygon validation budget exceeded");
    if(j==i+1 || (i==0 && j==count-1))continue;
    if(Intersects(p[i],p[(i+1)%count],p[j],p[(j+1)%count]))throw std::invalid_argument("Polygon boundary crosses or touches itself");
  }
  double area=0;for(size_t i=0;i<count;++i)area+=p[i][0]*p[(i+1)%count][1]-p[(i+1)%count][0]*p[i][1];
  if(std::abs(area)<=OrientationTolerance)throw std::invalid_argument("Degenerate projected polygon");
  const double sign=area>0?1:-1;bool strictConvex=true;out.convex=true;
  for(size_t i=0;i<count;++i) {
    const double turn=sign*Turn(p[i],p[(i+1)%count],p[(i+2)%count]);
    if(turn<-OrientationTolerance)out.convex=false;
    if(turn<=OrientationTolerance)strictConvex=false;
  }
  if(requireConvex && !out.convex)throw std::invalid_argument("Convex-only helper cannot accept a concave polygon");
  out.outline.resize(count);std::iota(out.outline.begin(),out.outline.end(),0u);
  if(strictConvex) {
    // Keep existing helper fan/diagonal order exactly for strictly convex input.
    for(uint32_t i=1;i+1<count;++i)out.triangles.insert(out.triangles.end(),{0,i,i+1});
    return out;
  }
  std::vector<uint32_t> ring=out.outline;
  while(ring.size()>3) {
    bool clipped=false;
    for(size_t i=0;i<ring.size();++i) {
      const uint32_t a=ring[(i+ring.size()-1)%ring.size()],b=ring[i],c=ring[(i+1)%ring.size()];
      if(sign*Turn(p[a],p[b],p[c])<=OrientationTolerance)continue;
      bool occupied=false;
      for(uint32_t v:ring) {
        if(++work>WorkBudget)throw std::length_error("Polygon tessellation budget exceeded");
        if(v==a || v==b || v==c)continue;
        if(sign*Turn(p[a],p[b],p[v])>=-OrientationTolerance && sign*Turn(p[b],p[c],p[v])>=-OrientationTolerance && sign*Turn(p[c],p[a],p[v])>=-OrientationTolerance) {occupied=true;break;}
      }
      if(occupied)continue;
      out.triangles.insert(out.triangles.end(),{a,b,c});ring.erase(ring.begin()+i);clipped=true;break;
    }
    if(!clipped)throw std::invalid_argument("Ill-conditioned polygon cannot be tessellated without degeneracy");
  }
  if(sign*Turn(p[ring[0]],p[ring[1]],p[ring[2]])<=OrientationTolerance)throw std::invalid_argument("Degenerate final polygon triangle");
  out.triangles.insert(out.triangles.end(),ring.begin(),ring.end());
  if(out.triangles.size()!=3*(count-2))throw std::runtime_error("Incomplete polygon tessellation");
  return out;
}
TriangleCommandBatch CapturePrimitivePolygon(const TArrayList<vec3f>& vertices,const TArrayList<vec3f>& normals,
    const TArrayList<uint32_t>& colors,const TriangleCommandState& initial) {
  const size_t count=vertices.Count();
  if(count<3 || count>MaxVertices)throw std::invalid_argument("Polygon requires 3..2048 bounded vertices");
  if(normals.Count()>1)throw std::invalid_argument("Polygon normals must be absent or one inherited face normal");
  if(!initial.selecting && colors.Count()!=0 && colors.Count()!=count)throw std::invalid_argument("Polygon colors must be absent or per vertex");
  CheckNormal(initial.normal);
  for(size_t i=0;i<count;++i)for(size_t k=0;k<3;++k)if(!std::isfinite(vertices[i][k]))throw std::invalid_argument("Nonfinite polygon position");
  if(normals.Count())CheckNormal({normals[0][0],normals[0][1],normals[0][2]});
  TriangleCommandBatch out;out.finalState=initial;out.vertices.reserve(count);PolygonSink sink{out};
  glObj::SubmitPrimitivePolygon(vertices,normals,colors,sink);
  if(out.vertices.size()!=count)throw std::runtime_error("Original polygon did not emit a complete boundary");
  return out;
}
OriginalPolygonTriangles BuildUncoloredPolygonTriangles(const TArrayList<vec3f>& vertices,const TArrayList<vec3f>& normals,const TriangleCommandState& initial) {
  TArrayList<uint32_t> colors;const auto captured=CapturePrimitivePolygon(vertices,normals,colors,initial);
  std::vector<MeshVertex> geometry;for(const auto& v:captured.vertices)geometry.push_back(v.geometry);
  const auto tess=TessellateSimplePolygon(geometry);
  TArrayList<vec3f> triVertices,triNormals;
  for(uint32_t index:tess.triangles) {
    const auto& v=geometry[index];triVertices.Add(vec3f(v.position[0],v.position[1],v.position[2]));triNormals.Add(vec3f(v.normal[0],v.normal[1],v.normal[2]));
  }
  const auto triangles=CapturePrimitiveTriangles(triVertices,triNormals,colors,captured.finalState);
  OriginalPolygonTriangles out;out.vertices=UncoloredTriangleVertices(triangles);out.sourceVertices=vertices.Count();out.triangles=out.vertices.size()/3;
  out.normalCommands=captured.normalCommands;out.colorCommands=captured.colorCommands;return out;
}
}
