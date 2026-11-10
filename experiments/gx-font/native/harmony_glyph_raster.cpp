#include "harmony_glyph_raster.h"
#include <native_drawing/drawing_bitmap.h>
#include <native_drawing/drawing_brush.h>
#include <native_drawing/drawing_canvas.h>
#include <native_drawing/drawing_error_code.h>
#include <native_drawing/drawing_font.h>
#include <native_drawing/drawing_rect.h>
#include <native_drawing/drawing_text_blob.h>
#include <native_drawing/drawing_typeface.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace olex2::harmony {
namespace {
template<class T, void (*Destroy)(T*)>
using NativeOwner=std::unique_ptr<T,decltype(Destroy)>;
using Typeface=NativeOwner<OH_Drawing_Typeface,OH_Drawing_TypefaceDestroy>;
using Font=NativeOwner<OH_Drawing_Font,OH_Drawing_FontDestroy>;
using Rect=NativeOwner<OH_Drawing_Rect,OH_Drawing_RectDestroy>;
using Blob=NativeOwner<OH_Drawing_TextBlob,OH_Drawing_TextBlobDestroy>;
using Bitmap=NativeOwner<OH_Drawing_Bitmap,OH_Drawing_BitmapDestroy>;
using Brush=NativeOwner<OH_Drawing_Brush,OH_Drawing_BrushDestroy>;
using Canvas=NativeOwner<OH_Drawing_Canvas,OH_Drawing_CanvasDestroy>;
void Require(bool value,const char* message) {
  if(!value)throw std::runtime_error(message);
}
void CheckBound(float value) {
  Require(std::isfinite(value) && value>=-4090 && value<=4090,
          "Native glyph bounds are non-finite or exceed the bounded cell");
}
}
struct HarmonyGlyphRasterizer::Impl {
  // Reverse destruction order keeps the owned face alive until after the font.
  Typeface ownedFace{nullptr,OH_Drawing_TypefaceDestroy};
  Font font{nullptr,OH_Drawing_FontDestroy};
  explicit Impl(const GlyphFontSpec& spec) {
    if(!std::isfinite(spec.pixelSize) || spec.pixelSize<1 || spec.pixelSize>512)
      throw std::invalid_argument("Font pixel size must be finite and in [1,512]");
    auto* face=spec.typeface;
    if(!face) {
      ownedFace.reset(OH_Drawing_TypefaceCreateDefault());face=ownedFace.get();
      Require(face!=nullptr,"Native default typeface could not be created");
    }
    font.reset(OH_Drawing_FontCreate());
    Require(font!=nullptr,"Native font could not be created");
    OH_Drawing_FontSetTypeface(font.get(),face);
    OH_Drawing_FontSetTextSize(font.get(),spec.pixelSize);
    OH_Drawing_FontSetFakeBoldText(font.get(),spec.bold);
    OH_Drawing_FontSetTextSkewX(font.get(),spec.italic?-0.25f:0.0f);
    OH_Drawing_FontSetEdging(font.get(),FONT_EDGING_ALIAS);
  }
};
HarmonyGlyphRasterizer::HarmonyGlyphRasterizer(const GlyphFontSpec& spec)
  :impl_(std::make_unique<Impl>(spec)) {}
