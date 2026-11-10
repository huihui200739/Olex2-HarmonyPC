#include "olex_font_resource.h"
#include <zlib.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace olex2::harmony;
using Bytes=std::vector<unsigned char>;
namespace {
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("Isolated synthetic font-resource assertion failed");}
template<class F>void Reject(F fn){bool rejected=false;try{fn();}catch(const std::runtime_error&){rejected=true;}Check(rejected);}
void U16(Bytes& b,unsigned n){b.push_back(static_cast<unsigned char>(n));b.push_back(static_cast<unsigned char>(n>>8));}
void U32(Bytes& b,uint32_t n){U16(b,n&65535);U16(b,n>>16);}
void Put(Bytes& b,const std::string& s){b.insert(b.end(),s.begin(),s.end());}
Bytes Payload(){Bytes b;U16(b,5);U16(b,7);for(unsigned i=0;i<5*7*32;++i)b.push_back(static_cast<unsigned char>(i*73+19));return b;}
Bytes Deflate(const Bytes& b){
 z_stream s{};Check(deflateInit2(&s,9,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)==Z_OK);
 Bytes result(compressBound(b.size()));s.next_in=const_cast<Bytef*>(b.data());s.avail_in=static_cast<uInt>(b.size());s.next_out=result.data();s.avail_out=static_cast<uInt>(result.size());
 Check(deflate(&s,Z_FINISH)==Z_STREAM_END);result.resize(s.total_out);Check(deflateEnd(&s)==Z_OK);return result;
}
Bytes Archive(const Bytes& payload,bool compressed,bool streamed,bool signature,bool absoluteOffsets){
 const auto data=compressed?Deflate(payload):payload;const uint32_t crc=static_cast<uint32_t>(crc32(crc32(0L,Z_NULL,0),payload.data(),static_cast<uInt>(payload.size())));
 const unsigned method=compressed?8:0,flags=streamed?8:0;const std::string name="nr__18";
 Bytes b={'o','f','n','t'};U32(b,0x04034b50);U16(b,20);U16(b,flags);U16(b,method);U16(b,0);U16(b,0);
 U32(b,streamed?0:crc);U32(b,streamed?0:static_cast<uint32_t>(data.size()));U32(b,streamed?0:static_cast<uint32_t>(payload.size()));U16(b,name.size());U16(b,0);Put(b,name);b.insert(b.end(),data.begin(),data.end());
 if(streamed){if(signature)U32(b,0x08074b50);U32(b,crc);U32(b,data.size());U32(b,payload.size());}
 const size_t central=b.size();U32(b,0x02014b50);U16(b,20);U16(b,20);U16(b,flags);U16(b,method);U16(b,0);U16(b,0);U32(b,crc);U32(b,data.size());U32(b,payload.size());
 U16(b,name.size());U16(b,0);U16(b,0);U16(b,0);U16(b,0);U32(b,0);U32(b,absoluteOffsets?4:0);Put(b,name);
 const auto centralSize=b.size()-central;U32(b,0x06054b50);U16(b,0);U16(b,0);U16(b,1);U16(b,1);U32(b,centralSize);U32(b,central-(absoluteOffsets?0:4));U16(b,0);return b;
}
}
int main(int argc,char** argv){
 if(argc==3){const auto font=LoadOlexFontResource(argv[1],argv[2]);std::cout<<"{\"width\":"<<font.width<<",\"height\":"<<font.height<<",\"bit_bytes\":"<<font.bits.size()<<",\"crc\":"<<font.crc<<",\"compression\":"<<font.compression<<"}\n";return 0;}
 const auto payload=Payload();
 for(bool compressed:{false,true})for(bool streamed:{false,true})for(bool signature:{false,true})for(bool absoluteOffsets:{false,true}){
   const auto archive=Archive(payload,compressed,streamed,signature,absoluteOffsets);const auto decoded=DecodeOlexFontResource(archive,"nr__18");
   Check(decoded.width==5&&decoded.height==7&&decoded.compression==(compressed?8:0));Check(decoded.entry=="nr__18");
   Check(decoded.bits==Bytes(payload.begin()+4,payload.end()));
   Reject([&]{DecodeOlexFontResource(archive,"nr__27");});
 }
 for(const auto& prefix:{std::string("nr__"),std::string("nrb_"),std::string("ni__"),std::string("nib_"),std::string("fr__"),std::string("frb_"),std::string("fi__"),std::string("fib_")})for(unsigned size=1;size<=128;++size){
   const auto d=ParseOlexResourceFontDescription("#中文路径/普通 字体.fnt:"+prefix+std::to_string(size));
   Check(d.style==prefix&&d.points==size&&d.Entry(size)==prefix+std::to_string(size));
   Check(d.fixed==(prefix[0]=='f')&&d.italic==(prefix[1]=='i')&&d.bold==(prefix[2]=='b'));
 }
 for(const std::string id:{"","@18","#font.fnt:nr__0","#font.fnt:nr__129","#font.fnt:nr__-1","#font.fnt:nr__+1","#font.fnt:nx__18","#font.fnt:nr__","#font.fnt:nr__18:extra","#/absolute:nr__18","#../parent.fnt:nr__18","#a/../b.fnt:nr__18","#a//b.fnt:nr__18","#a/:nr__18"})Reject([&]{ParseOlexResourceFontDescription(id);});
 auto archive=Archive(payload,false,false,false,false);
 auto bad=archive;bad[0]='x';Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 bad=archive;bad.resize(bad.size()-1);Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 bad=archive;bad[4+30+6+9]^=1;Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 bad=archive;bad[4+6]=1;Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 auto wrongPayload=payload;wrongPayload.pop_back();bad=Archive(wrongPayload,true,true,true,false);Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 wrongPayload=payload;wrongPayload[0]=0;wrongPayload[1]=0;bad=Archive(wrongPayload,false,false,false,false);Reject([&]{DecodeOlexFontResource(bad,"nr__18");});
 std::cout<<"PASS "<<checks<<" isolated resource-format assertions; no original GL, NativeDrawing or target execution\n";
}
