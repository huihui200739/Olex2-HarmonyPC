#include "family_font_window_probe.h"
#include "platform_font_factory.h"
#include "glfont.h"
#include "eutf8.h"
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
  const std::string prefix=std::string("family-")+label;
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
     <<",\"explicit_system_family\":"<<(record.explicitSystemFamily?"true":"false")
     <<",\"family_hex\":\""<<FontNameHex(record.selectedFamily)<<"\",\"style_hex\":\""<<FontNameHex(record.selectedStyle)
     <<"\",\"description\":\""<<record.description<<"\",\"family_index\":"<<record.familyIndex<<",\"style_index\":"<<record.styleIndex
     <<",\"weight_code\":"<<record.weightCode<<",\"width_code\":"<<record.widthCode<<",\"slant_code\":"<<record.slantCode
     <<",\"actual_face_bold\":"<<(record.actualFaceBold?"true":"false")<<",\"actual_face_italic\":"<<(record.actualFaceItalic?"true":"false")
     <<",\"is_vector\":"<<(font.IsVectorFont()?"true":"false")
     <<",\"origin\":[32,32],\"text\":\"Ab09\",\"glyphs\":[";
  for(unsigned c=0;c<256;++c){const auto* g=font.CharSize(c);if(c)out<<',';out<<'['<<g->Left<<','<<g->Top<<','<<g->Right<<','<<g->Bottom<<']';}
  out<<"],\"projection\":["<<std::setprecision(9);
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<projection[i];}
  out<<"],\"modelview\":[";
  for(unsigned i=0;i<16;++i){if(i)out<<',';out<<modelview[i];}
  out<<"]}";
}
}
std::string RunFamilyFontWindowProbe(const Olex2LegacyHost& host,PlatformWindowScene& scene,int w,int h) {
  const auto& catalog=scene.FontCatalog();
  const auto text=catalog.Json();std::ofstream saved(std::string(host.files_dir)+"/system-font-catalog.json",std::ios::binary);
  saved<<text;saved.close();if(!saved)throw std::runtime_error("Could not save actual native font catalog");
  const SystemFontFamily *first=nullptr,*other=nullptr;
  for(const auto& family:catalog.Families())if(family.styles.size()>1){first=&family;break;}
  if(!first)throw std::runtime_error("No actual system family has two exact styles; cannot qualify real style selection");
  for(const auto& family:catalog.Families())if(family.name!=first->name&&!family.styles.empty()){other=&family;break;}
  if(!other)throw std::runtime_error("No second actual system font family; cannot qualify family selection");
  auto spec=[](const SystemFontFamily& family,size_t index){const auto& style=family.styles.at(index);return FamilyFontDescription{family.name,style.name,18,style.weight,style.width,style.slant,false};};
  const auto a=spec(*first,0),b=spec(*other,0),c=spec(*first,1);
  bool missingFamily=false,missingStyle=false;auto absent=a;absent.family="__Olex2AbsentSystemFamily__";
  while(true){bool exists=false;for(const auto& family:catalog.Families())exists=exists||family.name==absent.family;if(!exists)break;absent.family+='_';}
  try {auto selected=catalog.SelectExact(absent);(void)selected;}catch(const std::runtime_error&){missingFamily=true;}
  absent=a;absent.styleName="__Olex2AbsentSystemStyle__";
  while(true){bool exists=false;for(const auto& style:first->styles)exists=exists||style.name==absent.styleName;if(!exists)break;absent.styleName+='_';}
  try {auto selected=catalog.SelectExact(absent);(void)selected;}catch(const std::runtime_error&){missingStyle=true;}
  auto registered=[&](const char* name,const FamilyFontDescription& description)->TGlFont*{
    auto* font=scene.FindFont(name);const auto id=FamilyFontId(description);
    if(!font)font=&scene.CreateFont(name,olxstr(id.c_str()));
    if(TUtf8::Encode(font->GetIdString())!=id.c_str())throw std::runtime_error("Stable family probe description changed");
    return font;
  };
  auto* normal=registered("HarmonyFamilyProbeFirst",a);
  std::ostringstream out;out<<"{\"exact_family_style_api\":\"OH_Drawing_FontStyleSetCreateTypeface\",\"default_fallback\":false,\"closest_style_match\":false,\"synthetic_style\":false,\"physical_point_dpi\":["<<std::setprecision(17)<<scene.DisplayMetrics().dpiX<<","<<scene.DisplayMetrics().dpiY<<"],\"actual_display_DPI_observed\":"<<(scene.DisplayMetrics().ready?"true":"false")<<",\"actual_display_DPI_qualified\":false,\"missing_family_rejected\":"<<(missingFamily?"true":"false")<<",\"missing_style_rejected\":"<<(missingStyle?"true":"false")<<",\"cases\":[";
  auto& initial=scene.GetFont(normal->GetId(),false);Snapshot(out,host,scene,initial,w,h,0,"first");
  scene.ScaleFonts(1.5);
  try{auto& font=scene.GetFont(normal->GetId(),false);Snapshot(out,host,scene,font,w,h,1,"scaled");}
  catch(...){scene.RestoreFontScale();throw;}
  scene.RestoreFontScale();auto& restored=scene.GetFont(normal->GetId(),false);Snapshot(out,host,scene,restored,w,h,2,"restored");
  auto* second=registered("HarmonyFamilyProbeOther",b);auto& secondFont=scene.GetFont(second->GetId(),false);Snapshot(out,host,scene,secondFont,w,h,3,"other-family");
  auto* alternate=registered("HarmonyFamilyProbeStyle",c);auto& alternateFont=scene.GetFont(alternate->GetId(),false);Snapshot(out,host,scene,alternateFont,w,h,4,"other-style");
  out<<"],\"font_object_preserved\":"<<(&initial==&restored?"true":"false")<<",\"execution_completed\":true}";return out.str();
}
}
