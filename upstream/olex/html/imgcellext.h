/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_image_cell_ext_H
#define __olx_image_cell_ext_H
#if defined(__OHOS__) && !defined(__WXWIDGETS__)
// Harmony-native HTML image cell. The ArkUI backend owns image decoding and
// the displayed drawable; this class owns only model data and native nodes.
#include "native_widget.h"
#include <functional>
#include <string>
#include <vector>

struct THtmlImageMapLink {
  std::string href;
  std::string target;
};

class THtmlImageCell {
  struct Shape {
    enum class Kind { Rectangle, Circle } kind;
    double a=0,b=0,c=0,d=0;
    THtmlImageMapLink link;
  };
  olex2::harmony::ui::NodeBackend& backend_;
  olex2::harmony::ui::Widget root_;
  olex2::harmony::ui::EventWidget image_;
  olex2::harmony::ui::ImageRef imageAsset_;
  std::vector<Shape> shapes_;
  std::string source_,text_;
  int requestedWidth_=-1,requestedHeight_=-1;
  int renderedWidth_=0,renderedHeight_=0;
  double scale_=1.0;
  bool widthPercent_=false,heightPercent_=false,showFrame_=false;
  std::function<void(const THtmlImageMapLink&)> linkHandler_;
  void OnPointer(const olex2::harmony::ui::NodeEvent& event);
  static int CheckedDimension(double value);
public:
  explicit THtmlImageCell(olex2::harmony::ui::NodeBackend& backend,
    olex2::harmony::ui::Widget* parent=nullptr);
  THtmlImageCell(const THtmlImageCell&)=delete;
  THtmlImageCell& operator=(const THtmlImageCell&)=delete;
  olex2::harmony::ui::NodeHandle NativeNode() const { return root_.Handle(); }
  olex2::harmony::ui::NodeHandle ImageNode() const { return image_.Handle(); }
  void SetImageData(const std::vector<uint8_t>& encoded,const std::string& source={});
  bool OnFileChange(const std::vector<uint8_t>& replacement);
  void SetText(const std::string& text);
  const std::string& GetText() const { return text_; }
  void SetSource(const std::string& source) { source_=source; }
  const std::string& GetSource() const { return source_; }
  void SetRequestedSize(int width,int height,double scale=1.0,
    bool widthInPercent=false,bool heightInPercent=false);
  void SetShowFrame(bool value);
  void SetLinkHandler(std::function<void(const THtmlImageMapLink&)> handler);
  void AddRect(double left,double top,double right,double bottom,
    const std::string& href,const std::string& target={});
  void AddCircle(double x,double y,double radius,const std::string& href,
    const std::string& target={});
  bool GetLink(double x,double y,THtmlImageMapLink& link) const;
  void Layout(int parentWidth,int parentHeight);
  int GetWidth() const { return renderedWidth_; }
  int GetHeight() const { return renderedHeight_; }
};

class THtmlWidgetCell {
  int height_=0;
public:
  void SetHeight(int height);
  int GetHeight() const { return height_; }
};
#else // OLEX2_HTML_IMAGE_WX_BRANCH
#include "estrlist.h"
#include "emath.h"
#include "wx/wxhtml.h"
#include "wx/gifdecod.h"
#include "wx/timer.h"
#include "../ctrls/olxctrlbase.h"

class THtmlImageCell : public wxHtmlCell, public AOlxCtrl {
  struct Rect {
    short left, top, right, bottom;
    Rect(short l, short t, short r, short b)
      : left(l), top(t), right(r), bottom(b)
    {}
    bool IsInside(short x, short y) const {
      return (x >= left && x <= right && y >= top && y <= bottom);
    }
  };
  struct Circle {
    short x, y;
    float qr;
    Circle(short _x, short _y, float _r)
      : x(_x), y(_y), qr(_r* _r)
    {}
    bool IsInside(short _x, short _y) const {
      return (olx_sqr(_x - x) + olx_sqr(_y - y) <= qr);
    }
  };
  struct AShapeInfo {
    wxHtmlLinkInfo* link;
    AShapeInfo(wxHtmlLinkInfo* lnk)
      : link(lnk)
    {}
    virtual ~AShapeInfo() { delete link; }
    virtual bool IsInside(short x, short y) const = 0;
  };
  template <class Shape>
  struct ShapeInfo : public AShapeInfo {
    Shape shape;
    ShapeInfo(const Shape& _shape, wxHtmlLinkInfo* lnk)
      : AShapeInfo(lnk), shape(_shape)
    {}
    virtual bool IsInside(short x, short y) const { return shape.IsInside(x, y); }
  };
  TTypeList<AShapeInfo> Shapes;
  void Draw(wxDC& dc, int x, int y);
public:
  THtmlImageCell(wxHtmlWindowInterface* windowIface,
    wxFSFile* input, int w = wxDefaultCoord, int h = wxDefaultCoord,
    double scale = 1.0, int align = wxHTML_ALIGN_BOTTOM,
    const wxString& mapname = wxEmptyString,
    bool WidthInPercent = false,
    bool HeightInPercent = false);
  ~THtmlImageCell();
  void Draw(wxDC& dc, int x, int y, int WXUNUSED(view_y1), int WXUNUSED(view_y2),
    wxHtmlRenderingInfo& WXUNUSED(info)) {
    Draw(dc, x, y);
  }
  virtual wxHtmlLinkInfo* GetLink(int x = 0, int y = 0) const;
  wxHtmlWindowInterface* GetWindowInterface() { return m_windowIface; }
  const wxHtmlWindowInterface* GetWindowInterface() const { return m_windowIface; }
  void SetImage(const wxImage& img);
  void AddRect(short l, short t, short r, short b, const wxString& href, const wxString& target) {
    Shapes.Add(new ShapeInfo<Rect>(Rect(l, t, r, b), new wxHtmlLinkInfo(href, target)));
  }
  void AddCircle(short x, short y, float r, const wxString& href, const wxString& target) {
    Shapes.Add(new ShapeInfo<Circle>(Circle(x, y, r), new wxHtmlLinkInfo(href, target)));
  }
#if wxUSE_GIF && wxUSE_TIMER
  void AdvanceAnimation(wxTimer* timer);
  virtual void Layout(int w);
#endif
  void SetText(const wxString& text) { Text = text; }
  const wxString& GetText() const { return Text; }

  void SetSource(const olxstr& text) { FSource = text; }
  const olxstr& GetSource() const { return FSource; }
  bool OnFileChange();
private:
  olx_object_ptr<wxBitmap> m_bitmap;
  olx_object_ptr<wxFSFile> File;
  wxString Text;
  olxstr FSource;
  int m_bmpW, m_bmpH;
  bool m_showFrame : 1;
  bool WidthInPercent, HeightInPercent;
  wxHtmlWindowInterface* m_windowIface;
#if wxUSE_GIF && wxUSE_TIMER
  olx_object_ptr<wxGIFDecoder> m_gifDecoder;
  olx_object_ptr<class TGIFTimer> m_gifTimer;
  int m_physX, m_physY;
  size_t m_nCurrFrame;
#endif
  double m_scale;
  wxString m_mapName;

  DECLARE_NO_COPY_CLASS(THtmlImageCell)
};

#if wxUSE_GIF && wxUSE_TIMER
class TGIFTimer : public wxTimer {
public:
  TGIFTimer(THtmlImageCell* cell);
  virtual void Notify();
private:
  THtmlImageCell* m_cell;
  DECLARE_NO_COPY_CLASS(TGIFTimer)
};
#endif

#endif // OLEX2_HTML_IMAGE_PLATFORM_BRANCH
#endif
