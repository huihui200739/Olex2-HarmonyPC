#include "font_description.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace olex2::harmony;
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("Synthetic font description assertion failed");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}Check(rejected);}
int main(){
  for(unsigned points=1;points<=128;++points) {
    for(const auto& style:std::vector<std::string>{"r","b","i","bi"})for(bool fixed:{false,true}) {
      const auto spec=ParseFontDescription("ohos-native:"+std::to_string(points)+":"+style+(fixed?":fixed":""));
      Check(spec.points==points);Check(spec.kind==PlatformFontKind::NativeRaster);Check(spec.fixed==fixed);
      Check(spec.bold==(style.find('b')!=std::string::npos));Check(spec.italic==(style.find('i')!=std::string::npos));
      Check(EffectiveFontPoints(spec,false,1)==points);
    }
    const auto vector=ParseFontDescription("@"+std::to_string(points));Check(vector.kind==PlatformFontKind::OriginalHershey);
    for(bool half:{false,true})for(double scale:{.125,.5,1.,1.5,8.})Check(EffectiveFontPoints(vector,half,scale)==points);
  }
  Check(ParseFontDescription("@").points==15);
  for(const auto& value:std::vector<std::string>{"","@0","@129","@-1","@+18","@18:fixed","@bad","18","wx:18","#resource:f___18","ohos-native:0:r","ohos-native:129:r","ohos-native::r","ohos-native:18:","ohos-native:18:rb","ohos-native:18:r:fixed:fixed",std::string(81,'@')})Reject([&]{ParseFontDescription(value);});
  const auto native=ParseFontDescription("ohos-native:18:r");Check(EffectiveFontPoints(native,false,1.5)==27);Check(EffectiveFontPoints(native,true,1)==13);
  for(const auto& spec:{native,ParseFontDescription("@18")})for(double scale:{0.,-1.,8.1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})Reject([&]{EffectiveFontPoints(spec,false,scale);});
  Reject([&]{EffectiveFontPoints(native,false,.001);});Reject([&]{EffectiveFontPoints(ParseFontDescription("ohos-native:128:r"),false,8);});
  std::cout<<"PASS "<<checks<<" isolated descriptor/size assertions; no NativeDrawing, GL or target execution\n";
}
