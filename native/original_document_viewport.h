#pragma once
// Private same-build viewport schema. Pure platform interchange, no application
// model, loader, macro, interpreter, GL or replacement selection implementation.
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace olex2::harmony {
constexpr size_t MaximumViewportJsonBytes=1024u*1024u;
constexpr size_t MaximumViewportObjects=1000000;
constexpr size_t MaximumViewportSelection=65536;
struct OriginalDocumentViewport {
  unsigned schemaVersion=1;
  std::string name,sha256,inventorySha256;
  size_t byteCount=0,objectCount=0;
  double yaw=0,pitch=0,distance=4.4;
  std::vector<size_t> selectedObjectIndices;
};
inline void ViewportRequire(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
inline bool ViewportHash(const std::string& text){
  if(text.size()!=64)return false;
  for(char c:text)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool ViewportUtf8(const std::string& text){
  for(size_t i=0;i<text.size();){
    unsigned char c=text[i++];if(c<0x80)continue;
    unsigned n=0,code=0,min=0;
    if(c>=0xc2&&c<=0xdf){n=1;code=c&31;min=0x80;}
    else if(c>=0xe0&&c<=0xef){n=2;code=c&15;min=0x800;}
    else if(c>=0xf0&&c<=0xf4){n=3;code=c&7;min=0x10000;}else return false;
    if(n>text.size()-i)return false;
    while(n--){c=text[i++];if((c&0xc0)!=0x80)return false;code=(code<<6)|(c&63);}
    if(code<min||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
  }
  return true;
}
inline void ValidateOriginalDocumentViewport(const OriginalDocumentViewport& s){
  ViewportRequire(s.schemaVersion==1,"Unsupported original viewport schemaVersion");
  ViewportRequire(!s.name.empty()&&s.name.size()<=240&&s.name!="."&&s.name!=".."&&
    s.name.find_first_of("/\\\r\n") == std::string::npos&&s.name.find('\0')==std::string::npos&&ViewportUtf8(s.name),"Invalid original viewport name");
  ViewportRequire(s.byteCount>0&&s.byteCount<=8u*1024u*1024u,"Invalid original viewport byteCount");
  ViewportRequire(ViewportHash(s.sha256)&&ViewportHash(s.inventorySha256),"Expected lowercase SHA256 viewport identities");
  ViewportRequire(std::isfinite(s.yaw)&&s.yaw>=-360&&s.yaw<=360&&
    std::isfinite(s.pitch)&&s.pitch>=-85&&s.pitch<=85&&
    std::isfinite(s.distance)&&s.distance>=2.2&&s.distance<=10,"Original viewport camera is outside representable limits");
  ViewportRequire(s.objectCount<=MaximumViewportObjects,"Original viewport objectCount exceeds bound");
  ViewportRequire(s.selectedObjectIndices.size()<=MaximumViewportSelection,"Original viewport selection exceeds bound");
  size_t previous=0;bool first=true;
  for(size_t i:s.selectedObjectIndices){
    ViewportRequire(i<s.objectCount&&(first||i>previous),"Original viewport indices must be ascending, unique and in range");
    first=false;previous=i;
  }
}
namespace viewport_detail {
class Parser {
  const std::string& text_;size_t p_=0;
  void Space(){while(p_<text_.size()&&(text_[p_]==' '||text_[p_]=='\t'||text_[p_]=='\r'||text_[p_]=='\n'))++p_;}
  char Take(){ViewportRequire(p_<text_.size(),"Truncated original viewport JSON");return text_[p_++];}
  bool Consume(char c){Space();if(p_<text_.size()&&text_[p_]==c){++p_;return true;}return false;}
  void Expect(char c){ViewportRequire(Consume(c),"Invalid original viewport JSON delimiter");}
  unsigned Hex(){unsigned n=0;for(unsigned i=0;i<4;++i){unsigned char c=Take();n<<=4;
    if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;
    else if(c>='A'&&c<='F')n+=c-'A'+10;else throw std::invalid_argument("Invalid viewport Unicode escape");}return n;}
  static void Utf8(std::string& out,unsigned c){
    if(c<0x80)out+=char(c);
    else if(c<0x800){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
    else if(c<0x10000){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
  }
  std::string String(){
    Expect('"');std::string out;
    for(;;){unsigned char c=Take();if(c=='"')break;ViewportRequire(c>=32,"Control byte in viewport JSON string");
      if(c!='\\'){out+=char(c);continue;}
      switch(Take()){
      case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
      case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
      case 'u':{unsigned n=Hex();if(n>=0xd800&&n<=0xdbff){ViewportRequire(Take()=='\\'&&Take()=='u',"Unpaired viewport Unicode surrogate");unsigned low=Hex();ViewportRequire(low>=0xdc00&&low<=0xdfff,"Invalid viewport Unicode surrogate");n=0x10000+((n-0xd800)<<10)+(low-0xdc00);}else ViewportRequire(n<0xdc00||n>0xdfff,"Unpaired viewport Unicode surrogate");Utf8(out,n);break;}
      default:throw std::invalid_argument("Invalid viewport JSON escape");
      }
    }
    ViewportRequire(ViewportUtf8(out),"Invalid viewport UTF-8");return out;
  }
  bool Digit()const{return p_<text_.size()&&text_[p_]>='0'&&text_[p_]<='9';}
  double Number(){
    Space();size_t start=p_;if(p_<text_.size()&&text_[p_]=='-')++p_;
    ViewportRequire(Digit(),"Expected viewport JSON number");
    if(text_[p_]=='0')++p_;else while(Digit())++p_;
    if(p_<text_.size()&&text_[p_]=='.'){++p_;ViewportRequire(Digit(),"Invalid viewport JSON fraction");while(Digit())++p_;}
    if(p_<text_.size()&&(text_[p_]=='e'||text_[p_]=='E')){++p_;if(p_<text_.size()&&(text_[p_]=='+'||text_[p_]=='-'))++p_;ViewportRequire(Digit(),"Invalid viewport JSON exponent");while(Digit())++p_;}
    std::istringstream in(text_.substr(start,p_-start));in.imbue(std::locale::classic());double value=0;in>>value;
    ViewportRequire(!in.fail()&&in.eof()&&std::isfinite(value),"Non-finite or unrepresentable viewport number");return value;
  }
  size_t Integer(size_t maximum){double value=Number();ViewportRequire(value>=0&&value<=double(maximum)&&value==std::floor(value),"Expected bounded viewport integer");return static_cast<size_t>(value);}
public:
  explicit Parser(const std::string& text):text_(text){}
  OriginalDocumentViewport Parse(){
    ViewportRequire(!text_.empty()&&text_.size()<=MaximumViewportJsonBytes,"Original viewport JSON must be 1..1048576 UTF-8 bytes");
    OriginalDocumentViewport out;std::set<std::string> fields;Expect('{');
    if(!Consume('}'))for(;;){
      const auto key=String();ViewportRequire(fields.insert(key).second,"Duplicate original viewport JSON field");Expect(':');
      if(key=="schemaVersion")out.schemaVersion=static_cast<unsigned>(Integer(1));
      else if(key=="name")out.name=String();else if(key=="sha256")out.sha256=String();else if(key=="inventorySha256")out.inventorySha256=String();
      else if(key=="byteCount")out.byteCount=Integer(8u*1024u*1024u);
      else if(key=="objectCount")out.objectCount=Integer(MaximumViewportObjects);
      else if(key=="yaw")out.yaw=Number();else if(key=="pitch")out.pitch=Number();else if(key=="distance")out.distance=Number();
      else if(key=="selectedObjectIndices"){
        Expect('[');if(!Consume(']'))for(;;){
          ViewportRequire(out.selectedObjectIndices.size()<MaximumViewportSelection,"Original viewport selection exceeds bound");
          out.selectedObjectIndices.push_back(Integer(MaximumViewportObjects));if(Consume(']'))break;Expect(',');
        }
      }else throw std::invalid_argument("Unknown original viewport JSON field");
      if(Consume('}'))break;Expect(',');
    }
    Space();ViewportRequire(p_==text_.size()&&fields.size()==10,"Original viewport JSON has missing fields or trailing input");
    ValidateOriginalDocumentViewport(out);return out;
  }
};
inline std::string Quote(const std::string& s){
  static const char hex[]="0123456789abcdef";std::string out="\"";
  for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
}
inline OriginalDocumentViewport ParseOriginalDocumentViewport(const std::string& json){return viewport_detail::Parser(json).Parse();}
inline std::string OriginalDocumentViewportJson(const OriginalDocumentViewport& s){
  ValidateOriginalDocumentViewport(s);std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<double>::max_digits10);
  out<<"{\"schemaVersion\":1,\"name\":"<<viewport_detail::Quote(s.name)<<",\"byteCount\":"<<s.byteCount<<",\"sha256\":"<<viewport_detail::Quote(s.sha256)
    <<",\"yaw\":"<<s.yaw<<",\"pitch\":"<<s.pitch<<",\"distance\":"<<s.distance<<",\"objectCount\":"<<s.objectCount
    <<",\"inventorySha256\":"<<viewport_detail::Quote(s.inventorySha256)<<",\"selectedObjectIndices\":[";
  for(size_t i=0;i<s.selectedObjectIndices.size();++i){if(i)out<<',';out<<s.selectedObjectIndices[i];}out<<"]}";
  const auto json=out.str();ViewportRequire(json.size()<=MaximumViewportJsonBytes,"Original viewport JSON exceeds bound");return json;
}
// Length-prefixed, ordered full inventory. Each independently stable identity
// must be unique: no ordinal fallback for indistinguishable same-type objects.
class ViewportInventoryEncoding {
  std::string bytes_="olex2-original-viewport-inventory-v1;";
  std::set<std::string> identities_;
  size_t count_=0;
public:
  void Add(const std::string& identity,const std::string& descriptor){
    ViewportRequire(count_<MaximumViewportObjects,"Original inventory objectCount exceeds bound");
    ViewportRequire(!identity.empty()&&identities_.insert(identity).second,"Original inventory contains ambiguous duplicate identities");
    const auto record=std::to_string(count_)+":"+std::to_string(identity.size())+":"+identity+std::to_string(descriptor.size())+":"+descriptor+";";
    ViewportRequire(record.size()<=64u*1024u*1024u&&bytes_.size()<=64u*1024u*1024u-record.size(),"Full original inventory exceeds 64 MiB; no truncated capture");
    bytes_+=record;++count_;
  }
  std::string Bytes()const{return bytes_+"count:"+std::to_string(count_)+";";}
  size_t Count()const{return count_;}
};
}
