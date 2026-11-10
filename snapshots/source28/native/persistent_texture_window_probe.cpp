#include "persistent_texture_window_probe.h"
#include "original_renderer_host.h"
#include "gltexture.h"
#include "glpixelstorescope.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
constexpr GLenum stores[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,
  GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_PIXELS};
using StoreState=std::array<GLint,8>;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Check(const char* message){const GLenum e=glGetError();if(e)throw std::runtime_error(std::string(message)+" GL "+std::to_string(e));}
StoreState State(){StoreState a{};for(unsigned i=0;i<a.size();++i)glGetIntegerv(stores[i],&a[i]);Check("Read transfer state");return a;}
void Array(std::ostringstream& out,const StoreState& a){out<<'[';for(unsigned i=0;i<a.size();++i){if(i)out<<',';out<<a[i];}out<<']';}
class CallerStores {
  StoreState saved_=State();
public:
  ~CallerStores(){for(unsigned i=0;i<saved_.size();++i)glPixelStorei(stores[i],saved_[i]);}
};
std::vector<unsigned char> Pixels(unsigned width,unsigned channels){
  constexpr unsigned char colors[][4]={{255,0,0,255},{0,255,0,191},{0,0,255,127},{255,255,0,63},{255,0,255,0}};
  std::vector<unsigned char> p;
  for(unsigned i=0;i<width;++i)for(unsigned c=0;c<channels;++c)p.push_back(colors[i][c]);
  return p;
}
void Draw(TGlTexture& texture,int width,int height){
  glViewport(0,0,width,height);glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_TEXTURE_2D);
  glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
  texture.SetCurrent();glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor4f(1,1,1,1);
  glBegin(GL_QUADS);
  glTexCoord1f(0);glVertex3f(-1,-1,0);glTexCoord1f(1);glVertex3f(1,-1,0);
  glTexCoord1f(1);glVertex3f(1,1,0);glTexCoord1f(0);glVertex3f(-1,1,0);
  glEnd();glFinish();Check("Original 1D texture actual window draw");
}
void Save(const std::string& file,const unsigned char* data,size_t size){
  std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(data),size);out.close();Require(bool(out),"Retain 1D window pixels");
}
}
std::string RunPersistentTextureWindowProbe(const Olex2LegacyHost& host,TTextureManager& manager,int width,int height){
  CallerStores caller;
  std::ostringstream out;
  out<<"{\"schema\":1,\"route\":\"original-renderer-manager-across-real-contexts\",\"offscreen\":false,\"run\":"<<host.run
     <<",\"manager_address\":"<<reinterpret_cast<uintptr_t>(&manager)<<",\"cases\":[";
  unsigned index=0;
  for(unsigned texWidth:{3u,5u})for(unsigned channels:{3u,4u}) {
    if(index++)out<<',';
    const std::string name="persistent-texture-1d-w"+std::to_string(texWidth)+(channels==3?"-rgb":"-rgba");
    const StoreState wanted={8,13,2,3,8,11,3,2};
    for(unsigned i=0;i<wanted.size();++i)glPixelStorei(stores[i],wanted[i]);
    Check("Select persistent texture caller transfer state");
    auto* texture=manager.FindByName(name.c_str());
    const bool restored=texture!=nullptr;
    Require(restored==(host.run>1),"Persistent texture presence differs from actual window generation");
    const auto pixels=Pixels(texWidth,channels);
    auto expected=Pixels(texWidth,4);
    if(channels==3)for(size_t a=3;a<expected.size();a+=4)expected[a]=255;
    if(!texture){
      const GLuint id=manager.Add1DTexture(name.c_str(),0,texWidth,0,channels==3?GL_RGB:GL_RGBA,pixels.data());
      texture=manager.FindByName(name.c_str());
      Require(texture && texture->GetId()==id,"Persistent manager upload identity");
      texture->SetMinFilter(tpFilterNearest);texture->SetMagFilter(tpFilterNearest);
      texture->SetSCrdWrapping(GL_CLAMP_TO_EDGE);texture->SetEnvMode(GL_REPLACE);texture->SetEnabled(true);
    }
    Check("Persistent original texture upload/restore");
    Require(texture->GetId()!=~GLuint(0) && texture->GetType()==GL_TEXTURE_1D && manager.FindTexture(texture->GetId())==texture,"Persistent restored manager identity");
    const auto read=texture->ReadData();
    Require(read.width==int(texWidth)&&read.height==1,"Persistent original texture dimensions");
    Require(std::equal(expected.begin(),expected.end(),&read.data[0]),"Persistent original CPU restoration bytes");
    const auto afterRead=State();Require(afterRead==wanted,"Persistent texture transfer state changed");
    Draw(*texture,width,height);
    GLint minFilter=0,magFilter=0,wrapping=0;
    glGetTexParameteriv(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,&minFilter);
    glGetTexParameteriv(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,&magFilter);
    glGetTexParameteriv(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,&wrapping);
    Check("Persistent restored sampler readback");
    Require(minFilter==GL_NEAREST&&magFilter==GL_NEAREST&&wrapping==GL_CLAMP_TO_EDGE,"Persistent sampler state not restored");
    std::vector<unsigned char> framebuffer(static_cast<size_t>(width)*height*4);
    {
      olex2_harmony::ScopedPackedPixels<olx_gl> pack(GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS);
      glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,framebuffer.data());
    }
    Check("Persistent texture actual framebuffer read");
    const std::string stem=std::string(host.files_dir)+"/persistent-run-"+std::to_string(host.run)+"-"+name;
    Save(stem+".rgba",framebuffer.data(),framebuffer.size());
    Save(stem+"-cpu.rgba",&read.data[0],static_cast<size_t>(texWidth)*4);
    if(host.gpu_snapshot)host.gpu_snapshot(host.run,name.c_str());
    const auto afterDraw=State();Require(afterDraw==wanted,"Persistent framebuffer caller transfer state changed");
    out<<"{\"name\":\""<<name<<"\",\"texture_width\":"<<texWidth<<",\"channels\":"<<channels
       <<",\"texture_address\":"<<reinterpret_cast<uintptr_t>(texture)<<",\"resident_id\":"<<texture->GetId()
       <<",\"restored_from_prior_context\":"<<(restored?"true":"false")<<",\"min_filter\":"<<minFilter
       <<",\"mag_filter\":"<<magFilter<<",\"wrap_s\":"<<wrapping<<",\"caller_before\":";
    Array(out,wanted);out<<",\"caller_after\":";Array(out,afterDraw);out<<'}';
  }
  out<<"]}";return out.str();
}
bool PersistentWindowTexturesDetached(TTextureManager& manager){
  for(unsigned width:{3u,5u})for(unsigned channels:{3u,4u}){
    const std::string name="persistent-texture-1d-w"+std::to_string(width)+(channels==3?"-rgb":"-rgba");
    auto* texture=manager.FindByName(name.c_str());
    if(!texture || texture->GetId()!=~GLuint(0))return false;
  }
  return true;
}
