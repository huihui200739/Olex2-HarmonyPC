#include "platform_font_factory.h"
#include "harmony_glyph_raster.h"
#include "font_description.h"
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
  const auto spec=ParseFontDescription(description);
  const unsigned effective=EffectiveFontPoints(spec,half,scale);
  NativeFontRecord* record=nullptr;
  for(auto& item:records_)if(item->font==&font){record=item.get();break;}
  if(!record){auto item=std::make_unique<NativeFontRecord>();item->font=&font;record=item.get();records_.push_back(std::move(item));}
  if(spec.kind==PlatformFontKind::OriginalHershey) {
    Check("before genuine original Hershey creation");
    try {
      olx_pdict<size_t,olxstr> originalDefaultDefinitions;
      font.CreateHershey(originalDefaultDefinitions,120);
      font.SetPointSize(static_cast<uint16_t>(effective));
      Check("genuine original Hershey display lists");
      if(!font.IsCreated()||!font.IsVectorFont())throw std::runtime_error("Original vector font was not created");
      record->cells=OriginalFontCells{};record->description=description;
      record->effectivePoints=effective;record->half=half;record->fixed=false;record->vector=true;
      ++record->builds;return font;
    } catch(...) {font.ClearData();throw;}
  }
  if(record->description!=description || record->effectivePoints!=effective || record->half!=half) {
    HarmonyGlyphRasterizer raster({static_cast<float>(effective*96.0/72.0),spec.bold,spec.italic,nullptr});
    std::array<RasterGlyph,256> glyphs;
    for(unsigned c=0;c<256;++c)glyphs[c]=raster.Rasterize(c);
    auto cells=PackOriginalFontCells(glyphs);
    record->cells=std::move(cells);record->description=description;record->effectivePoints=static_cast<unsigned>(effective);
    record->fixed=spec.fixed;record->half=half;record->vector=false;++record->rasterizations;
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
