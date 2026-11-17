#include "original_vector_font_window_probe.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
// The unmodified array defined and consumed by upstream/glib/glfont.cpp.
extern int gl_font_simplex[95][112];
namespace olex2::harmony {
namespace {
void Check(const char* stage) {const auto e=glGetError();if(e)throw std::runtime_error(std::string(stage)+" GL="+std::to_string(e));}
void Save(const std::string& path,const std::vector<unsigned char>& bytes) {
  std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();
  if(!f)throw std::runtime_error("Could not save vector font evidence");
}
void Setup(int w,int h) {
  glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glDisable(GL_LINE_SMOOTH);
  glLineWidth(1);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,0,h,-1,1);
  glMatrixMode(GL_MODELVIEW);glLoadIdentity();glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor4f(1,0,0,1);
}
// Direct replay of fixed upstream table geometry. This reference never calls
// TGlFont, reads its metrics or reuses its lists. It shares the GL4ES/driver
// backend, so it is not an independent GPU line-rasterization implementation.
void DrawReference(double drawScale) {
  glPushMatrix();glTranslated(32,96,0);const double scale=18*drawScale/15;
  glScaled(scale,scale,1);
  for(unsigned char code:std::string("Ab09")) {
    const int* glyph=gl_font_simplex[code-32];int right=-1;bool begun=false;
    for(int j=2;j<112;j+=2) {
      if(glyph[j]==-1) {if(begun){glEnd();begun=false;}continue;}
      right=std::max(right,glyph[j]);if(!begun){glBegin(GL_LINE_STRIP);begun=true;}
      glVertex2d(static_cast<double>(glyph[j])/1200,static_cast<double>(glyph[j+1])/1200);
    }
    if(begun)glEnd();glTranslated(static_cast<double>(right)/1200,0,0);
  }
  glPopMatrix();glFinish();Check("independent table reference line strips");
}
void Snapshot(std::ostringstream& out,const Olex2LegacyHost& host,PlatformWindowScene& scene,
              TGlFont& font,int w,int h,unsigned index,const char* label,double drawScale) {
  Setup(w,h);short state=0;font.DrawVectorText(vec3d(32,96,0),olxstr("Ab09"),drawScale,state);
  glFinish();Check("genuine original vector text");
  float projection[16]={},modelview[16]={};glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,modelview);
  const std::string prefix=std::string("vector-")+label;
  std::vector<unsigned char> actual(static_cast<size_t>(w)*h*4),reference(actual.size());
  glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());Check("original vector actual readback");
  Save(std::string(host.files_dir)+"/"+prefix+"-actual.rgba",actual);
  if(host.gpu_snapshot)host.gpu_snapshot(host.run,(prefix+"-actual").c_str());
  Setup(w,h);DrawReference(drawScale);
  glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,reference.data());Check("vector direct-reference readback");
  Save(std::string(host.files_dir)+"/"+prefix+"-reference.rgba",reference);
  if(host.gpu_snapshot)host.gpu_snapshot(host.run,(prefix+"-reference").c_str());
  const auto& record=scene.FontRecord(font);
  if(index)out<<',';
  out<<std::setprecision(17)<<"{\"label\":\""<<label<<"\",\"font_address\":"<<reinterpret_cast<uintptr_t>(&font)
     <<",\"font_created\":"<<(font.IsCreated()?"true":"false")<<",\"is_vector\":"<<(font.IsVectorFont()?"true":"false")
     <<",\"point_size\":"<<font.GetPointSize()<<",\"draw_scale\":"<<drawScale<<",\"vector_scale\":"<<font.GetVectorScale()
     <<",\"max_width\":"<<font.GetMaxWidth()<<",\"max_height\":"<<font.GetMaxHeight()<<",\"leftmost\":"<<font.GetLeftmost()
     <<",\"char_offset\":"<<font.GetCharOffset()<<",\"text_width\":"<<font.TextWidth(olxstr("Ab09"))
     <<",\"builds\":"<<record.builds<<",\"releases\":"<<record.releases<<",\"rasterizations\":"<<record.rasterizations
     <<",\"draw_state_after\":"<<state<<",\"actual_equals_direct_reference\":"<<(actual==reference?"true":"false")
     <<",\"origin\":[32,96,0],\"text\":\"Ab09\",\"glyphs\":[";
  for(unsigned i=0;i<256;++i) {const auto* c=font.CharSize(i);if(i)out<<',';out<<'['<<c->Left<<','<<c->Top<<','<<c->Right<<','<<c->Bottom<<']';}
  out<<"],\"projection\":[";for(unsigned i=0;i<16;++i){if(i)out<<',';out<<projection[i];}
  out<<"],\"modelview\":[";for(unsigned i=0;i<16;++i){if(i)out<<',';out<<modelview[i];}out<<"]}";
}
}
std::string RunOriginalVectorFontWindowProbe(const Olex2LegacyHost& host,PlatformWindowScene& scene,int w,int h) {
  auto* registered=scene.FindFont("HarmonyOriginalVectorProbe");
  if(!registered)registered=&scene.CreateFont("HarmonyOriginalVectorProbe",olxstr("@18"));
  auto& normal=scene.GetFont(registered->GetId(),false);
  std::ostringstream out;out<<"{\"description\":\"@18\",\"genuine_CreateHershey\":true,\"reference_calls_TGlFont\":false,\"shared_GPU_backend\":true,\"cases\":[";
  Snapshot(out,host,scene,normal,w,h,0,"normal",1200);
  scene.ScaleFonts(1.5);
  try {
    auto& scaled=scene.GetFont(registered->GetId(),false);
    Snapshot(out,host,scene,scaled,w,h,1,"scene-scaled",1200);
    Snapshot(out,host,scene,scaled,w,h,2,"draw-scaled",1800);
  } catch(...) {scene.RestoreFontScale();throw;}
  scene.RestoreFontScale();auto& restored=scene.GetFont(registered->GetId(),false);
  Snapshot(out,host,scene,restored,w,h,3,"restored",1200);
  out<<"],\"same_font_object\":"<<(&normal==&restored?"true":"false")<<",\"source_table\":[";
  for(unsigned i=0;i<95;++i){if(i)out<<',';out<<'[';for(unsigned j=0;j<112;++j){if(j)out<<',';out<<gl_font_simplex[i][j];}out<<']';}
  out<<"],\"execution_completed\":true}";return out.str();
}
}
