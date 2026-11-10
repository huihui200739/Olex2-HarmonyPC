#include "platform_font_factory.h"
#include "harmony_glyph_raster.h"
#include "font_description.h"
#include "olex_font_resource.h"
#include "resource_font_resample.h"
#include "bapp.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace olex2::harmony {
namespace {
void Check(const char* stage) {
  const auto error=glGetError();if(error)throw std::runtime_error(std::string(stage)+" GL="+std::to_string(error));
}
struct BorrowedCells {
  TGlFont& font;
  ~BorrowedCells(){for(size_t c=0;c<256;++c)font.CharSize(c)->Data=nullptr;}
};
}
TGlFont& PlatformFontFactory::Create(TGlFont& font,bool half,double scale) const {
  const std::string description=TUtf8::Encode(font.GetIdString()).c_str();
  const bool resource=!description.empty()&&description.front()=='#';
  const bool family=IsFamilyFontId(description);
  FamilyFontDescription familySpec;
  OlexResourceFontDescription resourceSpec;
  FontDescription spec;
  if(resource){resourceSpec=ParseOlexResourceFontDescription(description);spec.points=resourceSpec.points;spec.bold=resourceSpec.bold;spec.italic=resourceSpec.italic;spec.fixed=resourceSpec.fixed;}
  else if(family){familySpec=ParseFamilyFontId(description);spec.points=familySpec.points;spec.fixed=familySpec.fixed;}
  else spec=ParseFontDescription(description);
  const unsigned requested=EffectiveFontPoints(spec,half,scale);
  const unsigned effective=resource?PlanResourceFont(requested).points:requested;
  NativeFontRecord* record=nullptr;
  for(auto& item:records_)if(item->font==&font){record=item.get();break;}
  if(!record){auto item=std::make_unique<NativeFontRecord>();item->font=&font;record=item.get();records_.push_back(std::move(item));}
  if(resource) {
    const auto plan=PlanResourceFont(effective);
    const std::string base=TUtf8::Encode(TBasicApp::GetBaseDir()).c_str();
    const std::string path=base+"etc/Fonts/"+resourceSpec.file;
    if(record->description!=description||record->effectivePoints!=effective||record->half!=half||record->resourcePath!=path) {
      auto data=LoadOlexFontResource(path,plan.Entry(resourceSpec));
      OriginalFontCells cells;
      if(plan.useRGB)cells=ResampleResourceFont(data,plan);
      record->resourceData=std::move(data);record->description=description;record->effectivePoints=effective;
      record->resourceUsesRGB=plan.useRGB;record->resourceEntryPoints=plan.entryPoints;
      record->cells=std::move(cells);if(plan.useRGB)++record->resourceResamplings;
      record->half=half;record->fixed=resourceSpec.fixed;record->vector=false;record->resource=true;record->explicitSystemFamily=false;
      record->resourcePath=path;++record->resourceReads;
    }
    Check("before original bit-array resource font");GLint alignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    try {
      font.ClearData();font.SetPointSize(static_cast<uint16_t>(effective));
      if(record->resourceUsesRGB){
        BorrowedCells borrow{font};
        for(size_t c=0;c<256;++c)font.CharFromRGBArray(c,record->cells.rgb[c].data(),record->cells.width,record->cells.height);
        font.CreateGlyphsFromRGBArray(record->fixed,record->cells.width,record->cells.height);
      }else{
        TEBitArray bits(record->resourceData.bits.data(),record->resourceData.bits.size(),false);
        font.CreateGlyphs(bits,record->fixed,record->resourceData.width,record->resourceData.height);
      }
      Check("genuine original resource glyph creation");
      glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);Check("original resource pixel-store restoration");
      if(!font.IsCreated()||font.IsVectorFont())throw std::runtime_error("Original resource bitmap font was not created");
      ++record->builds;return font;
    } catch(...) {font.ClearData();glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);throw;}
  }
  if(spec.kind==PlatformFontKind::OriginalHershey) {
    Check("before genuine original Hershey creation");
    try {
      olx_pdict<size_t,olxstr> originalDefaultDefinitions;
      font.CreateHershey(originalDefaultDefinitions,120);
      font.SetPointSize(static_cast<uint16_t>(effective));
      Check("genuine original Hershey display lists");
      if(!font.IsCreated()||!font.IsVectorFont())throw std::runtime_error("Original vector font was not created");
      record->cells=OriginalFontCells{};record->description=description;
      record->effectivePoints=effective;record->half=half;record->fixed=false;record->vector=true;record->resource=false;record->explicitSystemFamily=false;
      ++record->builds;return font;
    } catch(...) {font.ClearData();throw;}
  }
  if(record->description!=description || record->effectivePoints!=effective || record->half!=half) {
    SelectedSystemTypeface selected;
    if(family)selected=FontCatalog().SelectExact(familySpec);
    // Exact native faces provide their own real weight/slant; never synthesize them.
    // The existing explicit ohos-native branch keeps its historical behavior.
    HarmonyGlyphRasterizer raster({static_cast<float>(effective*96.0/72.0),
      family?false:spec.bold,family?false:spec.italic,selected.typeface.get()});
    std::array<RasterGlyph,256> glyphs;
    for(unsigned c=0;c<256;++c)glyphs[c]=raster.Rasterize(c);
    auto cells=PackOriginalFontCells(glyphs);
    record->cells=std::move(cells);record->description=description;record->effectivePoints=static_cast<unsigned>(effective);
    record->fixed=spec.fixed;record->half=half;record->vector=false;record->resource=false;
    record->explicitSystemFamily=family;
    record->selectedFamily=family?familySpec.family:std::string();record->selectedStyle=family?familySpec.styleName:std::string();
    record->familyIndex=family?selected.family.index:-1;record->styleIndex=family?selected.style.index:-1;
    record->weightCode=family?familySpec.weight:0;record->widthCode=family?familySpec.width:0;record->slantCode=family?familySpec.slant:0;
    record->actualFaceBold=family&&selected.actualBold;record->actualFaceItalic=family&&selected.actualItalic;
    ++record->rasterizations;
  }
  Check("before original font creation");
  if(font.IsCreated())font.ClearData();
  GLint alignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
  try {
    BorrowedCells borrow{font};
    font.SetPointSize(static_cast<uint16_t>(record->effectivePoints));
    for(size_t c=0;c<256;++c)font.CharFromRGBArray(c,record->cells.rgb[c].data(),record->cells.width,record->cells.height);
    font.CreateGlyphsFromRGBArray(record->fixed,record->cells.width,record->cells.height);
    Check("genuine TGlFont glyph creation");
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);Check("original font pixel-store restoration");
    if(!font.IsCreated())throw std::runtime_error("Genuine original font did not become created");
    ++record->builds;return font;
  } catch(...) {
    for(size_t c=0;c<256;++c)font.CharSize(c)->Data=nullptr;
    font.ClearData();glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);throw;
  }
}
const SystemFontCatalog& PlatformFontFactory::FontCatalog()const {
  if(!catalog_)catalog_=std::make_unique<SystemFontCatalog>();
  return *catalog_;
}
void PlatformFontFactory::Release() const {
  for(auto& record:records_)if(record->font->IsCreated()){record->font->ClearData();++record->releases;}
  Check("original font release before context retirement");
}
const NativeFontRecord& PlatformFontFactory::Record(const TGlFont& font) const {
  for(const auto& item:records_)if(item->font==&font)return *item;
  throw std::logic_error("Font has not been created by the platform factory");
}
bool PlatformFontFactory::AllReleased() const {
  for(const auto& item:records_)if(item->font->IsCreated())return false;
  return true;
}
}
