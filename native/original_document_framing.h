#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace olex2::harmony::framing {
using Point=std::array<double,3>;
using Matrix=std::array<Point,3>;
using Projection=std::array<float,16>;

// Plain window math, not a second scene/model. The caller supplies the original
// visible world-object bounds, original centre, and the default camera matrix.
struct Bounds {
  Point minimum{},maximum{};
  bool populated=false;
  void AddBox(const Point& low,const Point& high,const Point& centre,const Matrix& rotation){
    for(unsigned i=0;i<3;++i){
      if(!std::isfinite(low[i])||!std::isfinite(high[i])||low[i]>high[i]||!std::isfinite(centre[i]))
        throw std::runtime_error("Invalid original framing bounds/centre");
      for(double x:rotation[i])if(!std::isfinite(x))throw std::runtime_error("Invalid original framing rotation");
    }
    for(unsigned corner=0;corner<8;++corner){
      Point p{},view{};
      for(unsigned i=0;i<3;++i)p[i]=((corner&(1u<<i))?high[i]:low[i])+centre[i];
      // Same row-vector convention as TGlRenderer::Project: (v+centre)*matrix.
      for(unsigned j=0;j<3;++j){
        for(unsigned i=0;i<3;++i)view[j]+=p[i]*rotation[i][j];
        if(!std::isfinite(view[j]))throw std::runtime_error("Original framing bounds overflow");
      }
      if(!populated){minimum=maximum=view;populated=true;}
      else for(unsigned i=0;i<3;++i){minimum[i]=std::min(minimum[i],view[i]);maximum[i]=std::max(maximum[i],view[i]);}
    }
  }
};

// Fit about the existing original centre; do not introduce pan/roll or change
// relative camera-distance semantics. Read the real GL projection, including
// its depth planes, rather than assume a framebuffer size or rebuild glOrtho.
inline double FitZoom(const Bounds& bounds,int width,int height,const Projection& projection,
    double eyeOriginZ,double originalZoom){
  if(width<=0||height<=0||!bounds.populated||!std::isfinite(originalZoom)||originalZoom<=0||!std::isfinite(eyeOriginZ))
    throw std::runtime_error("Invalid original framing viewport/basis");
  for(unsigned i=0;i<16;++i){
    if(!std::isfinite(projection[i]))throw std::runtime_error("Invalid original framing projection");
    if(i!=0&&i!=5&&i!=10&&i!=12&&i!=13&&i!=14&&i!=15&&projection[i]!=0)
      throw std::runtime_error("Original framing needs an orthographic projection");
  }
  if(projection[0]<=0||projection[5]<=0||projection[10]>=0||projection[15]!=1)
    throw std::runtime_error("Original framing projection is not supported");
  const double aspect=static_cast<double>(width)/height;
  if(std::abs(static_cast<double>(projection[5])/projection[0]/aspect-1)>1e-5)
    throw std::runtime_error("Original framing projection differs from the real viewport aspect");
  // An explicit composition policy: 5% of each viewport edge for x/y, 1% of
  // each depth-clip edge. These are clip-space fractions, never fake pixels.
  const Point limit{0.90,0.90,0.98};
  const Point origin{projection[12],projection[13],projection[14]+projection[10]*eyeOriginZ};
  double zoom=originalZoom; // only reduce overfilling; never enlarge a small scene
  for(unsigned axis=0;axis<3;++axis){
    if(!std::isfinite(bounds.minimum[axis])||!std::isfinite(bounds.maximum[axis])||bounds.minimum[axis]>bounds.maximum[axis]||
        !std::isfinite(origin[axis])||std::abs(origin[axis])>=limit[axis])
      throw std::runtime_error("Original framing origin/bounds cannot fit the projection");
    for(double endpoint:{bounds.minimum[axis],bounds.maximum[axis]}){
      const double slope=projection[axis*5]*endpoint;
      if(!std::isfinite(slope))throw std::runtime_error("Original framing projection overflow");
      if(slope>0)zoom=std::min(zoom,(limit[axis]-origin[axis])/slope);
      else if(slope<0)zoom=std::min(zoom,(-limit[axis]-origin[axis])/slope);
    }
  }
  if(!std::isfinite(zoom)||zoom<=0)throw std::runtime_error("Original framing zoom cannot be represented");
  return zoom;
}
}
