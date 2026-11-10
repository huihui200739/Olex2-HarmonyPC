#include "resource_font_resample.h"
#include <iostream>
#include <stdexcept>
using namespace olex2::harmony;
std::vector<unsigned char> OriginalWxHigh(const std::vector<unsigned char>&,unsigned,unsigned,unsigned,unsigned);
int main(){
 size_t cases=0,bytes=0;
 for(unsigned ow=1;ow<=8;++ow)for(unsigned oh=1;oh<=8;++oh){
  std::vector<unsigned char> source(ow*oh*3);
  for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x)for(unsigned channel=0;channel<3;++channel)
   source[(y*ow+x)*3+channel]=static_cast<unsigned char>((x*43+y*67+channel*89+(x^y)*17)%256);
  for(unsigned w=1;w<=10;++w)for(unsigned h=1;h<=10;++h){
   const auto actual=ResizeResourceRGBCell(source,ow,oh,w,h),expected=OriginalWxHigh(source,ow,oh,w,h);
   if(actual!=expected)throw std::runtime_error("Original source RGB mismatch "+std::to_string(ow)+"x"+std::to_string(oh)+" -> "+std::to_string(w)+"x"+std::to_string(h));
   ++cases;bytes+=actual.size();
  }
 }
 for(unsigned points:{2u,3u,7u,29u,31u,71u,72u,73u,96u,128u}){
  const auto plan=PlanResourceFont(points);const unsigned ow=plan.entryPoints==8?8:48,oh=plan.entryPoints==8?11:64;
  const unsigned w=static_cast<unsigned>(ow*double(plan.points)/plan.entryPoints+0.5),h=static_cast<unsigned>(oh*double(plan.points)/plan.entryPoints+0.5);
  std::vector<unsigned char> source(ow*oh*3);
  for(unsigned y=1;y+1<oh;++y)for(unsigned x=1;x+1<ow;++x)if(x==3||y==oh-3||(x+y)%7==0)source[(y*ow+x)*3]=255;
  const auto actual=ResizeResourceRGBCell(source,ow,oh,w,h);
  if(actual!=OriginalWxHigh(source,ow,oh,w,h))throw std::runtime_error("Original font-size source RGB mismatch");
  ++cases;bytes+=actual.size();
 }
 std::cout<<"PASS "<<cases<<" synthetic RGB cases / "<<bytes<<" exact bytes against unchanged fixed wx3.2.6 method bodies with minimal storage shim; not wx GUI/device execution\n";
}
