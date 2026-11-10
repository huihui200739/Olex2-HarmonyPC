#include "original_font_window_probe.h"
#include "platform_font_factory.h"
#include "original_vector_font_window_probe.h"
#include "original_resource_font_window_probe.h"
#include "resampled_resource_font_window_probe.h"
#include "family_font_window_probe.h"
#include "glfont.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace olex2::harmony {
namespace {
void Check(const char* stage){const auto error=glGetError();if(error)throw std::runtime_error(std::string(stage)+" GL="+std::to_string(error));}
void Save(const std::string& path,const std::vector<unsigned char>& data){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());out.close();if(!out)throw std::runtime_error("Could not save original font evidence");}
void Snapshot(std::ostringstream& out,const Olex2LegacyHost& host,PlatformWindowScene& scene,TGlFont& font,int w,int h,unsigned index,const char* label) {
  glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,0,h,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glColor4f(1,0,0,1);glRasterPos2i(32,32);float projection[16]={},modelview[16]={};glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,modelview);font.DrawRasterText(olxstr("Ab09"));glFinish();Check("genuine original raster text");
  const auto& record=scene.FontRecord(font);const auto& cells=record.cells;
  const std::string prefix=std::string("font-")+label;
  std::vector<unsigned char> pixels(static_cast<size_t>(w)*h*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());Check("actual original font window readback");
  Save(std::string(host.files_dir)+"/"+prefix+"-actual.rgba",pixels);
  std::vector<unsigned char> raw;raw.reserve(static_cast<size_t>(cells.width)*cells.height*256*3);
  for(const auto& rgb:cells.rgb)raw.insert(raw.end(),rgb.begin(),rgb.end());
  Save(std::string(host.files_dir)+"/"+prefix+"-cells.rgb",raw);
  if(host.gpu_snapshot)host.gpu_snapshot(host.run,prefix.c_str());
  if(index)out<<',';
  out<<"{\"label\":\""<<label<<"\",\"font_address\":"<<reinterpret_cast<uintptr_t>(&font)
     <<",\"font_created\":"<<(font.IsCreated()?"true":"false")<<",\"point_size\":"<<font.GetPointSize()
     <<",\"display_metrics\":"<<FontDisplayMetricsJson(record.displayMetrics)
     <<",\"raster_pixel_size\":"<<std::setprecision(9)<<record.rasterPixelSize<<",\"raster_scale_x\":"<<record.rasterScaleX
     <<",\"cell_width\":"<<cells.width<<",\"cell_height\":"<<cells.height<<",\"baseline_x\":"<<cells.baselineX<<",\"baseline_y\":"<<cells.baselineY
     <<",\"max_width\":"<<font.GetMaxWidth()<<",\"max_height\":"<<font.GetMaxHeight()<<",\"leftmost\":"<<font.GetLeftmost()<<",\"char_offset\":"<<font.GetCharOffset()
     <<",\"fixed\":"<<(font.IsFixedWidth()?"true":"false")<<",\"builds\":"<<record.builds<<",\"releases\":"<<record.releases<<",\"rasterizations\":"<<record.rasterizations
     <<",\"origin\":[32,32],\"text\":\"Ab09\",\"glyphs\":[";
  for(unsigned c=0;c<256;++c){const auto* g=font.CharSize(c);if(c)out<<',';out<<'['<<g->Left<<','<<g->Top<<','<<g->Right<<','<<g->Bottom<<']';}
  out<<"],\"projection\":["<<std::setprecision(9);
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<projection[i];}
  out<<"],\"modelview\":[";
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<modelview[i];}
  out<<"]}";
}
}
std::string RunOriginalFontWindowProbe(const Olex2LegacyHost& host,PlatformWindowScene& scene,int w,int h) {
  auto* registered=scene.FindFont("HarmonyNativeProbe");
  if(!registered)registered=&scene.CreateFont("HarmonyNativeProbe",olxstr("ohos-native:18:r"));
  auto& font=scene.GetFont(registered->GetId(),false);
  std::ostringstream out;out<<"{\"display_metrics\":"<<FontDisplayMetricsJson(scene.DisplayMetrics())<<",\"native_rasterizer\":\"OH_Drawing\",\"genuine_TGlFont\":true,\"original_256_glyph_lists\":true,\"cases\":[";
  Snapshot(out,host,scene,font,w,h,0,"normal");
  scene.ScaleFonts(1.5);
  try {auto& scaled=scene.GetFont(registered->GetId(),false);Snapshot(out,host,scene,scaled,w,h,1,"scaled");}
  catch(...){scene.RestoreFontScale();throw;}
  scene.RestoreFontScale();auto& restored=scene.GetFont(registered->GetId(),false);Snapshot(out,host,scene,restored,w,h,2,"restored");
  out<<"],\"font_object_preserved\":"<<(&font==&restored?"true":"false")<<",\"vector\":"<<RunOriginalVectorFontWindowProbe(host,scene,w,h)<<",\"resource\":"<<RunOriginalResourceFontWindowProbe(host,scene,w,h)<<",\"resampled_resource\":"<<RunResampledResourceFontWindowProbe(host,scene,w,h)<<",\"system_family\":"<<RunFamilyFontWindowProbe(host,scene,w,h)<<",\"execution_completed\":true}";
  return out.str();
}
}
