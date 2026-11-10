#include "glyph_bitmap.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("Synthetic cell-packing assertion failed");}
std::array<RasterGlyph,256> Inputs() {
  std::array<RasterGlyph,256> result;
  for(auto& g:result){g.width=2;g.height=3;g.left=-1;g.top=-2;g.advance=2;g.rgb.assign(18,255);}
  return result;
}
template<class F> void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
}
int main() {
  auto input=Inputs();input[65].rgb[0]=0;input[65].rgb[1]=17;input[65].rgb[2]=33;
  input[66].left=1;input[66].top=-1;input[66].rgb[15]=21;input[66].rgb[16]=42;input[66].rgb[17]=63;
  const auto result=PackOriginalFontCells(input);Check(result.width==4 && result.height==4);Check(result.baselineX==1 && result.baselineY==2);
  for(size_t i=0;i<256;++i){Check(result.rgb[i].size()==48);if(i!=65 && i!=66)for(auto x:result.rgb[i])Check(x==255);}
  Check(result.rgb[65][0]==0 && result.rgb[65][1]==17 && result.rgb[65][2]==33);
  const size_t b=((3*4)+3)*3;Check(result.rgb[66][b]==21 && result.rgb[66][b+1]==42 && result.rgb[66][b+2]==63);
  Check(input[65].rgb[0]==0 && input[66].left==1); // Sources not modified.
  for(unsigned mutation=0;mutation<8;++mutation) {
    auto invalid=Inputs();auto& g=invalid[127];
    switch(mutation){case 0:g.width=0;break;case 1:g.rgb.pop_back();break;case 2:g.left=std::numeric_limits<int>::min();break;
      case 3:g.top=4097;break;case 4:g.advance=std::numeric_limits<float>::quiet_NaN();break;
      case 5:g.advance=-1;break;case 6:g.height=4097;break;case 7:g.width=std::numeric_limits<uint32_t>::max();break;}
    Reject([&]{PackOriginalFontCells(invalid);});
  }
  auto huge=Inputs();huge[0].left=-4096;huge[1].left=4096;Reject([&]{PackOriginalFontCells(huge);});
  auto alloc=Inputs();for(auto& g:alloc){g.width=512;g.height=512;g.left=0;g.top=0;g.rgb.assign(512*512*3,255);}Reject([&]{PackOriginalFontCells(alloc);});
  std::cout<<"PASS "<<checks<<" synthetic cell geometry/channel/bounds assertions; no native font or GL execution\n";
}
