#include "system_font_catalog.h"
#include <native_drawing/drawing_error_code.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace olex2::harmony {
namespace {
using StyleSet=std::unique_ptr<OH_Drawing_FontStyleSet,decltype(&OH_Drawing_FontMgrDestroyFontStyleSet)>;
using Name=std::unique_ptr<char,decltype(&OH_Drawing_FontMgrDestroyFamilyName)>;
struct StyleName {char* value=nullptr;~StyleName(){if(value)OH_Drawing_FontStyleSetFreeStyleName(&value);}};
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string BoundedName(const char* text,size_t limit,bool empty=false){
  Require(text||empty,"Missing native font family name");
  const size_t count=text?strnlen(text,limit+1):0;Require(count<=limit,"Oversized native font name");
  std::string result=text?std::string(text,count):std::string();ValidateFontName(result,limit,empty);return result;
}
std::string Quoted(const std::string& value){
  std::ostringstream out;out<<'"';
  for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;else out<<c;}
  out<<'"';return out.str();
}
SystemFontStyle ReadStyle(OH_Drawing_FontStyleSet* set,int index){
  StyleName name;const auto native=OH_Drawing_FontStyleSetGetStyle(set,index,&name.value);
  SystemFontStyle style{index,static_cast<int>(native.weight),static_cast<int>(native.width),static_cast<int>(native.slant),BoundedName(name.value,256,true)};
  FamilyFontDescription check;check.family="bounded-style-contract";check.styleName=style.name;check.weight=style.weight;check.width=style.width;check.slant=style.slant;
  ValidateFamilyFontDescription(check);return style;
}
bool SameStyle(const SystemFontStyle& style,const FamilyFontDescription& spec){return style.name==spec.styleName&&style.weight==spec.weight&&style.width==spec.width&&style.slant==spec.slant;}
}
SystemFontCatalog::SystemFontCatalog(){
  manager_.reset(OH_Drawing_FontMgrCreate());Require(manager_!=nullptr,"Native system font manager unavailable");
  const int count=OH_Drawing_FontMgrGetFamilyCount(manager_.get());Require(count>=0&&count<=4096,"Native font family count outside bound");
  size_t total=0;families_.reserve(static_cast<size_t>(count));
  for(int i=0;i<count;++i){
    Name name(OH_Drawing_FontMgrGetFamilyName(manager_.get(),i),OH_Drawing_FontMgrDestroyFamilyName);
    SystemFontFamily family;family.index=i;family.name=BoundedName(name.get(),512);
    StyleSet set(OH_Drawing_FontMgrCreateFontStyleSet(manager_.get(),i),OH_Drawing_FontMgrDestroyFontStyleSet);
    Require(set!=nullptr,"Native family style set unavailable");const int styles=OH_Drawing_FontStyleSetCount(set.get());
    Require(styles>=0&&styles<=1024,"Native style count outside bound");total+=static_cast<size_t>(styles);Require(total<=16384,"Total native style count outside bound");
    for(int j=0;j<styles;++j)family.styles.push_back(ReadStyle(set.get(),j));
    families_.push_back(std::move(family));
  }
}
SelectedSystemTypeface SystemFontCatalog::SelectExact(const FamilyFontDescription& spec)const{
  ValidateFamilyFontDescription(spec);const SystemFontFamily* family=nullptr;const SystemFontStyle* style=nullptr;
  for(const auto& item:families_)if(item.name==spec.family){Require(!family,"Ambiguous exact system font family");family=&item;}
  Require(family!=nullptr,"Selected system font family is not installed; no fallback");
  for(const auto& item:family->styles)if(SameStyle(item,spec)){Require(!style,"Ambiguous exact system font style");style=&item;}
  Require(style!=nullptr,"Selected exact system font style is not installed; no closest-style fallback");
  Name name(OH_Drawing_FontMgrGetFamilyName(manager_.get(),family->index),OH_Drawing_FontMgrDestroyFamilyName);
  Require(BoundedName(name.get(),512)==family->name,"Native font family changed after enumeration");
  StyleSet set(OH_Drawing_FontMgrCreateFontStyleSet(manager_.get(),family->index),OH_Drawing_FontMgrDestroyFontStyleSet);
  Require(set!=nullptr&&OH_Drawing_FontStyleSetCount(set.get())==static_cast<int>(family->styles.size()),"Native style catalog changed after enumeration");
  const auto actual=ReadStyle(set.get(),style->index);Require(SameStyle(actual,spec),"Native selected style changed after enumeration");
  SelectedSystemTypeface result;result.family=*family;result.style=actual;
  result.typeface.reset(OH_Drawing_FontStyleSetCreateTypeface(set.get(),style->index));
  Require(result.typeface!=nullptr,"Native exact style typeface could not be created");
  Require(OH_Drawing_TypefaceIsBold(result.typeface.get(),&result.actualBold)==OH_DRAWING_SUCCESS&&
          OH_Drawing_TypefaceIsItalic(result.typeface.get(),&result.actualItalic)==OH_DRAWING_SUCCESS,"Native actual face style metadata unavailable");
  return result;
}
std::string SystemFontCatalog::Json()const{
  std::ostringstream out;out<<"{\"source\":\"OH_Drawing_FontMgr\",\"closest_match_used\":false,\"weight_codes_are_SDK_raw_values\":true,\"families\":[";
  for(size_t i=0;i<families_.size();++i){const auto& family=families_[i];if(i)out<<',';out<<"{\"index\":"<<family.index<<",\"name\":"<<Quoted(family.name)<<",\"styles\":[";
    for(size_t j=0;j<family.styles.size();++j){const auto& style=family.styles[j];if(j)out<<',';out<<"{\"index\":"<<style.index<<",\"name\":"<<Quoted(style.name)<<",\"weight_code\":"<<style.weight<<",\"width_code\":"<<style.width<<",\"slant_code\":"<<style.slant<<'}';}out<<"]}";}
  out<<"]}";return out.str();
}
}
