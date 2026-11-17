/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(__OHOS__) && !defined(__WXWIDGETS__)
#include "imgcellext.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
using namespace olex2::harmony::ui;

THtmlImageCell::THtmlImageCell(NodeBackend& backend,Widget* parent)
  :backend_(backend),root_(backend,NodeKind::Column,parent),
   image_(backend,NodeKind::Image,&root_,{EventKind::Pointer},
     [this](const NodeEvent& event){OnPointer(event);}) {
  image_.SetNumber(Property::ImageStretch,0);
}
int THtmlImageCell::CheckedDimension(double value) {
  if(!std::isfinite(value)||value<0||value>32768)
    throw std::invalid_argument("HTML image dimension is outside the native layout range");
  return static_cast<int>(std::ceil(value));
}
void THtmlImageCell::SetImageData(const std::vector<uint8_t>& encoded,const std::string& source) {
  if(encoded.empty())throw std::invalid_argument("HTML image data is empty");
  ImageRef decoded=backend_.DecodeImage(encoded);
  if(!decoded)throw std::runtime_error("Native image decoder returned no image");
  if(decoded->width>32768||decoded->height>32768)
    throw std::invalid_argument("Decoded HTML image exceeds the native layout range");
  std::string stagedSource=source.empty()?source_:source;
  const ImageRef previous=imageAsset_;
  const int oldWidth=renderedWidth_,oldHeight=renderedHeight_;
  const bool natural=requestedWidth_<0&&requestedHeight_<0&&!widthPercent_&&!heightPercent_;
  const int naturalWidth=natural?CheckedDimension(decoded->width*scale_):oldWidth;
  const int naturalHeight=natural?CheckedDimension(decoded->height*scale_):oldHeight;
  try {
    image_.SetImage(decoded);
    if(natural) {
      root_.SetWidth(naturalWidth);root_.SetHeight(naturalHeight);
      image_.SetWidth(naturalWidth);image_.SetHeight(naturalHeight);
    }
    image_.SetText(TextProperty::Accessibility,text_);
  }
  catch(...) {
    try { image_.SetImage(previous); } catch(...) {}
    try { root_.SetWidth(oldWidth);root_.SetHeight(oldHeight);image_.SetWidth(oldWidth);image_.SetHeight(oldHeight); } catch(...) {}
    throw;
  }
  imageAsset_=std::move(decoded);
  renderedWidth_=natural?naturalWidth:oldWidth;
  renderedHeight_=natural?naturalHeight:oldHeight;
  source_=std::move(stagedSource);
}
bool THtmlImageCell::OnFileChange(const std::vector<uint8_t>& replacement) {
  if(replacement.empty())return false;
  try { SetImageData(replacement,source_);return true; }
  catch(...) { return false; }
}
void THtmlImageCell::SetText(const std::string& text) {
  if(text.find('\0')!=std::string::npos)throw std::invalid_argument("HTML image text contains an embedded NUL");
  std::string staged=text;
  image_.SetText(TextProperty::Accessibility,staged);
  text_=std::move(staged);
}
void THtmlImageCell::SetRequestedSize(int width,int height,double scale,
    bool widthInPercent,bool heightInPercent) {
  if(width==0||height==0||width < -1||height < -1)
    throw std::invalid_argument("HTML image size must be positive or intrinsic");
  if((widthInPercent&&width<1)||(heightInPercent&&height<1))
    throw std::invalid_argument("Percent-sized HTML images require explicit percentages");
  if((widthInPercent&&width>1000)||(heightInPercent&&height>1000))
    throw std::invalid_argument("HTML image percentage is outside the supported range");
  if(!std::isfinite(scale)||scale<=0||scale>16)
    throw std::invalid_argument("HTML image scale must be finite and between 0 and 16");
  requestedWidth_=width;requestedHeight_=height;scale_=scale;
  widthPercent_=widthInPercent;heightPercent_=heightInPercent;
}
void THtmlImageCell::SetShowFrame(bool value) {
  if(value==showFrame_)return;
  root_.SetNumber(Property::BorderWidth,value?1:0);
  if(value)root_.SetNumber(Property::BorderColor,0xff000000u);
  showFrame_=value;
}
void THtmlImageCell::SetLinkHandler(std::function<void(const THtmlImageMapLink&)> handler) {
  linkHandler_=std::move(handler);
}
void THtmlImageCell::AddRect(double left,double top,double right,double bottom,
    const std::string& href,const std::string& target) {
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom)||
      left>right||top>bottom||href.empty())
    throw std::invalid_argument("Invalid HTML image-map rectangle");
  shapes_.push_back({Shape::Kind::Rectangle,left,top,right,bottom,{href,target}});
}
void THtmlImageCell::AddCircle(double x,double y,double radius,const std::string& href,
    const std::string& target) {
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(radius)||radius<=0||href.empty())
    throw std::invalid_argument("Invalid HTML image-map circle");
  shapes_.push_back({Shape::Kind::Circle,x,y,radius,0,{href,target}});
}
bool THtmlImageCell::GetLink(double x,double y,THtmlImageMapLink& link) const {
  if(!imageAsset_||renderedWidth_<=0||renderedHeight_<=0||!std::isfinite(x)||!std::isfinite(y)||
      x<0||y<0||x>=renderedWidth_||y>=renderedHeight_)return false;
  const double fit=std::min(static_cast<double>(renderedWidth_)/imageAsset_->width,
    static_cast<double>(renderedHeight_)/imageAsset_->height);
  const double contentWidth=imageAsset_->width*fit,contentHeight=imageAsset_->height*fit;
  const double offsetX=(renderedWidth_-contentWidth)/2,offsetY=(renderedHeight_-contentHeight)/2;
  if(x<offsetX||y<offsetY||x>=offsetX+contentWidth||y>=offsetY+contentHeight)return false;
  const double imageX=(x-offsetX)/fit;
  const double imageY=(y-offsetY)/fit;
  for(auto it=shapes_.rbegin();it!=shapes_.rend();++it) {
    bool inside=false;
    if(it->kind==Shape::Kind::Rectangle)
      inside=imageX>=it->a&&imageX<=it->c&&imageY>=it->b&&imageY<=it->d;
    else {const double dx=imageX-it->a,dy=imageY-it->b;inside=dx*dx+dy*dy<=it->c*it->c;}
    if(inside){link=it->link;return true;}
  }
  return false;
}
void THtmlImageCell::Layout(int parentWidth,int parentHeight) {
  if(!imageAsset_)throw std::logic_error("Cannot lay out an HTML image before decoding it");
  if(parentWidth<=0||parentHeight<=0)throw std::invalid_argument("HTML image parent size must be positive");
  const double intrinsicWidth=imageAsset_->width,intrinsicHeight=imageAsset_->height;
  double width=requestedWidth_>0?requestedWidth_:intrinsicWidth;
  double height=requestedHeight_>0?requestedHeight_:intrinsicHeight;
  if(widthPercent_)width=parentWidth*static_cast<double>(requestedWidth_)/100.0;
  if(heightPercent_)height=parentHeight*static_cast<double>(requestedHeight_)/100.0;
  if(widthPercent_&&requestedHeight_<0)height=width*intrinsicHeight/intrinsicWidth;
  if(heightPercent_&&requestedWidth_<0)width=height*intrinsicWidth/intrinsicHeight;
  if(requestedWidth_>0&&requestedHeight_<0&&!widthPercent_)height=width*intrinsicHeight/intrinsicWidth;
  if(requestedHeight_>0&&requestedWidth_<0&&!heightPercent_)width=height*intrinsicWidth/intrinsicHeight;
  const int newWidth=CheckedDimension(width*scale_),newHeight=CheckedDimension(height*scale_);
  if(newWidth<=0||newHeight<=0)throw std::invalid_argument("HTML image layout collapsed to zero size");
  const int oldRootWidth=root_.GetWidth(),oldRootHeight=root_.GetHeight();
  const int oldImageWidth=image_.GetWidth(),oldImageHeight=image_.GetHeight();
  try {
    root_.SetWidth(newWidth);root_.SetHeight(newHeight);
    image_.SetWidth(newWidth);image_.SetHeight(newHeight);
  }
  catch(...) {
    try { root_.SetWidth(oldRootWidth); } catch(...) {}
    try { root_.SetHeight(oldRootHeight); } catch(...) {}
    try { image_.SetWidth(oldImageWidth); } catch(...) {}
    try { image_.SetHeight(oldImageHeight); } catch(...) {}
    throw;
  }
  renderedWidth_=newWidth;renderedHeight_=newHeight;
}
void THtmlImageCell::OnPointer(const NodeEvent& event) {
  if(event.kind!=EventKind::Pointer||event.pointer==nullptr||
      event.pointer->phase!=PointerPhase::Up||!event.pointer->inside||!linkHandler_)return;
  THtmlImageMapLink link;
  if(GetLink(event.pointer->x,event.pointer->y,link))linkHandler_(link);
}
void THtmlWidgetCell::SetHeight(int height) {
  if(height<0||height>32768)throw std::invalid_argument("HTML widget height is outside the native layout range");
  height_=height;
}

