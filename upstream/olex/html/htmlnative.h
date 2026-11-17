/******************************************************************************
* Copyright (c) 2004-2026 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/
#pragma once

#if defined(__OHOS__) && !defined(__WXWIDGETS__)
#include "native_widget.h"
#include "imgcellext.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct THtmlNativeLink {
  std::string href;
  std::string target;
};

struct THtmlNativeElement {
  enum class Kind { Text, Break, Image, RectMap, CircleMap } kind=Kind::Text;
  std::string text;
  std::map<std::string,std::string> attributes;
  std::string href, target;
  int headingLevel=0;
  double coordinates[4]={0,0,0,0};
  size_t coordinateCount=0;
};

// Parses the static HTML subset used by original help and information pages.
// It deliberately does not execute script tags, embedded macros, or INCLUDE.
class THtmlNativeParser {
public:
  static std::vector<THtmlNativeElement> Parse(const std::string& source);
};

class THtmlNativeDocument {
public:
  using ImageProvider=std::function<std::vector<uint8_t>(const std::string&)>;
  using LinkHandler=std::function<void(const THtmlNativeLink&)>;
private:
  olex2::harmony::ui::NodeBackend& backend_;
  olex2::harmony::ui::Widget root_;
  olex2::harmony::ui::Widget content_;
  std::vector<std::unique_ptr<olex2::harmony::ui::Widget>> textNodes_;
  std::vector<std::unique_ptr<olex2::harmony::ui::EventWidget>> linkNodes_;
  std::vector<std::unique_ptr<THtmlImageCell>> imageNodes_;
  ImageProvider imageProvider_;
  LinkHandler linkHandler_;
  std::string source_;
  void ClearRendered();
public:
  THtmlNativeDocument(olex2::harmony::ui::NodeBackend& backend,
    olex2::harmony::ui::Widget* parent=nullptr);
  THtmlNativeDocument(const THtmlNativeDocument&)=delete;
  THtmlNativeDocument& operator=(const THtmlNativeDocument&)=delete;
  olex2::harmony::ui::NodeHandle NativeNode() const { return root_.Handle(); }
  const std::string& Source() const { return source_; }
  void SetImageProvider(ImageProvider provider) { imageProvider_=std::move(provider); }
  void SetLinkHandler(LinkHandler handler) { linkHandler_=std::move(handler); }
  void SetPage(const std::string& html);
  void Layout(int viewportWidth,int viewportHeight);
};
#endif