HarmonyGlyphRasterizer::~HarmonyGlyphRasterizer()=default;
RasterGlyph HarmonyGlyphRasterizer::Rasterize(uint32_t scalar) const {
  if(scalar>0x10ffff || (scalar>=0xd800 && scalar<=0xdfff))
    throw std::invalid_argument("Invalid Unicode scalar value");
  Rect bounds(OH_Drawing_RectCreate(0,0,0,0),OH_Drawing_RectDestroy);
  Require(bounds!=nullptr,"Native glyph bounds could not be created");
  float advance=0;
  Require(OH_Drawing_FontMeasureText(impl_->font.get(),&scalar,sizeof(scalar),
           TEXT_ENCODING_UTF32,bounds.get(),&advance)==OH_DRAWING_SUCCESS,
          "Native glyph measurement failed");
  Blob blob(OH_Drawing_TextBlobCreateFromText(&scalar,sizeof(scalar),
            impl_->font.get(),TEXT_ENCODING_UTF32),OH_Drawing_TextBlobDestroy);
  Require(blob!=nullptr,"Native glyph text blob could not be created");
  // Include both measured and actual blob bounds; preserve baseline and advance.
  float left=OH_Drawing_RectGetLeft(bounds.get());
  float top=OH_Drawing_RectGetTop(bounds.get());
  float right=OH_Drawing_RectGetRight(bounds.get());
  float bottom=OH_Drawing_RectGetBottom(bounds.get());
  for(float v:{advance,left,top,right,bottom})CheckBound(v);
  OH_Drawing_TextBlobGetBounds(blob.get(),bounds.get());
  const float bl=OH_Drawing_RectGetLeft(bounds.get()),bt=OH_Drawing_RectGetTop(bounds.get());
  const float br=OH_Drawing_RectGetRight(bounds.get()),bb=OH_Drawing_RectGetBottom(bounds.get());
  for(float v:{bl,bt,br,bb})CheckBound(v);
  left=std::min({left,bl,0.0f});top=std::min({top,bt,0.0f});
  right=std::max({right,br,advance,0.0f});bottom=std::max({bottom,bb,0.0f});
  RasterGlyph result;
  result.left=static_cast<int>(std::floor(left))-2;
  result.top=static_cast<int>(std::floor(top))-2;
  result.width=static_cast<uint32_t>(static_cast<int>(std::ceil(right))+2-result.left);
  result.height=static_cast<uint32_t>(static_cast<int>(std::ceil(bottom))+2-result.top);
  result.advance=advance;
  Require(result.width<=4096 && result.height<=4096,
          "Native glyph cell exceeds bounded dimensions");
  const size_t count=static_cast<size_t>(result.width)*result.height;
  std::vector<unsigned char> rgba(count*4,255);
  OH_Drawing_Image_Info info{static_cast<int32_t>(result.width),
    static_cast<int32_t>(result.height),COLOR_FORMAT_RGBA_8888,ALPHA_FORMAT_OPAQUE};
  // Own the storage. The native bitmap borrows it and dies before the vector.
  Bitmap bitmap(OH_Drawing_BitmapCreateFromPixels(&info,rgba.data(),result.width*4),
                OH_Drawing_BitmapDestroy);
  Require(bitmap!=nullptr,"Native glyph bitmap could not be created");
  Brush brush(OH_Drawing_BrushCreate(),OH_Drawing_BrushDestroy);
  Require(brush!=nullptr,"Native glyph brush could not be created");
  Canvas canvas(OH_Drawing_CanvasCreate(),OH_Drawing_CanvasDestroy);
  Require(canvas!=nullptr,"Native glyph canvas could not be created");
  OH_Drawing_CanvasBind(canvas.get(),bitmap.get());
  OH_Drawing_CanvasClear(canvas.get(),0xffffffffu);
  OH_Drawing_BrushSetColor(brush.get(),0xff000000u);
  OH_Drawing_BrushSetAntiAlias(brush.get(),false);
  OH_Drawing_CanvasAttachBrush(canvas.get(),brush.get());
  OH_Drawing_CanvasDrawTextBlob(canvas.get(),blob.get(),
    static_cast<float>(-result.left),static_cast<float>(-result.top));
  OH_Drawing_CanvasDetachBrush(canvas.get());
  result.rgb.resize(count*3);
  for(size_t i=0;i<count;++i) {
    Require(rgba[i*4+3]==255,"Native glyph output unexpectedly has non-opaque pixels");
    std::copy_n(rgba.data()+i*4,3,result.rgb.data()+i*3);
  }
  return result;
}
}