#else // OLEX2_HTML_IMAGE_WX_BRANCH
#include "imgcellext.h"
#include "widgetcellext.h"
#include "bapp.h"
#include "log.h"
#include "wx/dc.h"
#include "wx/artprov.h"
#include "fsext.h"

THtmlImageCell::THtmlImageCell(wxHtmlWindowInterface* windowIface, wxFSFile *input,
  int w, int h, double scale, int align,
  const wxString& mapname,
  bool width_per, bool height_per)
  : wxHtmlCell(), AOlxCtrl(windowIface->GetHTMLWindow())
{
  m_windowIface = windowIface;
  m_scale = scale;
  m_showFrame = false;
  m_bmpW = w;
  m_bmpH = h;
  m_mapName = mapname;
  SetCanLiveOnPagebreak(false);
#if wxUSE_GIF && wxUSE_TIMER
  File = input;
  m_physX = m_physY = wxDefaultCoord;
  m_nCurrFrame = 0;
#endif
  if (m_bmpW && m_bmpH) {
    if (input != 0) {
      wxInputStream *s = input->GetStream();
      if (s != 0) {
#if wxUSE_GIF && wxUSE_TIMER
        bool readImg = true;
        if (m_windowIface &&
          (input->GetLocation().Matches(wxT("*.gif")) ||
            input->GetLocation().Matches(wxT("*.GIF"))))
        {
          m_gifDecoder = new wxGIFDecoder();
          if (m_gifDecoder->LoadGIF(*s) == wxGIF_OK) {
            wxImage img;
            if (m_gifDecoder->ConvertToImage(0, &img)) {
              SetImage(img);
            }
            readImg = false;
            if (m_gifDecoder->IsAnimation()) {
              m_gifTimer = new TGIFTimer(this);
              long delay = m_gifDecoder->GetDelay(0);
              if (delay == 0) {
                delay = 1;
              }
              m_gifTimer->Start(delay, true);
            }
            else {
              m_gifDecoder = 0;
            }
          }
          else {
            m_gifDecoder = 0;
          }
        }

        if (readImg)
#endif // wxUSE_GIF && wxUSE_TIMER
        {
          wxImage image;
          {
            wxLogNull nl;
            image.LoadFile(*s, wxBITMAP_TYPE_ANY);
          }
          if (image.Ok()) {
            SetImage(image);
          }
          else {
            if (mapname.IsEmpty()) {
              TBasicApp::NewLogEntry(logError) << "Invalid image";
            }
            else {
              TBasicApp::NewLogEntry(logError) << "Invalid image with map: " << mapname;
            }
          }
        }
      }
    }
  }
  else {  // input==NULL, use "broken image" bitmap
    if (m_bmpW == wxDefaultCoord && m_bmpH == wxDefaultCoord) {
      m_bmpW = 29;
      m_bmpH = 31;
    }
    else {
      m_showFrame = true;
      if (m_bmpW == wxDefaultCoord) {
        m_bmpW = 31;
      }
      if (m_bmpH == wxDefaultCoord) {
        m_bmpH = 33;
      }
    }
    m_bitmap = new wxBitmap(wxArtProvider::GetBitmap(wxART_MISSING_IMAGE));
  }
  //else: ignore the 0-sized images used sometimes on the Web pages

  m_Width = (int)(scale * (double)m_bmpW);
  m_Height = (int)(scale * (double)m_bmpH);
  WidthInPercent = width_per;
  HeightInPercent = height_per;
  switch (align) {
  case wxHTML_ALIGN_TOP:
    m_Descent = m_Height;
    break;
  case wxHTML_ALIGN_CENTER:
    m_Descent = m_Height / 2;
    break;
  case wxHTML_ALIGN_BOTTOM:
  default:
    m_Descent = 0;
    break;
  }
}
//..............................................................................
bool THtmlImageCell::OnFileChange() {
  olx_object_ptr<wxFSFile> fsFile = TFileHandlerManager::GetFSFileHandler(FSource);
  if (!fsFile.ok()) {
    return false;
  }
  wxImage image;
  {
    wxLogNull nl;
    image.LoadFile(*fsFile->GetStream(), wxBITMAP_TYPE_ANY);
  }
  if (!image.Ok()) {
    return false;
  }
  File = fsFile.release();
  SetImage(image);
  return true;
}
//..............................................................................
void THtmlImageCell::SetImage(const wxImage& img) {
  if (img.Ok()) {
    int ww = img.GetWidth();
    int hh = img.GetHeight();
    if (m_bmpW == wxDefaultCoord) {
      if (m_bmpH != wxDefaultCoord) {
        m_bmpW = ww*m_bmpH / hh;
      }
      else {
        m_bmpW = ww;
      }
    }
    if (m_bmpH == wxDefaultCoord) {
      if (m_bmpW != wxDefaultCoord) {
        m_bmpH = hh * m_bmpW / ww;
      }
      else {
        m_bmpH = hh;
      }
    }

    if ((m_bmpW != ww || m_bmpH != hh) && m_bmpW > 0 && m_bmpH > 0) {
      wxImage img2 = img.Scale(m_bmpW, m_bmpH, wxIMAGE_QUALITY_HIGH);
      m_bitmap = new wxBitmap(img2);
    }
    else {
      m_bitmap = new wxBitmap(img);
    }
  }
}
//..............................................................................
#if wxUSE_GIF && wxUSE_TIMER
void THtmlImageCell::AdvanceAnimation(wxTimer* timer) {
  wxImage img;
  m_nCurrFrame++;
  if (!m_gifDecoder.ok()) {
    return;
  }
  if (m_nCurrFrame == m_gifDecoder->GetFrameCount()) {
    m_nCurrFrame = 0;
  }

  if (m_physX == wxDefaultCoord) {
    m_physX = m_physY = 0;
    for (wxHtmlCell* cell = this; cell; cell = cell->GetParent()) {
      m_physX += cell->GetPosX();
      m_physY += cell->GetPosY();
    }
  }

  wxWindow* win = m_windowIface->GetHTMLWindow();
  wxPoint pos =
    m_windowIface->HTMLCoordsToWindow(this, wxPoint(m_physX, m_physY));
  wxRect rect(pos, wxSize(m_Width, m_Height));

  if (win->GetClientRect().Intersects(rect) &&
    m_gifDecoder->ConvertToImage(m_nCurrFrame, &img))
  {
#if !defined(__WXMSW__) || wxUSE_WXDIB
    if (m_gifDecoder->GetFrameSize(m_nCurrFrame) != wxSize(m_Width, m_Height) ||
      m_gifDecoder->GetFramePosition(m_nCurrFrame) != wxPoint(0, 0))
    {
      wxBitmap bmp(img);
      wxMemoryDC dc;
      dc.SelectObject(*m_bitmap);
      dc.DrawBitmap(bmp, m_gifDecoder->GetFramePosition(m_nCurrFrame),
        true /* use mask */);
    }
    else
#endif
      SetImage(img);
    win->Refresh(img.HasMask(), &rect);
  }

  long delay = m_gifDecoder->GetDelay(m_nCurrFrame);
  if (delay == 0) {
    delay = 1;
  }
  timer->Start(delay, true);
}
//..............................................................................
//..............................................................................
//..............................................................................
TGIFTimer::TGIFTimer(THtmlImageCell* cell)
: m_cell(cell)
{}
void TGIFTimer::Notify() {
  m_cell->AdvanceAnimation(this);
}
#endif
//..............................................................................
void THtmlImageCell::Layout(int w) {
  wxHtmlCell::Layout(w);
  m_physX = m_physY = wxDefaultCoord;
}
//..............................................................................
THtmlImageCell::~THtmlImageCell() {
}
//..............................................................................
void THtmlImageCell::Draw(wxDC& dc, int x, int y) {
  int width = m_bmpW, height = m_bmpH;
  if (WidthInPercent || HeightInPercent) {
    if (WidthInPercent) {
      width = GetParent()->GetWidth() * m_Width / 100;
    }
    if (HeightInPercent) {
      height = GetParent()->GetHeight() * m_Height / 100;
    }
  }
  if (m_showFrame) {
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawRectangle(x + m_PosX, y + m_PosY, (int)(width*m_scale), (int)(height*m_scale));
    x++, y++;
  }
  if (m_bitmap != 0) {
    // We add in the scaling from the desired bitmap width
    // and height, so we only do the scaling once.
    double imageScaleX = 1.0;
    double imageScaleY = 1.0;
    if (width != m_bitmap->GetWidth())
      imageScaleX = (double)width / (double)m_bitmap->GetWidth();
    if (height != m_bitmap->GetHeight())
      imageScaleY = (double)height / (double)m_bitmap->GetHeight();

    double us_x, us_y;
    dc.GetUserScale(&us_x, &us_y);
    dc.SetUserScale(us_x * m_scale * imageScaleX, us_y * m_scale * imageScaleY);
    int cx = (int)((double)(x + m_PosX) / (m_scale*imageScaleX)),
      cy = (int)((double)(y + m_PosY) / (m_scale*imageScaleY));
    //        dc.DrawBitmap(*m_bitmap, cx+1, cy, true);
    dc.DrawBitmap(*m_bitmap, cx, cy, true);
    dc.SetUserScale(us_x, us_y);
    if (!Text.IsEmpty()) {
      dc.SetTextForeground(*wxBLACK);
      dc.DrawText(Text, x + m_PosX, y + m_PosY);
    }
  }
}
//..............................................................................
wxHtmlLinkInfo *THtmlImageCell::GetLink(int x, int y) const {
  for (int i = Shapes.Count() - 1; i >= 0; i--) {
    if (Shapes[i].IsInside(x, y)) {
      return Shapes[i].link;
    }
  }
  wxHtmlContainerCell *op = GetParent(), *p = op;
  while (p != 0) {
    op = p;
    p = p->GetParent();
  }
  p = op;
  wxHtmlCell *cell = (wxHtmlCell*)p->Find(wxHTML_COND_ISIMAGEMAP, (const void*)(&m_mapName));
  return (cell == 0) ? wxHtmlCell::GetLink(x, y) : cell->GetLink(x, y);
}
//..............................................................................
//..............................................................................
//..............................................................................
void THtmlWidgetCell::SetHeight(int h) {
  m_Height = h;
}
//..............................................................................
#endif // OLEX2_HTML_IMAGE_PLATFORM_BRANCH
