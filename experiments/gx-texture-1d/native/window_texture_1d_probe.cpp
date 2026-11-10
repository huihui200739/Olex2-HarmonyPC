#include "window_texture_1d_probe.h"
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
std::string RunOriginalTexture1DWindowProbe(const Olex2LegacyHost& host,int width,int height){
  CallerStores caller;std::ostringstream out;
  TTextureManager anchors;const unsigned char sentinel[]={29,37,41,255};
  const GLuint sentinelId=anchors.Add2DTexture("1d-readback-binding-sentinel",0,1,1,0,GL_RGBA,sentinel);
  Check("Create 2D readback binding sentinel");
  out<<"{\"schema\":1,\"route\":\"original-manager-current-window\",\"offscreen\":false,\"cases\":[";
  unsigned index=0;
  for(unsigned texWidth:{3u,5u})for(unsigned channels:{3u,4u}) {
    if(index++)out<<',';
    const std::string name="texture-1d-w"+std::to_string(texWidth)+(channels==3?"-rgb":"-rgba");
    const StoreState wanted={8,13,2,3,8,11,3,2};
    for(unsigned i=0;i<wanted.size();++i)glPixelStorei(stores[i],wanted[i]);Check("Select nondefault caller transfer state");
    const auto pixels=Pixels(texWidth,channels);const auto expected=Pixels(texWidth,4);
    std::vector<unsigned char> rgba=expected;
    if(channels==3)for(size_t a=3;a<rgba.size();a+=4)rgba[a]=255;
    GLuint lastId=0;out<<"{\"name\":\""<<name<<"\",\"texture_width\":"<<texWidth<<",\"channels\":"<<channels<<",\"caller_before\":";Array(out,wanted);out<<",\"generations\":[";
    {
      TTextureManager manager;
      lastId=manager.Add1DTexture(name.c_str(),0,texWidth,0,channels==3?GL_RGB:GL_RGBA,pixels.data());Check("Original Add1DTexture");
      auto* texture=manager.FindByName(name.c_str());Require(texture&&manager.FindTexture(lastId)==texture&&texture->GetType()==GL_TEXTURE_1D,"Original 1D manager identity");
      texture->SetMinFilter(tpFilterNearest);texture->SetMagFilter(tpFilterNearest);texture->SetEnvMode(GL_REPLACE);texture->SetEnabled(true);
      const auto afterUpload=State();Require(afterUpload==wanted,"1D upload caller transfer state differs");
      for(unsigned generation=0;generation<2;++generation){
        const GLuint id=texture->GetId();
        auto read=texture->ReadData();Check("Original 1D texture CPU read");
        Require(read.width==int(texWidth)&&read.height==1,"Original 1D read dimensions");
        // Real framebuffer sampling below is an independent GPU proof; this
        // CPU readback alone must never be called actual GPU qualification.
        Require(std::equal(rgba.begin(),rgba.end(),&read.data[0]),"Original 1D read exact RGBA differs");
        GLint after2D=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&after2D);
        Require(after2D==static_cast<GLint>(sentinelId),"1D readback did not restore caller 2D binding");
        Draw(*texture,width,height);
        const std::string label=name+"-generation-"+std::to_string(generation);
        if(host.gpu_snapshot)host.gpu_snapshot(host.run,label.c_str());
        std::vector<unsigned char> frame(static_cast<size_t>(width)*height*4);
        {olex2_harmony::ScopedPackedPixels<olx_gl> pack(GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS);
          glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,frame.data());Check("Actual textured window framebuffer read");}
        Save(std::string(host.files_dir)+"/legacy-run-"+std::to_string(host.run)+"-"+label+".rgba",frame.data(),frame.size());
        if(generation)out<<',';
        out<<"{\"generation\":"<<generation<<",\"texture_id\":"<<id<<",\"previous_2d_binding\":"<<sentinelId<<",\"after_read_2d_binding\":"<<after2D<<",\"manager_lookup_matches\":"<<(manager.FindTexture(id)==texture?"true":"false")<<",\"read_width\":"<<read.width<<",\"read_height\":"<<read.height<<",\"caller_after\":";Array(out,State());out<<'}';
        if(generation==0){
          manager.BeforeContextChange();Check("Release texture GL generation");Require(texture->GetId()==~GLuint(0),"Original texture ID was not detached");
          manager.AfterContextChange();Check("Recreate original texture GL generation");
          Require(manager.FindTexture(texture->GetId())==texture,"Recreated-ID manager lookup differs");lastId=texture->GetId();
        }
      }
      Require(State()==wanted,"Texture draw/read/recreation changed caller transfer state");
    }
    Check("Original 1D manager destruction");
    Require(!glIsTexture(lastId),"Original 1D manager final GPU object still alive");Check("Observe texture object release");
    out<<"],\"last_texture_released\":true,\"caller_after_destruction\":";Array(out,State());out<<'}';
  }
  glDisable(GL_TEXTURE_1D);Check("End original 1D window route");
  out<<"],\"resource_recreation\":\"same-current-context-only\",\"physical_pc_verified\":false,\"full_port_complete\":false}";
  return out.str();
}
