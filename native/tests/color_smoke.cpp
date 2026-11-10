// Synthetic color values/pixels only; no real model or complete material renderer.
#include "upstream_color.h"
#include "gloption.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void Check(bool ok,const char* name) { if(!ok) throw std::runtime_error(name); ++checks; std::cout<<"PASS "<<name<<'\n'; }
bool Near(float a,float b) { return std::abs(a-b)<.00001f; }
}
int main() {
  using namespace olex2::harmony;
  try {
    TGlOption original;
    Check(original.IsBlack() && original[3]==1,"upstream default opaque black");
    Check(original.FromString("0.2,0.8,0.59,1") && Near(original[0],.2f) && Near(original[2],.59f),"original four-component float parser");
    Check(original.GetRGB()==(51u|(204u<<8)|(150u<<16)|(255u<<24)),"original RGBA low-byte-red packing");
    const auto packed=original.GetRGB();
    TGlOption roundtrip; Check(roundtrip.FromString(original.ToString()) && roundtrip.GetRGB()==packed,"original decimal serialization round trip");
    Check(roundtrip.FromString("4278190335") && roundtrip[0]==1 && roundtrip[1]==0 && roundtrip[2]==0 && roundtrip[3]==1,"original decimal parser exact channel order");
    Check(!roundtrip.FromString("1,2,3"),"original parser rejects wrong component count");
    TGlOption arithmetic(.2f,.3f,.4f,1.f); arithmetic+=TGlOption(.1f,.1f,.1f,0.f);
    Check(Near(arithmetic[0],.3f) && Near(arithmetic.GetMean(),.4f),"original arithmetic and mean unchanged");
    TGlOption extended(2.f,4.f,1.f,2.f); extended.Scale();
    Check(extended[0]==.5f && extended[1]==1 && extended[2]==.25f && extended[3]==.5f,"original Scale semantics unchanged");
    TGlOption grey(.3f,.3f,.3f,1.f); Check(grey.IsGrey() && !grey.IsBlack(),"original grey predicate unchanged");
    OpaqueColor color; Check(Near(color.Rgba()[1],.8f) && color.Packed()==packed,"adapter default uses original parser");
    for(unsigned preset=0;preset<3;++preset) {
      Check(color.SetPreset(preset),"valid opaque preset accepted");
      TGlOption source; Check(source.FromString(olxstr(color.Canonical().c_str())) && source.GetRGB()==color.Packed(),"adapter canonical value matches original parser");
      const auto rgba=color.Rgba(); const auto before=color.Canonical();
      const std::vector<std::string> invalid={"", "garbage", "-1", "4294967296", "99999999999999999999999999", "0xFFFFFFFF", "1,2,3", "0.2,0.8,0.59,1,0", "0.2,,0.59,1", "nan,0,0,1", "inf,0,0,1", "1e999,0,0,1", "1.1,0,0,1", "-0.1,0,0,1", "0,1,0,0.5", "0,1,0,0", "0,1,0,1junk", "0,1,0,1 ", " 4294967295", "4294967295 ", "1e3", "++1,0,0,1", "0,0,0,", std::string(129,'1'),std::string("0,1,0,1\0junk",12)};
      for(const auto& value:invalid) Check(!color.Parse(value) && color.Rgba()==rgba && color.Canonical()==before,"invalid value rejected without changing active color");
      Check(!color.SetPreset(3) && color.Rgba()==rgba,"unknown preset preserves state");
      for(float light:{.3f,.5f,1.f}) {
        unsigned char pixel[4]={0,0,0,255};
        for(unsigned i=0;i<3;++i) pixel[i]=static_cast<unsigned char>(std::lround(rgba[i]*light*255));
        Check(MatchesLitColor(pixel,rgba),"expected lit opaque pixel matched");
        for(unsigned other=0;other<3;++other) if(other!=preset) {
          OpaqueColor wrong; wrong.SetPreset(other);
          Check(!MatchesLitColor(pixel,wrong.Rgba()),"wrong preset pixel rejected at each tested shade");
        }
      }
      const unsigned char background[]={14,20,26,255}, marker[]={0,255,0,255}, black[]={0,0,0,255}, transparent[]={51,204,150,0};
      Check(!MatchesLitColor(background,rgba),"background does not satisfy color diagnostic");
      Check(!MatchesLitColor(marker,rgba),"separate marker does not satisfy geometry diagnostic");
      Check(!MatchesLitColor(black,rgba),"black does not satisfy geometry diagnostic");
      Check(!MatchesLitColor(transparent,rgba) && !MatchesLitColor(nullptr,rgba),"invalid pixel input rejected");
    }
    Check(color.Parse("4278190335") && color.Rgba()[0]==1 && color.Rgba()[2]==0,"adapter accepts opaque packed red");
    Check(!color.Parse("0") && !color.Parse("16777215"),"adapter rejects transparent packed values");
    Check(color.Parse("0,0,0,1"),"adapter accepts valid opaque black value");
    const unsigned char black[]={0,0,0,255}; Check(!MatchesLitColor(black,color.Rgba()),"black cannot provide positive geometry color evidence");
    Check(color.Parse("1e-1,2E-1,0.3,1"),"normalized decimal exponent syntax supported");
    Check(std::string(OpaqueColor::Source()).find("gloption.cpp")!=std::string::npos,"source provenance exposed");
    std::cout<<"PASS upstream color value adapter only; "<<checks<<" assertions; not complete materials or renderer\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
