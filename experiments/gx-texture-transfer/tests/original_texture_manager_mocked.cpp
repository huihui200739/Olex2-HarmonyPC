// Real remediated upstream TTextureManager methods; test-owned GL call capture.
// These mocks NEVER link into a HAP and NEVER create a GL context or GPU image.
#include "gltexture.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
namespace {
struct Image {int width=0,height=0;std::vector<unsigned char> rgba;std::map<GLenum,GLint> parameters;};
std::map<GLuint,Image> objects;GLuint nextId=100,bound=0;
int unpack=4,pack=4;
std::map<GLenum,GLint> rowState={{GL_UNPACK_ROW_LENGTH,9},{GL_UNPACK_SKIP_ROWS,2},{GL_UNPACK_SKIP_PIXELS,3},
  {GL_PACK_ROW_LENGTH,7},{GL_PACK_SKIP_ROWS,3},{GL_PACK_SKIP_PIXELS,2}};
unsigned generated=0,deleted=0,uploads=0,reads=0,checks=0;bool enabled=false;
void Require(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}
void BackendRequire(bool b,const char* e){if(!b)throw std::runtime_error(e);}
}
extern "C" {
void glGetIntegerv(GLenum parameter,GLint* out) {
  if(parameter==GL_UNPACK_ALIGNMENT)*out=unpack;else if(parameter==GL_PACK_ALIGNMENT)*out=pack;
  else {*out=rowState.at(parameter);}
}
void glPixelStorei(GLenum parameter,GLint value) {
  if(parameter==GL_UNPACK_ALIGNMENT)unpack=value;else if(parameter==GL_PACK_ALIGNMENT)pack=value;else rowState.at(parameter)=value;
}
void glGenTextures(GLsizei count,GLuint* ids) {for(int i=0;i<count;++i){ids[i]=nextId++;objects[ids[i]]=Image{};++generated;}}
void glBindTexture(GLenum type,GLuint id) {BackendRequire(type==GL_TEXTURE_2D,"Unexpected texture type");bound=id;if(id)objects.try_emplace(id);}
void glDeleteTextures(GLsizei count,const GLuint* ids) {for(int i=0;i<count;++i){BackendRequire(objects.erase(ids[i])==1,"Unknown/double texture release");if(bound==ids[i])bound=0;++deleted;}}
void glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const GLvoid* pixels) {
  BackendRequire(target==GL_TEXTURE_2D&&level==0&&border==0&&bound&&type==GL_UNSIGNED_BYTE,"Unexpected upload interface");
  BackendRequire(w>0&&h>0&&w<=4&&h<=4&&pixels&&(format==GL_RGB||format==GL_RGBA),"Unexpected bounded synthetic image");
  // Reject a wrong alignment BEFORE inspecting bytes; never replay the old
  // layout mismatch or ask a driver to read an undersized source buffer.
  BackendRequire(unpack==1&&rowState.at(GL_UNPACK_ROW_LENGTH)==0&&rowState.at(GL_UNPACK_SKIP_ROWS)==0&&rowState.at(GL_UNPACK_SKIP_PIXELS)==0,"Candidate did not select bounded packed upload layout");
  BackendRequire(internal==(format==GL_RGB?3:4),"Original component format changed");
  auto& image=objects.at(bound);image.width=w;image.height=h;image.rgba.clear();const auto* bytes=static_cast<const unsigned char*>(pixels);
  const int channels=format==GL_RGB?3:4;
  for(int i=0;i<w*h;++i)for(int c=0;c<4;++c)image.rgba.push_back(c<3||channels==4?bytes[i*channels+c]:255);
  ++uploads;
}
void glGetTexLevelParameteriv(GLenum target,GLint level,GLenum parameter,GLint* value) {
  BackendRequire(target==GL_TEXTURE_2D&&level==0&&bound,"Unexpected texture-level query");const auto& image=objects.at(bound);
  if(parameter==GL_TEXTURE_WIDTH)*value=image.width;else if(parameter==GL_TEXTURE_HEIGHT)*value=image.height;
  else if(parameter==GL_TEXTURE_BORDER)*value=0;else throw std::runtime_error("Unexpected texture-level parameter");
}
void glGetTexImage(GLenum target,GLint level,GLenum format,GLenum type,GLvoid* pixels) {
  BackendRequire(target==GL_TEXTURE_2D&&level==0&&format==GL_RGBA&&type==GL_UNSIGNED_BYTE&&pixels&&pack==1&&rowState.at(GL_PACK_ROW_LENGTH)==0&&rowState.at(GL_PACK_SKIP_ROWS)==0&&rowState.at(GL_PACK_SKIP_PIXELS)==0,"Unexpected readback contract");
  const auto& bytes=objects.at(bound).rgba;std::memcpy(pixels,bytes.data(),bytes.size());++reads;
}
void glEnable(GLenum target){BackendRequire(target==GL_TEXTURE_2D,"Unexpected enable");enabled=true;}
void glDisable(GLenum target){BackendRequire(target==GL_TEXTURE_2D,"Unexpected disable");enabled=false;}
void glTexParameteri(GLenum target,GLenum name,GLint value){BackendRequire(target==GL_TEXTURE_2D&&bound,"Unexpected sampler update");objects.at(bound).parameters[name]=value;}
}
int main() {
  try {
    for(int saved:{1,2,4,8})for(int size:{1,2,4}) {
      const unsigned releasedBefore=deleted;GLuint id=0;const auto savedRowState=rowState;
      {
        TTextureManager manager;unpack=saved;pack=saved;
        std::vector<unsigned char> rgb(size*size*3);for(size_t i=0;i<rgb.size();++i)rgb[i]=static_cast<unsigned char>((i*17+31)%256);
        id=manager.Add2DTexture("synthetic-color",0,size,size,0,GL_RGB,rgb.data());
        auto* texture=manager.FindByName("synthetic-color");Require(texture&&texture->GetId()==id,"Original name/ID lookup");
        Require(unpack==saved&&pack==saved&&rowState==savedRowState,"Upload restores caller's pack/unpack alignment");
        Require(objects.at(id).width==size&&objects.at(id).height==size,"Original upload dimensions");
        for(size_t i=0;i<rgb.size()/3;++i)for(size_t c=0;c<3;++c)Require(objects.at(id).rgba[i*4+c]==rgb[i*3+c],"Packed RGB color/order preserved");
        texture->SetSCrdWrapping(tpCrdRepeat);texture->SetTCrdWrapping(tpCrdRepeat);texture->SetMinFilter(tpFilterNearest);texture->SetMagFilter(tpFilterLinear);texture->SetEnabled(true);texture->SetCurrent();
        const auto parameters=objects.at(id).parameters;Require(enabled&&parameters.at(GL_TEXTURE_MIN_FILTER)==GL_NEAREST&&parameters.at(GL_TEXTURE_MAG_FILTER)==GL_LINEAR,"Original sampler application");
        std::vector<unsigned char> rgba(size*size*4,137);for(size_t i=3;i<rgba.size();i+=4)rgba[i]=255;
        const unsigned genBefore=generated;
        manager.Replace2DTexture(*texture,0,size,size,0,GL_RGBA,rgba.data());
        Require(generated==genBefore&&deleted==releasedBefore&&texture->GetId()==id,"Replace keeps existing object ID without deleting it");
        Require(manager.FindByName("synthetic-color")==texture&&objects.at(id).parameters==parameters,"Replace preserves manager/name/sampler identity");
        Require(unpack==saved&&rowState==savedRowState&&objects.at(id).rgba==rgba,"Replace restores alignment and exact RGBA bytes");
        auto data=texture->ReadData();Require(pack==saved&&rowState==savedRowState&&data.width==size&&data.height==size,"Readback restores pack alignment");
        Require(std::equal(rgba.begin(),rgba.end(),&data.data[0]),"Actual upstream read buffer content");
        texture->WriteData(data);Require(unpack==saved&&pack==saved&&rowState==savedRowState&&objects.at(id).rgba==rgba,"Actual upstream write restores transfer alignment");
        bool rejected=false;try{manager.Add2DTexture("invalid-format",0,size,size,0,GL_ALPHA,rgb.data());}catch(const TExceptionBase&){rejected=true;}
        Require(rejected&&generated==genBefore&&unpack==saved,"Invalid add format rejected before allocation/state changes");
        rejected=false;try{manager.Replace2DTexture(*texture,0,size,size,0,GL_ALPHA,rgb.data());}catch(const TExceptionBase&){rejected=true;}
        Require(rejected&&deleted==releasedBefore&&objects.at(id).rgba==rgba&&unpack==saved,"Invalid replace leaves the valid texture unchanged");
      }
      Require(deleted==releasedBefore+1&&objects.count(id)==0,"Original manager releases its texture exactly once");
    }
    Require(objects.empty()&&generated==deleted,"All isolated test-owned objects released");
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"uploads\":"<<uploads<<",\"readbacks\":"<<reads<<",\"actual_original_manager_methods\":true,\"backend\":\"explicitly mocked GL only\",\"real_gpu_executed\":false}\n";return 0;
  }catch(const TExceptionBase& e){std::cerr<<e.GetException()->GetFullMessage().ToMBStr().c_str()<<'\n';return 1;}
  catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
