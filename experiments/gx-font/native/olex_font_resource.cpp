#include "olex_font_resource.h"
#include <zlib.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
namespace olex2::harmony {
namespace {
constexpr size_t MaxArchive=64u*1024u*1024u,MaxEntry=16u*1024u*1024u;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Range(size_t start,size_t length,size_t end){Require(start<=end&&length<=end-start,"Truncated font archive field");}
uint16_t U16(const std::vector<unsigned char>& b,size_t p){Range(p,2,b.size());return static_cast<uint16_t>(b[p]|(unsigned(b[p+1])<<8));}
uint32_t U32(const std::vector<unsigned char>& b,size_t p){Range(p,4,b.size());return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
std::string Text(const std::vector<unsigned char>& b,size_t p,size_t n){Range(p,n,b.size());return {reinterpret_cast<const char*>(b.data()+p),n};}
void RelativeResourceName(const std::string& name){
  Require(!name.empty()&&name.size()<=512&&name.front()!='/',"Invalid relative font resource filename");
  for(unsigned char c:name)Require(c>=32&&c!=127&&c!='\\'&&c!=':',"Invalid font resource filename character");
  size_t first=0;
  while(first<name.size()){
    const size_t next=name.find('/',first),end=next==std::string::npos?name.size():next;
    const auto part=name.substr(first,end-first);
    Require(!part.empty()&&part!="."&&part!="..","Invalid relative font resource path segment");
    if(next==std::string::npos)return;
    first=next+1;
  }
  throw std::runtime_error("Font resource path names a directory");
}
struct ZipEntry {std::string name;uint16_t flags=0,method=0;uint32_t crc=0,packed=0,unpacked=0;size_t start=0,data=0,end=0;};
std::vector<unsigned char> Unpack(const std::vector<unsigned char>& archive,const ZipEntry& e){
  Require(e.unpacked>=4&&e.unpacked<=MaxEntry,"Font entry size is outside bounded dimensions");
  std::vector<unsigned char> out(e.unpacked);
  if(e.method==0){Require(e.packed==e.unpacked,"Stored font entry lengths differ");std::copy_n(archive.data()+e.data,out.size(),out.data());}
  else {
    z_stream stream{};
    Require(inflateInit2(&stream,-MAX_WBITS)==Z_OK,"Could not initialize original ZIP deflate reader");
    stream.next_in=const_cast<Bytef*>(archive.data()+e.data);stream.avail_in=e.packed;
    stream.next_out=out.data();stream.avail_out=e.unpacked;
    const int rc=inflate(&stream,Z_FINISH);
    const bool complete=rc==Z_STREAM_END&&stream.total_in==e.packed&&stream.total_out==e.unpacked;
    const int closed=inflateEnd(&stream);
    Require(complete&&closed==Z_OK,"Incomplete or inconsistent font deflate stream");
  }
  const auto crc=crc32(crc32(0L,Z_NULL,0),out.data(),static_cast<uInt>(out.size()));
  Require(crc==e.crc,"Original font entry CRC mismatch");
  return out;
}
}
std::string OlexResourceFontDescription::Entry(unsigned pointSize)const{
  Require(pointSize>=1&&pointSize<=256,"Resource font point size is outside bounded range");return style+std::to_string(pointSize);
}
OlexResourceFontDescription ParseOlexResourceFontDescription(const std::string& text){
  Require(text.size()>=8&&text.size()<=540&&text.front()=='#',"Expected original #file:style-size font description");
  const size_t colon=text.find(':',1);Require(colon!=std::string::npos&&colon+5<text.size(),"Incomplete original font resource description");
  OlexResourceFontDescription result;result.file=text.substr(1,colon-1);RelativeResourceName(result.file);
  result.style=text.substr(colon+1,4);
  Require((result.style[0]=='f'||result.style[0]=='n')&&(result.style[1]=='r'||result.style[1]=='i')&&
          (result.style[2]=='_'||result.style[2]=='b')&&result.style[3]=='_',"Unknown original font resource style");
  result.fixed=result.style[0]=='f';result.italic=result.style[1]=='i';result.bold=result.style[2]=='b';
  for(size_t i=colon+5;i<text.size();++i){Require(text[i]>='0'&&text[i]<='9'&&result.points<=128,"Invalid bounded font resource point size");result.points=result.points*10+unsigned(text[i]-'0');}
  Require(result.points>=1&&result.points<=128,"Resource font point size must be in [1,128]");return result;
}
OlexResourceFontData DecodeOlexFontResource(const std::vector<unsigned char>& b,const std::string& entry){
  Require(b.size()>=26&&b.size()<=MaxArchive&&Text(b,0,4)=="ofnt","Invalid or oversized original ofnt resource");
  Require(!entry.empty()&&entry.size()<=1024,"Invalid font ZIP entry name");
  const size_t minimum=std::max(size_t(4),b.size()>65557?b.size()-65557:size_t(4));
  size_t end=std::numeric_limits<size_t>::max();
  for(size_t p=b.size()-22;;--p){
    if(U32(b,p)==0x06054b50u&&size_t(U16(b,p+20))==b.size()-p-22){end=p;break;}
    if(p==minimum)break;
  }
  Require(end!=std::numeric_limits<size_t>::max(),"Font ZIP end record is absent");
  const auto count=U16(b,end+10);const size_t directorySize=U32(b,end+12),directoryOffset=U32(b,end+16);
  Require(U16(b,end+4)==0&&U16(b,end+6)==0&&U16(b,end+8)==count&&count>0&&count<=4096,
          "Multipart, empty or ZIP64 font resources are not supported");
  Require(directorySize<=end&&directorySize<=8u*1024u*1024u,"Invalid font central directory size");
  const size_t directory=end-directorySize;
  Require(directory>=4&&directoryOffset<=directory,"Invalid font central directory offset");
  const size_t prefix=directory-directoryOffset;
  // wx streams may count the pre-existing ofnt bytes in ZIP offsets, or start
  // offsets at their own stream origin. Accept only those two exact layouts.
  Require(prefix==0||prefix==4,"Unexpected font ZIP offset origin");
  size_t p=directory;std::set<std::string> names;std::vector<std::pair<size_t,size_t>> ranges;
  ZipEntry selected;bool found=false;
  for(unsigned i=0;i<count;++i){
    Range(p,46,end);Require(U32(b,p)==0x02014b50u,"Invalid font central entry");
    ZipEntry e;e.flags=U16(b,p+8);e.method=U16(b,p+10);e.crc=U32(b,p+16);e.packed=U32(b,p+20);e.unpacked=U32(b,p+24);
    const size_t nameLength=U16(b,p+28),extraLength=U16(b,p+30),commentLength=U16(b,p+32),local=U32(b,p+42);
    Require(U16(b,p+6)<=20&&U16(b,p+34)==0&&(e.flags&~uint16_t(0x080e))==0&&(e.method==0||e.method==8),
            "Unsupported font ZIP version, flags or compression");
    Require(e.packed!=0xffffffffu&&e.unpacked!=0xffffffffu&&local!=0xffffffffu&&nameLength>0&&nameLength<=1024,
            "Unsupported ZIP64 or invalid font entry name");
    Range(p+46,nameLength+extraLength+commentLength,end);e.name=Text(b,p+46,nameLength);
    Require(e.name.find('\0')==std::string::npos&&names.insert(e.name).second,"Duplicate or invalid font ZIP entry");
    e.start=local+prefix;Range(e.start,30,directory);
    Require(e.start>=4&&U32(b,e.start)==0x04034b50u&&U16(b,e.start+4)<=20&&U16(b,e.start+6)==e.flags&&U16(b,e.start+8)==e.method,
            "Local/central font ZIP metadata differs");
    const size_t localName=U16(b,e.start+26),localExtra=U16(b,e.start+28);
    Range(e.start+30,localName+localExtra,directory);
    Require(Text(b,e.start+30,localName)==e.name,"Local/central font entry names differ");
    e.data=e.start+30+localName+localExtra;Range(e.data,e.packed,directory);e.end=e.data+e.packed;
    const uint32_t localCRC=U32(b,e.start+14),localPacked=U32(b,e.start+18),localUnpacked=U32(b,e.start+22);
    if(e.flags&8){
      Require((localCRC==0||localCRC==e.crc)&&(localPacked==0||localPacked==e.packed)&&(localUnpacked==0||localUnpacked==e.unpacked),
              "Inconsistent streamed font ZIP header");
      Range(e.end,12,directory);
      const bool unsignedDescriptor=U32(b,e.end)==e.crc&&U32(b,e.end+4)==e.packed&&U32(b,e.end+8)==e.unpacked;
      if(!unsignedDescriptor){
        Range(e.end,16,directory);
        Require(U32(b,e.end)==0x08074b50u&&U32(b,e.end+4)==e.crc&&U32(b,e.end+8)==e.packed&&U32(b,e.end+12)==e.unpacked,"Font ZIP data descriptor differs");
        e.end+=4;
      }
      e.end+=12;
    } else Require(localCRC==e.crc&&localPacked==e.packed&&localUnpacked==e.unpacked,"Local/central font sizes or CRC differ");
    ranges.emplace_back(e.start,e.end);
    if(e.name==entry){selected=e;found=true;}
    p+=46+nameLength+extraLength+commentLength;
  }
  Require(p==end,"Font central directory byte count differs");
  std::sort(ranges.begin(),ranges.end());size_t previous=4;
  for(const auto& r:ranges){Require(r.first>=previous&&r.second>=r.first&&r.second<=directory,"Overlapping font archive entries");previous=r.second;}
  Require(found,"Requested original font style/size is absent; no default-font substitution");
  const auto bytes=Unpack(b,selected);const uint16_t width=U16(bytes,0),height=U16(bytes,2);
  Require(width>0&&height>0&&width<=2048&&height<=2048,"Invalid original font cell dimensions");
  const size_t required=size_t(width)*height*32;
  Require(required<=MaxEntry-4&&bytes.size()==required+4,"Original256 glyph bit-array length differs");
  OlexResourceFontData result;result.width=width;result.height=height;result.compression=selected.method;result.crc=selected.crc;result.entry=entry;
  result.bits.assign(bytes.begin()+4,bytes.end());return result;
}
OlexResourceFontData LoadOlexFontResource(const std::string& path,const std::string& entry){
  Require(!path.empty()&&path.find('\0')==std::string::npos,"Invalid font resource file path");
  std::ifstream file(path,std::ios::binary|std::ios::ate);Require(bool(file),"Original font resource could not be opened");
  const auto size=file.tellg();Require(size>=0&&static_cast<uint64_t>(size)<=MaxArchive,"Font resource file exceeds bounded size");
  std::vector<unsigned char> bytes(static_cast<size_t>(size));file.seekg(0);
  if(!bytes.empty())file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  Require(bool(file),"Original font resource read was incomplete");return DecodeOlexFontResource(bytes,entry);
}
}
