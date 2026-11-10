// Clearly isolated fake state, not a GPU, application or SDK runtime test.
#include "glpixelstorescope.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace {int value=4;unsigned checks=0;
struct Api {static void get(unsigned,int* out){*out=value;}static void pixelStore(unsigned,int v){value=v;}};
std::map<unsigned,int> states;
struct MultiApi {static void get(unsigned p,int* out){*out=states.at(p);}static void pixelStore(unsigned p,int v){states.at(p)=v;}};
void Require(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}}
int main() {
  try {
    for(int original:{1,2,4,8}) {
      value=original;
      {olex2_harmony::ScopedPixelStore<Api> scope(7,1);Require(value==1,"Packed transfer alignment");
        {olex2_harmony::ScopedPixelStore<Api> nested(7,2);Require(value==2,"Nested transfer alignment");}
        Require(value==1,"Nested scope restored its prior alignment");}
      Require(value==original,"Original alignment restored");
      try {olex2_harmony::ScopedPixelStore<Api> scope(7,1);throw std::runtime_error("synthetic caller exit");}catch(const std::runtime_error&){}
      Require(value==original,"Caller exception still restores alignment");
    }
    for(int original:{1,2,4,8}) {
      states={{1,original},{2,7},{3,2},{4,3}};const auto saved=states;
      {olex2_harmony::ScopedPackedPixels<MultiApi> scope(1,2,3,4);
        Require(states.at(1)==1&&states.at(2)==0&&states.at(3)==0&&states.at(4)==0,"Packed row length and skips cleared");}
      Require(states==saved,"All packed 2D transfer state restored");
      try {olex2_harmony::ScopedPackedPixels<MultiApi> scope(1,2,3,4);throw std::runtime_error("synthetic caller exit");}catch(const std::runtime_error&){}
      Require(states==saved,"All packed 2D state restored after caller exception");
    }
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"real_gpu_executed\":false}\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
