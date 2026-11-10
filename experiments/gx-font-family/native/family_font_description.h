#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace olex2::harmony {
// The SDK's weight member is kept as its observed raw code, not guessed to be CSS weight.
struct FamilyFontDescription {
  std::string family, styleName;
  unsigned points=18;
  int weight=0,width=5,slant=0;
  bool fixed=false;
};
inline void ValidateFontName(const std::string& value,size_t limit,bool empty=false) {
  if(value.size()>limit || (!empty&&value.empty()))throw std::invalid_argument("Invalid bounded font name");
  for(size_t i=0;i<value.size();) {
    const auto lead=static_cast<unsigned char>(value[i]);unsigned scalar=0,count=0;
    if(lead<0x80){scalar=lead;count=1;}
    else if(lead>=0xc2&&lead<=0xdf){scalar=lead&31;count=2;}
    else if(lead>=0xe0&&lead<=0xef){scalar=lead&15;count=3;}
    else if(lead>=0xf0&&lead<=0xf4){scalar=lead&7;count=4;}
    else throw std::invalid_argument("Invalid UTF8 font name");
    if(i+count>value.size())throw std::invalid_argument("Truncated UTF8 font name");
    for(unsigned j=1;j<count;++j){const auto c=static_cast<unsigned char>(value[i+j]);if((c&0xc0)!=0x80)throw std::invalid_argument("Invalid UTF8 font continuation");scalar=(scalar<<6)|(c&63);}
    if((count==2&&scalar<0x80)||(count==3&&scalar<0x800)||(count==4&&scalar<0x10000)||
       scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff)||scalar<0x20||scalar==0x7f)
      throw std::invalid_argument("Invalid scalar or control in font name");
    i+=count;
  }
}
inline std::string FontNameHex(const std::string& value) {
  constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(value.size()*2);
  for(unsigned char c:value){result+=hex[c>>4];result+=hex[c&15];}return result;
}
inline std::string FontNameUnhex(const std::string& value) {
  if(value.size()%2)throw std::invalid_argument("Odd encoded font name");
  auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("Noncanonical encoded font name");};
  std::string result;result.reserve(value.size()/2);
  for(size_t i=0;i<value.size();i+=2)result+=static_cast<char>((digit(value[i])<<4)|digit(value[i+1]));
  return result;
}
inline void ValidateFamilyFontDescription(const FamilyFontDescription& spec) {
  ValidateFontName(spec.family,512);ValidateFontName(spec.styleName,256,true);
  if(!spec.points||spec.points>128||spec.weight<0||spec.weight>1000||spec.width<1||spec.width>9||spec.slant<0||spec.slant>2)
    throw std::invalid_argument("Invalid bounded family font style/size");
}
inline std::string FamilyFontId(const FamilyFontDescription& spec) {
  ValidateFamilyFontDescription(spec);
  return "ohos-family:v1:"+std::to_string(spec.points)+":"+std::to_string(spec.weight)+":"+
    std::to_string(spec.width)+":"+std::to_string(spec.slant)+":"+FontNameHex(spec.family)+":"+
    FontNameHex(spec.styleName)+(spec.fixed?":fixed":"");
}
inline bool IsFamilyFontId(const std::string& value){return value.rfind("ohos-family:",0)==0;}
inline FamilyFontDescription ParseFamilyFontId(const std::string& value) {
  if(value.size()>1650)throw std::invalid_argument("Oversized family font description");
  std::vector<std::string> parts;size_t first=0;
  for(size_t next;(next=value.find(':',first))!=std::string::npos;first=next+1)parts.push_back(value.substr(first,next-first));
  parts.push_back(value.substr(first));
  if((parts.size()!=8&&parts.size()!=9)||parts[0]!="ohos-family"||parts[1]!="v1"||
      (parts.size()==9&&parts[8]!="fixed"))throw std::invalid_argument("Invalid explicit family font format");
  auto number=[](const std::string& text)->unsigned{
    if(text.empty()||text.size()>4)throw std::invalid_argument("Invalid font number");unsigned result=0;
    for(char c:text){if(c<'0'||c>'9')throw std::invalid_argument("Invalid font number");result=result*10+static_cast<unsigned>(c-'0');}return result;
  };
  FamilyFontDescription spec;spec.points=number(parts[2]);spec.weight=static_cast<int>(number(parts[3]));
  spec.width=static_cast<int>(number(parts[4]));spec.slant=static_cast<int>(number(parts[5]));
  spec.family=FontNameUnhex(parts[6]);spec.styleName=FontNameUnhex(parts[7]);spec.fixed=parts.size()==9;
  ValidateFamilyFontDescription(spec);
  if(FamilyFontId(spec)!=value)throw std::invalid_argument("Noncanonical family font description");
  return spec;
}
}
