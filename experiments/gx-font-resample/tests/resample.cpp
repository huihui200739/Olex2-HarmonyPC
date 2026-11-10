#include "resource_font_resample.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned checks=0;
void Check(bool b){++checks;if(!b)throw std::runtime_error("isolated synthetic resource resampling assertion "+std::to_string(checks));}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}Check(rejected);}
}
int main(){
 for(unsigned n=1;n<=128;++n){const auto p=PlanResourceFont(n);const unsigned normalized=n==1?10:n;
  Check(p.points==normalized);Check(p.entryPoints==(normalized<8?8u:normalized>28?72u:normalized));Check(p.useRGB==(normalized<8||normalized>28));}
 Reject([]{PlanResourceFont(0);});Reject([]{PlanResourceFont(129);});
 std::vector<unsigned char> cell(4*4*3);
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){cell[(y*4+x)*3]=static_cast<unsigned char>(y*40+x*10);cell[(y*4+x)*3+1]=200;}
 Check(ResizeResourceRGBCell(cell,4,4,4,4)==cell);
 auto box=ResizeResourceRGBCell(cell,4,4,2,2);Check(box==std::vector<unsigned char>({25,200,0,45,200,0,105,200,0,125,200,0}));
 Check(ResizeResourceRGBCell(cell,4,4,1,1)==std::vector<unsigned char>({75,200,0}));
 for(unsigned w=1;w<=17;++w)for(unsigned h=1;h<=17;++h){
  auto data=ResizeResourceRGBCell(std::vector<unsigned char>(3*3*3,128),3,3,w,h);
  Check(data.size()==w*h*3);Check(std::all_of(data.begin(),data.end(),[](unsigned char v){return v==128;}));}
 Reject([&]{ResizeResourceRGBCell(cell,4,4,0,2);});Reject([&]{ResizeResourceRGBCell(cell,4,4,2049,2);});
 Reject([&]{ResizeResourceRGBCell({},4,4,2,2);});
 for(unsigned original:{8u,72u}){
  OlexResourceFontData font;font.width=8;font.height=8;font.bits.resize(8*8*32);
  for(unsigned c=1;c<256;++c)for(unsigned y=1;y<7;++y){size_t bit=(c*8+y)*8+3;font.bits[bit/8]|=static_cast<unsigned char>(1u<<(bit%8));}
  const auto p=PlanResourceFont(original==8?7:96);auto scaled=ResampleResourceFont(font,p);
  const unsigned dim=original==8?7:11;Check(scaled.width==dim&&scaled.height==dim);
  Check(std::all_of(scaled.rgb[0].begin(),scaled.rgb[0].end(),[](unsigned char b){return b==0;}));
  for(unsigned c=1;c<256;++c){Check(scaled.rgb[c]==scaled.rgb[1]);Check(std::any_of(scaled.rgb[c].begin(),scaled.rgb[c].end(),[](unsigned char b){return b!=0;}));}
 }
 std::cout<<"PASS "<<checks<<" isolated synthetic resource selection/RGB resampling assertions; no GL or device execution\n";
}
