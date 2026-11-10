// Explicitly isolated descriptor/UTF8/math tests; no NativeDrawing, fonts or GPU.
#include "family_font_description.h"
#include "font_description.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned count=0;
void Check(bool value){++count;if(!value)throw std::runtime_error("Isolated font-family descriptor assertion failed");}
template<class Function> void Reject(Function fn){bool rejected=false;try{fn();}catch(const std::invalid_argument&){rejected=true;}Check(rejected);}
}
int main(){
  const std::string families[]={"Synthetic Sans","Synthetic \"quoted\" : \\ family",u8"合成测试字体",u8"𝄞 Family"};
  for(const auto& family:families)for(const auto& style:{std::string(),std::string("Regular"),std::string(u8"粗斜体")})
    for(unsigned points:{1u,12u,18u,128u})for(int weight:{0,3,8,400,700,1000})for(int slant:{0,1,2})for(bool fixed:{false,true}){
      FamilyFontDescription spec{family,style,points,weight,5,slant,fixed};const auto id=FamilyFontId(spec);const auto parsed=ParseFamilyFontId(id);
      Check(parsed.family==family&&parsed.styleName==style&&parsed.points==points&&parsed.weight==weight&&parsed.width==5&&parsed.slant==slant&&parsed.fixed==fixed);
      Check(FamilyFontId(parsed)==id&&IsFamilyFontId(id));
      FontDescription base;base.points=points;base.fixed=fixed;
      for(double scale:{0.75,1.0,1.5})for(bool half:{false,true}){
        const auto expected=static_cast<unsigned>(std::floor(points*scale*(half?0.75:1.0)));
        if(expected)Check(EffectiveFontPoints(base,half,scale)==expected);else Reject([&]{EffectiveFontPoints(base,half,scale);});
      }
    }
  for(const std::string& bad:{std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4),std::string("\xe2\x82",2),std::string("\x80",1),std::string("a\0b",3),std::string("a\nb")})Reject([&]{ValidateFontName(bad,512);});
  for(const auto& id:{"ohos-family:v1:0:3:5:0:41:","ohos-family:v1:129:3:5:0:41:","ohos-family:v1:18:1001:5:0:41:",
    "ohos-family:v1:18:3:0:0:41:","ohos-family:v1:18:3:10:0:41:","ohos-family:v1:18:3:5:3:41:",
    "ohos-family:v1:018:3:5:0:41:","ohos-family:v1:18:3:5:0:4A:","ohos-family:v1:18:3:5:0:4:",
    "ohos-family:v1:18:3:5:0::", "ohos-family:v1:18:3:5:0:00:","ohos-family:v2:18:3:5:0:41:",
    "ohos-family:v1:18:3:5:0:41::fixed:ignored","ohos-family:v1:18:3:5:0:41::fallback","ohos-native:18:r"})Reject([&]{ParseFamilyFontId(id);});
  Reject([&]{ParseFamilyFontId(std::string(1651,'x'));});
  FamilyFontDescription tooLong;tooLong.family=std::string(513,'x');Reject([&]{FamilyFontId(tooLong);});
  tooLong.family="bounded";tooLong.styleName=std::string(257,'x');Reject([&]{FamilyFontId(tooLong);});
  Check(ParseFontDescription("ohos-native:18:bi:fixed").points==18);
  Check(ParseFontDescription("@18").kind==PlatformFontKind::OriginalHershey);
  std::cout<<"PASS "<<count<<" isolated synthetic family descriptor assertions; no NativeDrawing or device execution\n";
}
