#include "platform_font_factory.h"
#include "harmony_glyph_raster.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace olex2::harmony {
namespace {
struct Description {unsigned points=0;bool bold=false,italic=false,fixed=false;};
Description Parse(const std::string& text) {
  const std::string prefix="ohos-native:";
  if(text.rfind(prefix,0)!=0 || text.size()>80)
    throw std::invalid_argument("Unsupported font description; explicit ohos-native description required");
  const auto colon=text.find(':',prefix.size());
  if(colon==std::string::npos || colon==prefix.size())throw std::invalid_argument("Missing native font size/style");
  Description result;
  for(size_t i=prefix.size();i<colon;++i) {
    if(text[i]<'0'||text[i]>'9'||result.points>128)throw std::invalid_argument("Invalid bounded native font size");
    result.points=result.points*10+static_cast<unsigned>(text[i]-'0');
  }
  if(!result.points || result.points>128)throw std::invalid_argument("Native point size must be in [1,128]");
  auto style=text.substr(colon+1);const std::string suffix=":fixed";
  if(style.size()>suffix.size() && style.compare(style.size()-suffix.size(),suffix.size(),suffix)==0) {
    result.fixed=true;style.resize(style.size()-suffix.size());
  }
  if(style!="r"&&style!="b"&&style!="i"&&style!="bi")throw std::invalid_argument("Unknown native font style");
  result.bold=style.find('b')!=std::string::npos;result.italic=style.find('i')!=std::string::npos;
  return result;
}
void Check(const char* stage) {
  const auto error=glGetError();if(error)throw std::runtime_error(std::string(stage)+" GL="+std::to_string(error));
}
struct BorrowedCells {
  TGlFont& font;
  ~BorrowedCells(){for(size_t c=0;c<256;++c)font.CharSize(c)->Data=nullptr;}
};
}
TGlFont& PlatformFontFactory::Create(TGlFont& font,bool half,double scale) const {
  if(!std::isfinite(scale)||scale<=0||scale>8)throw std::invalid_argument("Invalid native font scale");
  const std::string description=TUtf8::Encode(font.GetIdString()).c_str();
  const auto spec=Parse(description);
  const double effective=std::floor(spec.points*scale*(half?0.75:1.0));
  if(effective<1||effective>256)throw std::invalid_argument("Scaled native font exceeds bounded size");
  NativeFontRecord* record=nullptr;
  for(auto& item:records_)if(item->font==&font){record=item.get();break;}
  if(!record){auto item=std::make_unique<NativeFontRecord>();item->font=&font;record=item.get();records_.push_back(std::move(item));}
  if(record->description!=description || record->effectivePoints!=static_cast<unsigned>(effective) || record->half!=half) {
    HarmonyGlyphRasterizer raster({static_cast<float>(effective*96.0/72.0),spec.bold,spec.italic,nullptr});
    std::array<RasterGlyph,256> glyphs;
    for(unsigned c=0;c<256;++c)glyphs[c]=raster.Rasterize(c);
    auto cells=PackOriginalFontCells(glyphs);
    record->cells=std::move(cells);record->description=description;record->effectivePoints=static_cast<unsigned>(effective);
    record->fixed=spec.fixed;record->half=half;++record->rasterizations;
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
