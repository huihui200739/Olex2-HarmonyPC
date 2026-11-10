#include "resource_font_resample.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace olex2::harmony {
namespace {
constexpr size_t MaxCellDimension=2048,MaxRGBFont=64u*1024u*1024u;
void Require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
size_t RGBBytes(unsigned w,unsigned h){
  Require(w>0&&h>0&&w<=MaxCellDimension&&h<=MaxCellDimension,"Invalid bounded resource RGB cell dimensions");
  return static_cast<size_t>(w)*h*3;
}
struct CubicSample{std::array<unsigned,4> indices;std::array<double,4> weights;};
double Cube(double x){return x<=0.0?0.0:x*x*x;}
double CubicWeight(double x){return (Cube(x+2)-4*Cube(x+1)+6*Cube(x)-4*Cube(x-1))/6;}
std::vector<CubicSample> CubicSamples(unsigned oldDim,unsigned newDim){
  std::vector<CubicSample> samples(newDim);
  const double ratio=newDim>1?static_cast<double>(oldDim-1)/(newDim-1):0;
  for(unsigned i=0;i<newDim;++i){
    const double coordinate=newDim>1?static_cast<double>(i)*ratio:static_cast<double>(oldDim-1)/2.0;
    const double fraction=coordinate-static_cast<int>(coordinate);
    for(int k=-1;k<=2;++k){
      const double at=coordinate+k;
      samples[i].indices[k+1]=at<0.0?0:at>=oldDim?oldDim-1:static_cast<unsigned>(at);
      samples[i].weights[k+1]=CubicWeight(k-fraction);
    }
  }
  return samples;
}
}
std::string ResourceFontPlan::Entry(const OlexResourceFontDescription& spec)const{
  Require(entryPoints>=1&&entryPoints<=128,"Invalid original resource entry plan");
  return spec.style+std::to_string(entryPoints);
}
ResourceFontPlan PlanResourceFont(unsigned effective){
  Require(effective>=1&&effective<=128,"Resource font requested size must be in [1,128]");
  const unsigned points=effective<=1?10:effective;
  return {points,points<8?8u:points>28?72u:points,points<8||points>28};
}
std::vector<unsigned char> ResizeResourceRGBCell(const std::vector<unsigned char>& source,
  unsigned oldWidth,unsigned oldHeight,unsigned width,unsigned height){
  Require(source.size()==RGBBytes(oldWidth,oldHeight),"Resource RGB source byte count differs");
  const auto bytes=RGBBytes(width,height);
  if(width==oldWidth&&height==oldHeight)return source;
  std::vector<unsigned char> result(bytes);
  if(width<oldWidth&&height<oldHeight){
    for(unsigned y=0;y<height;++y){
      const unsigned firstY=oldHeight*y/height,endY=(oldHeight*(y+1)+height-1)/height;
      for(unsigned x=0;x<width;++x){
        const unsigned firstX=oldWidth*x/width,endX=(oldWidth*(x+1)+width-1)/width;
        std::array<double,3> sums{};
        for(unsigned sy=firstY;sy<endY;++sy)for(unsigned sx=firstX;sx<endX;++sx)
          for(unsigned channel=0;channel<3;++channel)sums[channel]+=source[(static_cast<size_t>(sy)*oldWidth+sx)*3+channel];
        const unsigned count=(endY-firstY)*(endX-firstX);
        for(unsigned channel=0;channel<3;++channel)
          result[(static_cast<size_t>(y)*width+x)*3+channel]=static_cast<unsigned char>(sums[channel]/count);
      }
    }
  }else{
    const auto vertical=CubicSamples(oldHeight,height),horizontal=CubicSamples(oldWidth,width);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
      std::array<double,3> sums{};
      for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx){
        const double weight=vertical[y].weights[sy]*horizontal[x].weights[sx];
        const size_t at=(static_cast<size_t>(vertical[y].indices[sy])*oldWidth+horizontal[x].indices[sx])*3;
        for(unsigned channel=0;channel<3;++channel)sums[channel]+=source[at+channel]*weight;
      }
      for(unsigned channel=0;channel<3;++channel)
        result[(static_cast<size_t>(y)*width+x)*3+channel]=static_cast<unsigned char>(sums[channel]+0.5);
    }
  }
  return result;
}
OriginalFontCells ResampleResourceFont(const OlexResourceFontData& source,const ResourceFontPlan& plan){
  Require(plan.useRGB&&plan.points>=2&&plan.points<=128&&(plan.points<8||plan.points>28)&&
    plan.entryPoints==(plan.points<8?8u:72u),"Expected original outside8..28 resource scale plan");
  Require(source.bits.size()==static_cast<size_t>(source.width)*source.height*32,"Resource bit-array byte count differs");
  const size_t oldBytes=RGBBytes(source.width,source.height);
  const auto width=static_cast<unsigned>(std::floor(static_cast<double>(source.width)*plan.points/plan.entryPoints+0.5));
  const auto height=static_cast<unsigned>(std::floor(static_cast<double>(source.height)*plan.points/plan.entryPoints+0.5));
  const size_t bytes=RGBBytes(width,height);
  Require(bytes<=MaxRGBFont/256,"Resource RGB font exceeds private bounded cache");
  OriginalFontCells cells;cells.width=static_cast<uint16_t>(width);cells.height=static_cast<uint16_t>(height);
  std::vector<unsigned char> rgb(oldBytes);
  for(unsigned code=0;code<256;++code){
    std::fill(rgb.begin(),rgb.end(),0);
    for(unsigned y=0;y<source.height;++y)for(unsigned x=0;x<source.width;++x){
      const size_t index=(static_cast<size_t>(code)*source.height+y)*source.width+x;
      if(source.bits[index/8]&(1u<<(index%8)))rgb[(static_cast<size_t>(y)*source.width+x)*3]=255;
    }
    cells.rgb[code]=ResizeResourceRGBCell(rgb,source.width,source.height,width,height);
  }
  return cells;
}
}
