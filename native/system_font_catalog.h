#pragma once
#include "family_font_description.h"
#include <native_drawing/drawing_font_mgr.h>
#include <native_drawing/drawing_typeface.h>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony {
struct SystemFontStyle {int index=0,weight=0,width=0,slant=0;std::string name;};
struct SystemFontFamily {int index=0;std::string name;std::vector<SystemFontStyle> styles;};
using OwnedSystemTypeface=std::unique_ptr<OH_Drawing_Typeface,decltype(&OH_Drawing_TypefaceDestroy)>;
struct SelectedSystemTypeface {
  OwnedSystemTypeface typeface{nullptr,OH_Drawing_TypefaceDestroy};
  SystemFontFamily family;
  SystemFontStyle style;
  bool actualBold=false,actualItalic=false;
};
// Owns the public FontMgr for this real factory, never installs or changes fonts.
class SystemFontCatalog {
  std::unique_ptr<OH_Drawing_FontMgr,decltype(&OH_Drawing_FontMgrDestroy)> manager_{nullptr,OH_Drawing_FontMgrDestroy};
  std::vector<SystemFontFamily> families_;
public:
  SystemFontCatalog();
  const std::vector<SystemFontFamily>& Families()const{return families_;}
  SelectedSystemTypeface SelectExact(const FamilyFontDescription&)const;
  std::string Json()const;
};
}
