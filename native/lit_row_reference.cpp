#include "lit_row_reference.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace olex2::harmony {
namespace {
struct Vertex { double x,y,z,w,diffuse,specular; };
double Edge(const Vertex& a,const Vertex& b,double x,double y) {
  return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
}
}
std::vector<RowLightingSample> ReferenceLightingRow(const MeshData& mesh,
    const std::array<float,16>& m,const std::array<float,16>& rotation,
    unsigned width,unsigned height,float shininess) {
  if(!width || width>16384 || !height || !std::isfinite(shininess) || shininess<0 || shininess>128 ||
      mesh.drawTriangles.size()%3) throw std::invalid_argument("invalid lighting reference bounds");
  for(float value:m) if(!std::isfinite(value)) throw std::invalid_argument("invalid reference matrix");
  for(float value:rotation) if(!std::isfinite(value)) throw std::invalid_argument("invalid reference rotation");
  const double lightLength=std::sqrt(.35*.35+.5*.5+1.);
  const std::array<double,3> light={.35/lightLength,.5/lightLength,1./lightLength};
  const double halfLength=std::sqrt(light[0]*light[0]+light[1]*light[1]+(light[2]+1)*(light[2]+1));
  const std::array<double,3> half={light[0]/halfLength,light[1]/halfLength,(light[2]+1)/halfLength};
  std::vector<Vertex> vertices;vertices.reserve(mesh.submittedVertices.size());
  for(const auto& input:mesh.submittedVertices) {
    const auto& p=input.position;const auto& n=input.normal;
    double clip[4]{},normal[3]{};
    for(unsigned row=0;row<4;++row) clip[row]=m[row]*p[0]+m[4+row]*p[1]+m[8+row]*p[2]+m[12+row];
    for(unsigned row=0;row<3;++row) normal[row]=rotation[row]*n[0]+rotation[4+row]*n[1]+rotation[8+row]*n[2];
    double norm=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    for(double value:clip) if(!std::isfinite(value)) throw std::invalid_argument("invalid reference vertex");
    if(clip[2]<-clip[3] || clip[2]>clip[3]) throw std::invalid_argument("reference does not implement near/far clipping");
    if(!std::isfinite(clip[3]) || clip[3]<=0 || !std::isfinite(norm) || norm==0)
      throw std::invalid_argument("reference requires finite nonzero normals and positive clip w");
    double d=0,h=0;for(unsigned i=0;i<3;++i){d+=normal[i]*light[i]/norm;h+=normal[i]*half[i]/norm;}
    d=std::max(d,0.);h=d>0?std::pow(std::max(h,0.),shininess):0;
    vertices.push_back({(clip[0]/clip[3]+1)*width*.5,(clip[1]/clip[3]+1)*height*.5,clip[2]/clip[3],clip[3],d,h});
  }
  std::vector<double> depths(width,std::numeric_limits<double>::infinity());
  std::vector<RowLightingSample> result(width);
  std::vector<bool> occupied(width,false),interior(width,false);
  const double row=height/2+.5;
  for(size_t i=0;i<mesh.drawTriangles.size();i+=3) {
    for(unsigned j=0;j<3;++j) if(mesh.drawTriangles[i+j]>=vertices.size()) throw std::invalid_argument("invalid reference index");
    const auto& a=vertices[mesh.drawTriangles[i]];const auto& b=vertices[mesh.drawTriangles[i+1]];const auto& c=vertices[mesh.drawTriangles[i+2]];
    if(row<std::min({a.y,b.y,c.y}) || row>std::max({a.y,b.y,c.y})) continue;
    const double area=Edge(a,b,c.x,c.y);if(std::abs(area)<1e-12) continue;
    const double minX=std::min({a.x,b.x,c.x}),maxX=std::max({a.x,b.x,c.x});
    if(maxX<0 || minX>width) continue;
    int begin=static_cast<int>(std::max(0.,std::min(double(width-1),std::floor(minX))));
    int end=static_cast<int>(std::max(0.,std::min(double(width-1),std::ceil(maxX))));
    for(int x=begin;x<=end;++x) {
      double u=Edge(b,c,x+.5,row)/area,v=Edge(c,a,x+.5,row)/area,w=1-u-v;
      if(u<0 || v<0 || w<0) continue;
      double z=u*a.z+v*b.z+w*c.z;if(z<-1 || z>1 || z>=depths[x]) continue;
      depths[x]=z;occupied[x]=true;
      // Exclude subpixel edge/tie rules and visible shared-edge ambiguity from
      // numerical pixel assertions, but still let those triangles occlude.
      const double distance=std::min({u*std::abs(area)/std::hypot(b.x-c.x,b.y-c.y),
        v*std::abs(area)/std::hypot(c.x-a.x,c.y-a.y),w*std::abs(area)/std::hypot(a.x-b.x,a.y-b.y)});
      interior[x]=distance>.05;
      const double inverse=u/a.w+v/b.w+w/c.w;
      result[x]={static_cast<unsigned>(x),static_cast<float>((u*a.diffuse/a.w+v*b.diffuse/b.w+w*c.diffuse/c.w)/inverse),
        static_cast<float>((u*a.specular/a.w+v*b.specular/b.w+w*c.specular/c.w)/inverse)};
    }
  }
  std::vector<RowLightingSample> selected;
  for(unsigned x=0;x<width;++x) if(occupied[x] && interior[x]) selected.push_back(result[x]);
  return selected;
}
}
