// Real shell-context probe; never substitutes for the packaged app/GPU qualification.
#include "system_font_catalog.h"
#include "harmony_glyph_raster.h"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace olex2::harmony;
int main(){
  try{
    SystemFontCatalog catalog;const SystemFontFamily *first=nullptr,*other=nullptr;
    for(const auto& family:catalog.Families())if(family.styles.size()>1){first=&family;break;}
    if(!first)throw std::runtime_error("Need an actual installed family with two styles");
    for(const auto& family:catalog.Families())if(family.name!=first->name&&!family.styles.empty()){other=&family;break;}
    if(!other)throw std::runtime_error("Need two actual installed families");
    const std::array<const SystemFontFamily*,3> families{{first,other,first}};
    const std::array<size_t,3> styles{{0,0,1}};
    std::ostringstream out;out<<"{\"execution_context\":\"hdc-shell-not-application\",\"packaged_application_qualified\":false,\"GPU_qualified\":false,\"catalog\":"<<catalog.Json()<<",\"cases\":[";
    for(size_t i=0;i<3;++i){
      const auto& family=*families[i];const auto& style=family.styles.at(styles[i]);
      FamilyFontDescription spec{family.name,style.name,18,style.weight,style.width,style.slant,false};
      auto selected=catalog.SelectExact(spec);HarmonyGlyphRasterizer raster({24,false,false,selected.typeface.get()});
      std::array<RasterGlyph,256> glyphs;for(unsigned c=0;c<256;++c)glyphs[c]=raster.Rasterize(c);
      const auto cells=PackOriginalFontCells(glyphs);const std::string filename="selected-face-"+std::to_string(i)+".rgb";
      std::ofstream file(filename,std::ios::binary);for(const auto& rgb:cells.rgb)file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());file.close();
      if(!file)throw std::runtime_error("Could not save selected-family raw RGB evidence");
      if(i)out<<',';
      out<<"{\"family_index\":"<<family.index<<",\"style_index\":"<<style.index<<",\"description\":\""<<FamilyFontId(spec)<<"\",\"actual_face_bold\":"<<(selected.actualBold?"true":"false")<<",\"actual_face_italic\":"<<(selected.actualItalic?"true":"false")<<",\"cell_width\":"<<cells.width<<",\"cell_height\":"<<cells.height<<",\"baseline_x\":"<<cells.baselineX<<",\"baseline_y\":"<<cells.baselineY<<",\"file\":\""<<filename<<"\"}";
    }
    bool missingFamily=false,missingStyle=false;const auto& initial=first->styles[0];
    FamilyFontDescription absent{first->name,"",18,initial.weight,initial.width,initial.slant,false};
    absent.family="__Olex2AbsentShellFamily__";bool exists=true;
    while(exists){exists=false;for(const auto& family:catalog.Families())exists=exists||family.name==absent.family;if(exists)absent.family+='_';}
    try{auto face=catalog.SelectExact(absent);(void)face;}catch(const std::runtime_error&){missingFamily=true;}
    absent.family=first->name;absent.styleName="__Olex2AbsentShellStyle__";exists=true;
    while(exists){exists=false;for(const auto& style:first->styles)exists=exists||style.name==absent.styleName;if(exists)absent.styleName+='_';}
    try{auto face=catalog.SelectExact(absent);(void)face;}catch(const std::runtime_error&){missingStyle=true;}
    if(!missingFamily||!missingStyle)throw std::runtime_error("Missing exact selection was not rejected");
    out<<"],\"missing_family_rejected\":true,\"missing_style_rejected\":true,\"default_fallback\":false,\"synthetic_style\":false,\"execution_completed\":true}";
    std::cout<<out.str()<<'\n';return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
