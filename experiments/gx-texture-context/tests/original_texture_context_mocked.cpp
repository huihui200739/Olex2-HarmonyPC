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
std::map<GLuint,Image> objects;GLuint nextId=100,bound=0,bound2D=0;
int unpack=4,pack=4;
std::map<GLenum,GLint> rowState={{GL_UNPACK_ROW_LENGTH,9},{GL_UNPACK_SKIP_ROWS,2},{GL_UNPACK_SKIP_PIXELS,3},
  {GL_PACK_ROW_LENGTH,7},{GL_PACK_SKIP_ROWS,3},{GL_PACK_SKIP_PIXELS,2}};
unsigned generated=0,deleted=0,uploads=0,reads=0,checks=0;bool enabled=false;bool contextCurrent=true;unsigned outOfContextDeletes=0,deleteCalls=0;
void Require(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}
void BackendRequire(bool b,const char* e){if(!b)throw std::runtime_error(e);}
}
extern "C" {
void glGetIntegerv(GLenum parameter,GLint* out) {
  if(parameter==GL_TEXTURE_BINDING_2D)*out=static_cast<GLint>(bound2D);
  else if(parameter==GL_UNPACK_ALIGNMENT)*out=unpack;else if(parameter==GL_PACK_ALIGNMENT)*out=pack;
  else {*out=rowState.at(parameter);}
}
void glPixelStorei(GLenum parameter,GLint value) {
  if(parameter==GL_UNPACK_ALIGNMENT)unpack=value;else if(parameter==GL_PACK_ALIGNMENT)pack=value;else rowState.at(parameter)=value;
}
void glGenTextures(GLsizei count,GLuint* ids) {for(int i=0;i<count;++i){ids[i]=nextId++;objects[ids[i]]=Image{};++generated;}}
void glBindTexture(GLenum type,GLuint id) {BackendRequire(type==GL_TEXTURE_1D||type==GL_TEXTURE_2D,"Unexpected texture type");bound=id;if(type==GL_TEXTURE_2D)bound2D=id;if(id)objects.try_emplace(id);}
void glDeleteTextures(GLsizei count,const GLuint* ids) {++deleteCalls;if(!contextCurrent){++outOfContextDeletes;return;}for(int i=0;i<count;++i){BackendRequire(objects.erase(ids[i])==1,"Unknown/double texture release");if(bound==ids[i])bound=0;if(bound2D==ids[i])bound2D=0;++deleted;}}
void glTexImage1D(GLenum target,GLint level,GLint internal,GLsizei w,GLint border,GLenum format,GLenum type,const GLvoid* pixels) {
  const GLsizei h=1;
  BackendRequire(target==GL_TEXTURE_1D&&level==0&&border==0&&bound&&type==GL_UNSIGNED_BYTE,"Unexpected upload interface");
  BackendRequire(w>0&&h>0&&w<=5&&h==1&&pixels&&(format==GL_RGB||format==GL_RGBA),"Unexpected bounded synthetic image");
  // Reject a wrong alignment BEFORE inspecting bytes; never replay the old
  // layout mismatch or ask a driver to read an undersized source buffer.
  BackendRequire(unpack==1&&rowState.at(GL_UNPACK_ROW_LENGTH)==0&&rowState.at(GL_UNPACK_SKIP_ROWS)==0&&rowState.at(GL_UNPACK_SKIP_PIXELS)==0,"Candidate did not select bounded packed upload layout");
  BackendRequire(internal==(format==GL_RGB?3:4),"Original component format changed");
  auto& image=objects.at(bound);image.width=w;image.height=h;image.rgba.clear();const auto* bytes=static_cast<const unsigned char*>(pixels);
  const int channels=format==GL_RGB?3:4;
  for(int i=0;i<w*h;++i)for(int c=0;c<4;++c)image.rgba.push_back(c<3||channels==4?bytes[i*channels+c]:255);
  ++uploads;
}
void glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const GLvoid* pixels) {
  BackendRequire(target==GL_TEXTURE_2D&&level==0&&border==0&&bound&&type==GL_UNSIGNED_BYTE,"Unexpected upload interface");
  BackendRequire(w>0&&h>0&&w<=5&&h<=4&&pixels&&(format==GL_RGB||format==GL_RGBA),"Unexpected bounded synthetic image");
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
  BackendRequire((target==GL_TEXTURE_1D||target==GL_TEXTURE_2D)&&level==0&&bound,"Unexpected texture-level query");const auto& image=objects.at(bound);
  if(parameter==GL_TEXTURE_WIDTH)*value=image.width;else if(parameter==GL_TEXTURE_HEIGHT)*value=image.height;
  else if(parameter==GL_TEXTURE_BORDER)*value=0;else throw std::runtime_error("Unexpected texture-level parameter");
}
void glGetTexImage(GLenum target,GLint level,GLenum format,GLenum type,GLvoid* pixels) {
  BackendRequire((target==GL_TEXTURE_1D||target==GL_TEXTURE_2D)&&level==0&&format==GL_RGBA&&type==GL_UNSIGNED_BYTE&&pixels&&pack==1&&rowState.at(GL_PACK_ROW_LENGTH)==0&&rowState.at(GL_PACK_SKIP_ROWS)==0&&rowState.at(GL_PACK_SKIP_PIXELS)==0,"Unexpected readback contract");
  const auto& bytes=objects.at(bound).rgba;std::memcpy(pixels,bytes.data(),bytes.size());++reads;
}
void glEnable(GLenum target){BackendRequire(target==GL_TEXTURE_1D,"Unexpected enable");enabled=true;}
void glDisable(GLenum target){BackendRequire(target==GL_TEXTURE_1D,"Unexpected disable");enabled=false;}
void glTexParameteri(GLenum target,GLenum name,GLint value){BackendRequire(target==GL_TEXTURE_1D&&bound,"Unexpected sampler update");objects.at(bound).parameters[name]=value;}
}
int main() {
  try {
    for(int alignment:{1,2,4,8})for(int width:{1,3,5})for(GLenum format:{GL_RGB,GL_RGBA}) {
      const auto savedRows=rowState;unpack=pack=alignment;const unsigned freed=deleted;
      GLuint current=0;
      {
        TTextureManager manager;const unsigned channels=format==GL_RGB?3:4;
        std::vector<unsigned char> pixels(width*channels);
        for(size_t i=0;i<pixels.size();++i)pixels[i]=static_cast<unsigned char>((i*17+31)%256);
        current=manager.Add1DTexture("synthetic-1d",0,width,0,format,pixels.data());
        auto* texture=manager.FindByName("synthetic-1d");
        Require(texture&&texture->GetId()==current&&texture->GetType()==GL_TEXTURE_1D,"1D manager identity/target");
        Require(manager.FindTexture(current)==texture,"Generated-ID lookup");
        Require(unpack==alignment&&pack==alignment&&rowState==savedRows,"Packed upload restores all caller transfer state");
        Require(objects.at(current).width==width&&objects.at(current).height==1,"1D width/height");
        for(int x=0;x<width;++x)for(unsigned c=0;c<4;++c)Require(objects.at(current).rgba[x*4+c]==(c<channels?pixels[x*channels+c]:255),"1D exact sample bytes");
        const auto expected=objects.at(current).rgba;
        texture->SetMinFilter(tpFilterNearest);texture->SetMagFilter(tpFilterNearest);texture->SetEnabled(true);texture->SetCurrent();
        auto data=texture->ReadData();Require(bound2D==0,"Pinned-backend read restores previous 2D binding");Require(data.width==width&&data.height==1,"Original read dimensions");
        Require(std::equal(expected.begin(),expected.end(),&data.data[0]),"Original read exact RGBA");
        texture->WriteData(data);Require(objects.at(current).rgba==expected,"Target-aware 1D rewrite");
        for(int cycle=0;cycle<3;++cycle) {
          manager.BeforeContextChange();Require(texture->GetId()==~GLuint(0)&&objects.empty(),"Resources released before recreation");
          manager.AfterContextChange();const GLuint next=texture->GetId();
          Require(next!=current&&manager.FindTexture(next)==texture&&manager.FindByName("synthetic-1d")==texture,"Recreated IDs rekey manager without replacing texture");
          Require(objects.at(next).rgba==expected,"Recreated original 1D sample bytes");current=next;
          Require(unpack==alignment&&pack==alignment&&rowState==savedRows,"Read/write recreation restores all transfer state");
        }
        const auto allocated=generated;bool rejected=false;
        try{manager.Add1DTexture("not-supported",0,width,0,GL_ALPHA,pixels.data());}catch(const TExceptionBase&){rejected=true;}
        Require(rejected&&generated==allocated&&rowState==savedRows&&unpack==alignment,"Unsupported format rejected before GL changes");
      }
      Require(objects.empty()&&deleted==freed+4,"All four owned generations released once");
    }
    {
      TTextureManager mixed;std::vector<unsigned char> one(3*4,91),two(2*2*4,137);
      mixed.Add1DTexture("mixed-1d",0,3,0,GL_RGBA,one.data());mixed.Add2DTexture("mixed-2d",0,2,2,0,GL_RGBA,two.data());
      auto* a=mixed.FindByName("mixed-1d");auto* b=mixed.FindByName("mixed-2d");
      for(int cycle=0;cycle<3;++cycle){
        const auto prior2D=bound2D;
        auto readOne=a->ReadData();Require(bound2D==prior2D,"1D alias preserves nonzero mixed 2D binding");
        mixed.BeforeContextChange();Require(objects.empty(),"Mixed resources released before recreation");
        mixed.AfterContextChange();
        Require(mixed.FindTexture(a->GetId())==a&&mixed.FindTexture(b->GetId())==b,"Mixed regenerated-ID lookups");
        Require(objects.at(a->GetId()).rgba==one&&objects.at(b->GetId()).rgba==two,"Mixed 1D/2D target-aware restored samples");
      }
    }
    Require(objects.empty(),"Mixed resources released at destruction");
    Require(generated==deleted,"All owned objects balanced");
    const unsigned legacyChecks=checks;
    Require(legacyChecks==830,"All prior 830 interface assertions preserved");
    for(bool mixed:{false,true}){
      auto* manager=new TTextureManager;
      const unsigned char one[]={255,0,0,127,0,255,0,0,0,0,255,255};
      manager->Add1DTexture("detached-1d",0,3,0,GL_RGBA,one);
      if(mixed)manager->Add2DTexture("detached-2d",0,3,1,0,GL_RGBA,one);
      const auto* same=manager->FindByName("detached-1d");
      manager->BeforeContextChange();
      Require(same->GetId()==~GLuint(0),"Original manager marks retired resource detached");
      Require(objects.empty(),"All GPU names retired before context close");
      const auto calls=deleteCalls,retired=deleted;
      contextCurrent=false;
      delete manager;
      Require(deleteCalls==calls,"Detached manager destruction must not call GL at all");
      Require(outOfContextDeletes==0,"No GL dispatch after compatibility context closes");
      Require(deleted==retired,"Detached names must not be retired a second time");
      contextCurrent=true;
    }
    {
      auto* empty=new TTextureManager;const auto calls=deleteCalls;
      contextCurrent=false;delete empty;contextCurrent=true;
      Require(deleteCalls==calls,"Empty detached destruction needs no context");
    }
    Require(objects.empty()&&generated==deleted,"All old and detached resources balanced");

    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"uploads\":"<<uploads<<",\"readbacks\":"<<reads<<",\"real_gpu_executed\":false,\"backend\":\"host-only GL call capture\"}\n";
    return 0;
  }catch(const TExceptionBase& e){std::cerr<<e.GetException()->GetFullMessage().ToMBStr().c_str()<<'\n';return 1;}
  catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
