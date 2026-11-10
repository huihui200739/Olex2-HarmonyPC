// Ordinary, valid synthetic images only. Not security testing or a fuzzer.
#include "png_texture_image.h"
#include <png.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace olex2::harmony;
namespace {
unsigned checks=0,decoded=0,hiddenRgbImages=0,interlacedImages=0;
void Check(bool ok,const char* text) {++checks;if(!ok)throw std::runtime_error(text);}
struct Fixture {
  std::string name;
  unsigned width=8,height=8;
  int depth=8,type=PNG_COLOR_TYPE_RGB,interlace=PNG_INTERLACE_NONE;
  bool transparency=false,gamma=false;
  std::vector<unsigned char> encodedRows,expected;
};
Fixture Make(int type,int depth,bool transparency,int interlace,unsigned width=8,unsigned height=8,bool gamma=false) {
  Fixture f;f.type=type;f.depth=depth;f.interlace=interlace;f.transparency=transparency;f.width=width;f.height=height;f.gamma=gamma;
  f.name="png-"+std::to_string(type)+"-"+std::to_string(depth)+"-"+std::to_string(transparency)+"-"+std::to_string(interlace)+"-"+std::to_string(width)+"x"+std::to_string(height)+(gamma?"-gamma":"");
  const unsigned channels=type==PNG_COLOR_TYPE_RGB?3:(type==PNG_COLOR_TYPE_RGBA?4:(type==PNG_COLOR_TYPE_GRAY_ALPHA?2:1));
  const size_t stride=(size_t(width)*channels*depth+7)/8;f.encodedRows.assign(stride*height,0);
  const std::array<std::array<unsigned char,3>,4> colors={{{31,207,93},{241,17,139},{63,125,251},{180,41,7}}};
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
    const unsigned index=y*width+x;
    unsigned values[4]={0,0,0,255};
    if(type==PNG_COLOR_TYPE_PALETTE) {
      values[0]=index%(depth==1?2:4);
      const auto& color=colors[values[0]];f.expected.insert(f.expected.end(),color.begin(),color.end());
    }else if(type==PNG_COLOR_TYPE_GRAY||type==PNG_COLOR_TYPE_GRAY_ALPHA) {
      const unsigned max=depth==16?65535:((1u<<depth)-1);
      values[0]=depth<8?index%(max+1):(depth==16?((index*991+117)&65535):((index*37+29)&255));
      if(type==PNG_COLOR_TYPE_GRAY_ALPHA)values[1]=depth==16?(index%4==0?0:((index*1237)&65535)):(index%4==0?0:((index*41)&255));
      const unsigned char gray=depth==16?values[0]>>8:values[0]*255/max;
      f.expected.insert(f.expected.end(),{gray,gray,gray});
    }else {
      const unsigned max=depth==16?65535:255;
      for(unsigned c=0;c<3;++c)values[c]=(index*(depth==16?1129:37)+y*13+c*(depth==16?17717:59)+31)&max;
      if(type==PNG_COLOR_TYPE_RGBA)values[3]=index%4==0?0:(depth==16?((index*1357)&65535):((index*53)&255));
      for(unsigned c=0;c<3;++c)f.expected.push_back(depth==16?values[c]>>8:values[c]);
    }
    if(depth<8) {
      const size_t bit=size_t(x)*depth;f.encodedRows[y*stride+bit/8]|=static_cast<unsigned char>(values[0]<<(8-depth-bit%8));
    }else {
      for(unsigned c=0;c<channels;++c) {
        const size_t pos=y*stride+(size_t(x)*channels+c)*(depth/8);
        f.encodedRows[pos]=depth==16?values[c]>>8:values[c];
        if(depth==16)f.encodedRows[pos+1]=values[c]&255;
      }
    }
  }
  return f;
}
void Write(const fs::path& filename,const Fixture& f) {
  FILE* file=fopen(filename.c_str(),"wb");if(!file)throw std::runtime_error("Cannot create ordinary PNG fixture");
  png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
  png_infop info=png_create_info_struct(png);
  std::vector<png_bytep> rows(f.height);
  const size_t stride=f.encodedRows.size()/f.height;
  for(unsigned y=0;y<f.height;++y)rows[y]=const_cast<png_bytep>(f.encodedRows.data()+y*stride);
  png_color palette[4]={{31,207,93},{241,17,139},{63,125,251},{180,41,7}};
  png_byte alpha[4]={0,1,127,255};png_color_16 transparent{};
  if(setjmp(png_jmpbuf(png)))std::abort(); // no test seeks an error/longjmp
  png_init_io(png,file);
  png_set_IHDR(png,info,f.width,f.height,f.depth,f.type,f.interlace,PNG_COMPRESSION_TYPE_BASE,PNG_FILTER_TYPE_BASE);
  if(f.type==PNG_COLOR_TYPE_PALETTE) {
    const int count=f.depth==1?2:4;png_set_PLTE(png,info,palette,count);
    if(f.transparency)png_set_tRNS(png,info,alpha,count,nullptr);
  }else if(f.transparency) {
    transparent.gray=f.depth<8?0:(f.depth==16?117:29);transparent.red=31;transparent.green=90;transparent.blue=149;
    png_set_tRNS(png,info,nullptr,0,&transparent);
  }
  if(f.gamma)png_set_gAMA(png,info,0.7);
  png_write_info(png,info);png_write_image(png,rows.data());png_write_end(png,info);
  png_destroy_write_struct(&png,&info);fclose(file);
}
void Verify(const fs::path& path,const Fixture& fixture,std::ofstream& manifest,bool unicode=false) {
  Write(path,fixture);const auto result=DecodePngTextureImage(path.string());
  if(!result.image.Decoded())std::cerr<<path<<": "<<result.image.error<<'\n';
  Check(result.image.Decoded(),"Valid supported PNG was not decoded");
  Check(result.image.rgb==fixture.expected,"Original PNG RGB samples changed");
  Check(result.image.width==fixture.width&&result.image.height==fixture.height,"PNG dimensions differ");
  Check(result.image.rowStride==fixture.width*3&&result.image.readBytes==fixture.expected.size(),"PNG row layout differs");
  Check(result.sourceBitDepth==fixture.depth&&result.sourceColorType==fixture.type&&result.sourceInterlace==fixture.interlace,"Source metadata differs");
  Check(result.DecoderAllocationsBalanced(),"libpng allocation callback scope did not balance");
  Check(result.warnings==0,"Valid fixture emitted a PNG warning");
  const auto& audit=result.image.resourceAudit;
  Check(audit.OwnedScopeBalanced(),"Owned PNG input resource scope did not balance");
  Check(audit.file.opened&&audit.file.identityKnown&&audit.file.closeAttempted&&audit.file.closeCode==0,"Own file did not close");
  Check(audit.inputKind==std::string("owned-memory-buffer")&&audit.encodedBytes>0&&!audit.sdkReceivedFileDescriptor&&audit.bufferReleasedAfterHandles,"Encoded input lifetime differs");
  Check(audit.handleCount==0,"CPU PNG decoder was misrepresented as ImageKit execution");
  ++decoded;if(fixture.transparency||fixture.type==PNG_COLOR_TYPE_RGBA||fixture.type==PNG_COLOR_TYPE_GRAY_ALPHA)++hiddenRgbImages;
  if(fixture.interlace==PNG_INTERLACE_ADAM7)++interlacedImages;
  if(decoded>1)manifest<<',';
  manifest<<"{\"name\":\""<<path.filename().string()<<"\",\"decoded\":true,\"source_bit_depth\":"<<result.sourceBitDepth
    <<",\"source_color_type\":"<<result.sourceColorType<<",\"source_interlace\":"<<result.sourceInterlace<<",\"unicode_path\":"<<(unicode?"true":"false")
    <<",\"rgb_bytes\":"<<result.image.rgb.size()<<",\"allocations\":"<<result.allocations<<",\"releases\":"<<result.releases
    <<",\"live_allocations\":"<<result.liveAllocations<<",\"live_bytes\":"<<result.liveBytes<<",\"input_resources\":"<<ImageResourceAuditJson(audit)<<'}';
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("An exclusive fixture directory is required");
    const fs::path root=argv[1];fs::create_directories(root);
    std::ofstream manifest(root/"png-observations.json");manifest<<"{\"decoder_version\":\""<<olex2_png_decoder_version()<<"\",\"scope\":\"ordinary synthetic PNG CPU decoding only; no GPU or application qualification\",\"images\":[";
    for(int interlace:{PNG_INTERLACE_NONE,PNG_INTERLACE_ADAM7}) {
      for(int depth:{1,2,4,8,16})for(bool trns:{false,true}) {
        auto f=Make(PNG_COLOR_TYPE_GRAY,depth,trns,interlace);Verify(root/(f.name+".png"),f,manifest);
      }
      for(int depth:{1,2,4,8})for(bool trns:{false,true}) {
        auto f=Make(PNG_COLOR_TYPE_PALETTE,depth,trns,interlace);Verify(root/(f.name+".png"),f,manifest);
      }
      for(int depth:{8,16}) {
        for(int type:{PNG_COLOR_TYPE_RGB,PNG_COLOR_TYPE_RGBA,PNG_COLOR_TYPE_GRAY_ALPHA}) {
          auto f=Make(type,depth,false,interlace);Verify(root/(f.name+".png"),f,manifest);
        }
        auto f=Make(PNG_COLOR_TYPE_RGB,depth,true,interlace);Verify(root/(f.name+".png"),f,manifest);
      }
    }
    for(unsigned size:{1,2,4}) {
      auto f=Make(PNG_COLOR_TYPE_RGBA,8,false,PNG_INTERLACE_ADAM7,size,size);Verify(root/(f.name+".png"),f,manifest);
    }
    auto f=Make(PNG_COLOR_TYPE_RGBA,8,false,PNG_INTERLACE_NONE,2,2,true);
    Verify(root/"合成透明颜色.png",f,manifest,true);
    for(auto dimensions:{std::array<unsigned,2>{3,3},std::array<unsigned,2>{2,4}}) {
      auto ordinary=Make(PNG_COLOR_TYPE_RGB,8,false,PNG_INTERLACE_NONE,dimensions[0],dimensions[1]);const auto path=root/(ordinary.name+".png");Write(path,ordinary);
      const auto unsupported=DecodePngTextureImage(path.string());
      Check(unsupported.image.status==TextureImageStatus::UnsupportedDimensions,"Original dimension restriction was lost");
      Check(unsupported.image.rgb.empty(),"Unsupported dimensions returned pixels");
      Check(unsupported.DecoderAllocationsBalanced()&&unsupported.image.resourceAudit.OwnedScopeBalanced(),"Unsupported dimension resources did not balance");
    }
    Check(DecodePngTextureImage("").image.status==TextureImageStatus::FileError,"Empty path status differs");
    Check(DecodePngTextureImage((root/"no-file-selected.png").string()).image.status==TextureImageStatus::FileError,"Missing selection status differs");
    manifest<<"],\"passed\":true,\"checks\":"<<checks<<",\"decoded_images\":"<<decoded<<",\"hidden_rgb_images\":"<<hiddenRgbImages<<",\"adam7_images\":"<<interlacedImages<<",\"real_gpu_execution\":false,\"full_port_complete\":false}\n";
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"decoded_images\":"<<decoded<<",\"hidden_rgb_images\":"<<hiddenRgbImages<<",\"adam7_images\":"<<interlacedImages<<",\"decoder_version\":\""<<olex2_png_decoder_version()<<"\",\"real_gpu_execution\":false,\"full_port_complete\":false}\n";
    return 0;
  }catch(const std::exception& error) {std::cerr<<"PNG test failed: "<<error.what()<<'\n';return 1;}
}
