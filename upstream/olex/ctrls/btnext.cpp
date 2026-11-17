/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "btnext.h"
#include "original_button_mode.h"
#include "olx_control_state.h"
#include "fsext.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace ctrl_ext;
using namespace olex2::harmony::ui;
namespace {
std::string Utf8(const olxstr& value){auto text=TUtf8::Encode(value);return std::string(text.c_str(),text.Length());}
uint32_t Lightness(uint32_t color,int amount){amount=std::clamp(amount,0,200);uint32_t out=color&0xff000000u;for(unsigned shift:{0u,8u,16u}){int c=(color>>shift)&255;c=amount<=100?c*amount/100:c+(255-c)*(amount-100)/100;out|=uint32_t(c)<<shift;}return out;}
void Dimensions(int w,int h){if(w<=0||h<=0||w>32767||h>32767)throw std::invalid_argument("Native button dimensions must fit positive original short coordinates");}
}
AButtonBase::AButtonBase(Widget* window,Widget* parent):AOlxCtrl(window),window_(window),parent_(parent),OnClick(AOlxCtrl::ActionQueue::New(Actions,evt_on_click_id)),OnUp(AOlxCtrl::ActionQueue::New(Actions,evt_on_uncheck_id)),OnDown(AOlxCtrl::ActionQueue::New(Actions,evt_on_check_id)){SetToDelete(false);}
AButtonBase::~AButtonBase(){if(ActionQueue)ActionQueue->Remove(this);}
void AButtonBase::SetActionQueue(TActionQueue& queue,const olxstr& mode){if(ActionQueue)ActionQueue->Remove(this);ActionQueue=&queue;DependMode=mode;ActionQueue->Add(this);}
bool AButtonBase::Execute(const IOlxObject*,const IOlxObject* data,TActionQueue*){if(data&&data->Is<TModeChange>())SetDown(OriginalButtonModeActive(DependMode));return true;}
void AButtonBase::SetDown(bool value){if(Down==value)return;if(Down){Down=false;OnUp.Execute(static_cast<AOlxCtrl*>(this));}else{Down=true;OnDown.Execute(static_cast<AOlxCtrl*>(this));}}
void AButtonBase::_ClickEvent(){if(Down)OnUp.Execute(static_cast<AOlxCtrl*>(this));OnClick.Execute(static_cast<AOlxCtrl*>(this));if(!Down)OnDown.Execute(static_cast<AOlxCtrl*>(this));Down=!Down;}
void AButtonBase::SetHint(const olxstr& hint){window_->SetText(TextProperty::Accessibility,Utf8(hint));Hint=hint;}
TButton::TButton(NodeBackend& backend,Widget* parent,int id,const olxstr& label,Point pos,Size size,long style):Widget(backend,NodeKind::Button,parent,id,pos,size,style),AButtonBase(this,parent){SetNumber(Property::Focusable,1);SetNumber(Property::Background,baseBackground_);SetNumber(Property::FontColor,0xff202020u);SetCaption(label);Listen({EventKind::Click,EventKind::Hover});}
void TButton::SetCaption(const olxstr& caption){SetText(TextProperty::Content,Utf8(caption));}
olxstr TButton::GetCaption()const{auto text=GetText(TextProperty::Content);return TUtf8::Decode(text.c_str(),text.size());}
void TButton::RefreshVisual(){
 const auto actual=GetBackgroundColor();if(actual!=renderedBackground_)baseBackground_=actual;
 if(drawParams.IsEmpty()){if(customApplied_){SetNumber(Property::Background,baseBackground_);SetNumber(Property::BorderWidth,0);SetNumber(Property::ButtonRectangular,0);renderedBackground_=baseBackground_;customApplied_=false;}return;}
 const int border=drawParams.Find("border.width","1").ToInt(),highlight=drawParams.Find("highlight.lightness",CustomDraw_Highlight_Lightness).ToInt(),edge=drawParams.Find("border.lightness",CustomDraw_Border_Lightness).ToInt();const double radius=drawParams.Find("border.radius","0").ToDouble();
 SetNumber(Property::ButtonRectangular,1);SetNumber(Property::BorderWidth,border);SetNumber(Property::BorderColor,Lightness(baseBackground_,edge));SetNumber(Property::BorderRadius,radius);const auto fill=hovered_?Lightness(baseBackground_,highlight):baseBackground_;SetNumber(Property::Background,fill);renderedBackground_=fill;customApplied_=true;
}
void TButton::SetCustomDrawParams(const olxstr& params){olxstr_dict<olxstr> next;next=MapFromToks(params);const int border=next.Find("border.width","1").ToInt(),highlight=next.Find("highlight.lightness",CustomDraw_Highlight_Lightness).ToInt(),edge=next.Find("border.lightness",CustomDraw_Border_Lightness).ToInt();const double radius=next.Find("border.radius","0").ToDouble();if(border<0||border>64||highlight<0||highlight>200||edge<0||edge>200||!std::isfinite(radius)||radius<0||radius>32767)throw std::invalid_argument("Invalid native button draw parameters");auto previous=drawParams;drawParams=next;try{RefreshVisual();}catch(...){drawParams=previous;try{RefreshVisual();}catch(...){}throw;}}
void TButton::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Hover){hovered_=event.value!=0;RefreshVisual();}else if(event.kind==EventKind::Click){AButtonBase::_ClickEvent();SetNumber(Property::Focused,1);}}
void TButton::OnInteractionBlocked(const NodeEvent&){if(hovered_){hovered_=false;RefreshVisual();}}
TBmpButton::TBmpButton(NodeBackend& backend,Widget* parent,int id,const ImageRef& bitmap,Point pos,Size size,long style):Widget(backend,NodeKind::Button,parent,id,pos,size,style),AButtonBase(this,parent),image_(backend,NodeKind::Image,this),autoWidth_(size.width<0),autoHeight_(size.height<0){SetNumber(Property::Focusable,1);SetNumber(Property::ButtonRectangular,1);SetNumber(Property::Padding,0);image_.SetNumber(Property::ImageStretch,1);if(bitmap)SetBitmap(bitmap);Listen({EventKind::Click,EventKind::Hover});}
ImageRef TBmpButton::LoadBitmap(NodeBackend& backend,const olxstr& filename){if(filename.IsEmpty())throw std::invalid_argument("Button image source is empty");auto stream=TFileHandlerManager::GetInputStream(filename);if(!stream.ok())throw std::runtime_error("Button image resource was not found: "+Utf8(filename));const auto size=stream->GetAvailableSizeT();if(!size||size>64u*1024u*1024u)throw std::invalid_argument("Button image source size is unsupported");std::vector<uint8_t> bytes(size);stream->Read(bytes.data(),bytes.size());auto image=backend.DecodeImage(bytes);if(!image)throw std::runtime_error("Native button image decoding returned no image");Dimensions(image->width,image->height);return image;}
void TBmpButton::SetBitmap(const ImageRef& bitmap){if(!bitmap)throw std::invalid_argument("A bitmap button needs decoded pixels");Dimensions(bitmap->width,bitmap->height);const int oldW=Widget::GetWidth(),oldH=Widget::GetHeight(),oldImageW=image_.GetWidth(),oldImageH=image_.GetHeight();auto previous=bitmap_;try{image_.SetImage(bitmap);image_.SetWidth(bitmap->width);image_.SetHeight(bitmap->height);if(autoWidth_)SetWidth(bitmap->width);if(autoHeight_)SetHeight(bitmap->height);bitmap_=bitmap;}catch(...){try{image_.SetImage(previous);image_.SetWidth(oldImageW);image_.SetHeight(oldImageH);SetWidth(oldW);SetHeight(oldH);}catch(...){throw std::runtime_error("Bitmap update and previous visual restoration both failed");}throw;}}
void TBmpButton::SetSource(const olxstr& source){auto bitmap=LoadBitmap(Backend(),source);SetBitmap(bitmap);Source=source;}
void TBmpButton::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Click){AButtonBase::_ClickEvent();SetNumber(Property::Focused,1);}}
const short TImgButton::stUp;const short TImgButton::stDown;const short TImgButton::stDisabled;const short TImgButton::stHover;
TImgButton::TImgButton(NodeBackend& backend,Widget* parent):Widget(backend,NodeKind::Button,parent),AButtonBase(this,parent),image_(backend,NodeKind::Image,this){SetNumber(Property::Focusable,1);SetNumber(Property::ButtonRectangular,1);SetNumber(Property::Padding,0);image_.SetNumber(Property::ImageStretch,1);Listen({EventKind::Click,EventKind::Hover,EventKind::Pointer});}
const ImageRef& TImgButton::ChooseBitmap()const{if(!Widget::IsEnabled()||state==stDisabled)return images_[2]?images_[2]:images_[0];if(state==stDown)return images_[1]?images_[1]:images_[0];if(state==stHover)return images_[3]?images_[3]:images_[0];return images_[0];}
void TImgButton::Render(){auto bitmap=ChooseBitmap();if(bitmap)image_.SetImage(bitmap);}
void TImgButton::SetImages(const std::array<ImageRef,4>& images,int w,int h){
 if(!images[0])throw std::invalid_argument("Image buttons require a real up image; other states may fall back to it");if(w==-1)w=images[0]->width;if(h==-1)h=images[0]->height;Dimensions(w,h);const auto previous=images_;const short oldW=width,oldH=height;const int oldNodeW=Widget::GetWidth(),oldNodeH=Widget::GetHeight(),oldImageW=image_.GetWidth(),oldImageH=image_.GetHeight();
 try{image_.SetImage((!Widget::IsEnabled()||state==stDisabled)?(images[2]?images[2]:images[0]):state==stDown?(images[1]?images[1]:images[0]):state==stHover?(images[3]?images[3]:images[0]):images[0]);SetWidth(w);SetHeight(h);image_.SetWidth(w);image_.SetHeight(h);images_=images;width=static_cast<short>(w);height=static_cast<short>(h);}
 catch(...){images_=previous;width=oldW;height=oldH;try{image_.SetImage(ChooseBitmap());SetWidth(oldNodeW);SetHeight(oldNodeH);image_.SetWidth(oldImageW);image_.SetHeight(oldImageH);}catch(...){throw std::runtime_error("Image-set update and previous visual restoration both failed");}throw;}
}
void TImgButton::SetImages(const olxstr& source,int w,int h){std::array<ImageRef,4> next{};TStrList tokens(source,',');for(size_t i=0;i<tokens.Count();++i){const auto eq=tokens[i].IndexOf('=');if(eq==InvalidIndex)continue;auto key=tokens[i].SubStringTo(eq);int slot=key.Equalsi("up")?0:key.Equalsi("down")?1:key.Equalsi("disabled")?2:key.Equalsi("hover")?3:-1;if(slot<0)continue;next[slot]=TBmpButton::LoadBitmap(Backend(),tokens[i].SubStringFrom(eq+1));}SetImages(next,w,h);Source=source;}
void TImgButton::SetDown(bool value){if(IsDown()==value)return;auto previous=images_;std::swap(images_[0],images_[1]);if(!images_[0])images_[0]=images_[1];if(!images_[1])images_[1]=images_[0];try{Render();}catch(...){images_=previous;throw;}_SetDown(value);}
void TImgButton::SetEnabled(bool value){const auto oldState=state;const bool enabled=Widget::IsEnabled();try{Enable(value);state=value?stUp:stDisabled;Render();pressed_=false;waitingClick_=false;pointerId_=-1;}catch(...){state=oldState;try{Enable(enabled);Render();}catch(...){}throw;}}
void TImgButton::CancelPress(bool notify){const bool wasPressed=pressed_;pressed_=false;waitingClick_=false;pointerId_=-1;state=Widget::IsEnabled()?(MouseIn?stHover:stUp):stDisabled;Render();if(wasPressed&&notify)OnUp.Execute(this);}
void TImgButton::Pointer(const PointerEvent& pointer){
 if(pointer.phase==PointerPhase::Cancel){MouseIn=false;CancelPress(true);return;}
 if(pointer.phase==PointerPhase::Down){if(pressed_||!pointer.inside)return;MouseIn=true;pressed_=true;waitingClick_=false;pointerId_=pointer.id;state=stDown;Render();ProcessingOnDown=true;try{OnDown.Execute(this);ProcessingOnDown=false;}catch(...){ProcessingOnDown=false;CancelPress(false);throw;}if(!IsEventAllowed()){CancelPress(false);return;}if(!MouseIn){CancelPress(true);return;}SetNumber(Property::Focused,1);return;}
 if(!pressed_||pointer.id!=pointerId_)return;MouseIn=pointer.inside;
 if(pointer.phase==PointerPhase::Move){if(!MouseIn)CancelPress(true);return;}
 if(pointer.phase==PointerPhase::Up){pressed_=false;pointerId_=-1;state=MouseIn?stHover:stUp;waitingClick_=MouseIn;Render();OnUp.Execute(this);}
}
void TImgButton::Activate(){if(!waitingClick_){OnDown.Execute(this);if(!IsEventAllowed())return;OnUp.Execute(this);if(!IsEventAllowed())return;}waitingClick_=false;OnClick.Execute(this);SetNumber(Property::Focused,1);}
void TImgButton::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Pointer){if(event.pointer)Pointer(*event.pointer);}else if(event.kind==EventKind::Hover){MouseIn=event.value!=0;if(!MouseIn&&pressed_){if(!ProcessingOnDown)CancelPress(true);}else if(!pressed_){state=MouseIn?stHover:stUp;Render();}}else if(event.kind==EventKind::Click)Activate();}
void TImgButton::OnInteractionBlocked(const NodeEvent&){MouseIn=false;CancelPress(false);}
#else
#include "btnext.h"
#include "frameext.h"
#include "olxstate.h"
#include "fsext.h"
#include "olxvar.h"

using namespace ctrl_ext;
//..............................................................................
void AButtonBase::SetActionQueue(TActionQueue& q, const olxstr& dependMode) {
  ActionQueue = &q;
  DependMode = dependMode;
  ActionQueue->Add(this);
}
//..............................................................................
bool AButtonBase::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  if (Data && Data->Is<TModeChange>()) {
    const TModeChange* mc = (const TModeChange*)Data;
    SetDown(TModeRegistry::CheckMode(DependMode));
  }
  return true;
}
//..............................................................................
void AButtonBase::SetDown(bool v) {
  if (Down == v) {
    return;
  }
  if (Down) {
    Down = false;
    OnUp.Execute((AOlxCtrl*)this);
  }
  else {
    Down = true;
    OnDown.Execute((AOlxCtrl*)this);
  }
}
//..............................................................................
void AButtonBase::_ClickEvent() {
  if (Down) {
    OnUp.Execute((AOlxCtrl*)this);
  }
  OnClick.Execute((AOlxCtrl*)this);
  if (!Down) {
    OnDown.Execute((AOlxCtrl*)this);
  }
  Down = !Down;
}
//..............................................................................
//..............................................................................
//..............................................................................
TButton::TButton(wxWindow* parent, wxWindowID id, const wxString& label,
  const wxPoint& pos, const wxSize& size, long style)
: wxButton(parent, id, label, pos, size, style),
  AButtonBase(this)
{
  Bind(wxEVT_BUTTON, &TButton::ClickEvent, this);
  Bind(wxEVT_ENTER_WINDOW, &TButton::MouseEnterEvent, this);
  Bind(wxEVT_LEAVE_WINDOW, &TButton::MouseLeaveEvent, this);
  Bind(wxEVT_PAINT, &TButton::PaintEvent, this);
}
//..............................................................................
void TButton::ClickEvent(wxCommandEvent&) {
  AButtonBase::_ClickEvent();
  SetFocus();
}
//..............................................................................
void TButton::MouseEnterEvent(wxMouseEvent& event) {
  SetCursor(wxCursor(wxCURSOR_HAND));
  if (!GetHint().IsEmpty()) {
    SetToolTip(GetHint().u_str());
  }
  event.Skip();
}
//..............................................................................
void TButton::MouseLeaveEvent(wxMouseEvent& event) {
  event.Skip();
}
//..............................................................................
void TButton::PaintEvent(wxPaintEvent& evt) {
  if (IsBeingDeleted() || drawParams.IsEmpty()) {
    evt.Skip();
#ifdef __MAC__
    wxPaintDC dc(this);
    dc.SetBrush(wxBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
    dc.SetTextForeground(GetForegroundColour());
    wxSize sz = dc.GetTextExtent(GetLabel());
    dc.DrawText(GetLabel(), (WI.GetWidth() - sz.GetWidth()) / 2,
      (WI.GetHeight() - sz.GetHeight()) / 2);
    evt.Skip(false);
#endif
    return;
  }
  wxPaintDC dc(this);
  wxColor bg = GetBackgroundColour();
  int border_width = drawParams.Find("border.width", "1").ToInt();
  bool in_wnd = GetScreenRect().Contains(wxGetMousePosition());
  if (in_wnd) {
    int alpha1 = drawParams.Find("highlight.lightness",
      CustomDraw_Highlight_Lightness).ToInt();
    bg = GetBackgroundColour().ChangeLightness(alpha1);
  }
  dc.SetBrush(wxBrush(bg, wxBRUSHSTYLE_SOLID));
  if (border_width > 0) {
    int alpha = drawParams.Find("border.lightness",
      CustomDraw_Border_Lightness).ToInt();
    double border_r = drawParams.Find("border.radius", "0").ToDouble();
    dc.SetPen(wxPen(GetBackgroundColour().ChangeLightness(alpha), 1, wxPENSTYLE_SOLID));
    if (border_r > 0) {
      dc.DrawRoundedRectangle(0, 0, WI.GetWidth(), WI.GetHeight(), border_r);
    }
    else {
      dc.DrawRectangle(0, 0, WI.GetWidth(), WI.GetHeight());
    }
  }
  wxSize sz = dc.GetTextExtent(GetLabel());
  dc.DrawText(GetLabel(), (WI.GetWidth() - sz.GetWidth()) / 2,
    (WI.GetHeight()-sz.GetHeight())/2);
  evt.Skip(false);
}
//..............................................................................
//..............................................................................
//..............................................................................
TBmpButton::TBmpButton(wxWindow* parent, wxWindowID id, const wxBitmap& bitmap,
const wxPoint& pos, const wxSize& size, long style)
: wxBitmapButton(parent, id, bitmap, pos, size, style),
AButtonBase(this)
{
  Bind(wxEVT_BUTTON, &TBmpButton::ClickEvent, this);
  Bind(wxEVT_ENTER_WINDOW, &TBmpButton::MouseEnterEvent, this);
  Bind(wxEVT_LEAVE_WINDOW, &TBmpButton::MouseLeaveEvent, this);
}
//..............................................................................
void TBmpButton::MouseEnterEvent(wxMouseEvent& event)  {
  SetCursor(wxCursor(wxCURSOR_HAND));
  if (!GetHint().IsEmpty()) {
    SetToolTip(GetHint().u_str());
  }
  event.Skip();
}
//..............................................................................
void TBmpButton::MouseLeaveEvent(wxMouseEvent& event) {
  event.Skip();
}
//..............................................................................
//..............................................................................
//..............................................................................
#ifdef __GNUC__
const short TImgButton::stUp;
const short TImgButton::stDown;
const short TImgButton::stDisabled;
const short TImgButton::stHover;
#endif
//..............................................................................
TImgButton::TImgButton(wxWindow* parent)
  : wxPanel(parent), AButtonBase(this)
{
  Bind(wxEVT_LEFT_DOWN, &TImgButton::MouseDownEvent, this);
  Bind(wxEVT_LEFT_UP, &TImgButton::MouseUpEvent, this);
  Bind(wxEVT_MOTION, &TImgButton::MouseMoveEvent, this);
  Bind(wxEVT_ENTER_WINDOW, &TImgButton::MouseEnterEvent, this);
  Bind(wxEVT_LEAVE_WINDOW, &TImgButton::MouseLeaveEvent, this);
  Bind(wxEVT_PAINT, &TImgButton::PaintEvent, this);
  Bind(wxEVT_ERASE_BACKGROUND, &TImgButton::EraseBGEvent, this);
  state = 0;
  width = height = -1;
  ProcessingOnDown = MouseIn = false;
}
//..............................................................................
void TImgButton::MouseEnterEvent(wxMouseEvent& event) {
  SetCursor(wxCursor(wxCURSOR_HAND));
  if (!GetHint().IsEmpty()) {
    SetToolTip(GetHint().u_str());
  }
  MouseIn = true;
  if (state != stDisabled && state != stDown) {
    state = stHover;
    Paint();
  }
  event.Skip();
}
//..............................................................................
void TImgButton::MouseLeaveEvent(wxMouseEvent& event) {
  MouseIn = false;
  if (state != stDisabled && state != stUp && !ProcessingOnDown) {
    state = stUp;
    Paint();
  }
  event.Skip();
}
//..............................................................................
const wxBitmap& TImgButton::ChooseBitmap() const {
  switch (state) {
  case stUp:        return bmpUp;
  case stDown:      return bmpDown;
  case stDisabled:  return bmpDisabled;
  case stHover:     return (bmpHover.IsOk() ? bmpHover : bmpUp);
  default:          return bmpUp;
  }
}
//..............................................................................
void TImgButton::Render(wxDC& dc) const {
  const wxBitmap& bmp = ChooseBitmap();
  if (bmp.IsOk()) {
#ifdef __MAC__
    dc.DrawBitmap(bmp, 0, 0, true);
#else
    wxBitmap tbmp(bmp.GetWidth(), bmp.GetHeight(), bmp.GetDepth());
    wxMemoryDC mdc(tbmp);
    mdc.SetBackground(wxBrush(GetBackgroundColour()));
    mdc.Clear();
    mdc.DrawBitmap(bmp, 0, 0, true);
    dc.Blit(0, 0, bmp.GetWidth(), bmp.GetHeight(), &mdc, 0, 0, wxCOPY);
#endif
  }
}
//..............................................................................
void TImgButton::MouseDownEvent(wxMouseEvent& event) {
  if (state == stDisabled || state == stDown) {
    return;
  }
  state = stDown;
  Paint();
  ProcessingOnDown = true;
  OnDown.Execute(this);
  if (!MouseIn) {
    OnUp.Execute((AOlxCtrl*)this);
    state = stUp;
    Paint();
  }
  SetFocus();
  ProcessingOnDown = false;
}
//..............................................................................
void TImgButton::MouseUpEvent(wxMouseEvent& event) {
  if (state == stDisabled || state == stUp || state == stHover) {
    return;
  }
  OnUp.Execute(this);
  OnClick.Execute(this);
  state = stUp;
  Paint();
}
//..............................................................................
void TImgButton::MouseMoveEvent(wxMouseEvent& event) {
  if (state != stDisabled && state != stDown && state != stHover) {
    state = stHover;
    Paint();
  }
}
//..............................................................................
wxBitmap TImgButton::BmpFromImage(const wxImage& img, int w, int h) const {
  if (img.GetWidth() != w || img.GetHeight() != h) {
    return wxBitmap(img.Scale(w, h));
  }
  return wxBitmap(img);
}
//..............................................................................
void TImgButton::SetImages(const olxstr& src, int w, int h) {
  Source = src;
  width = w;
  height = h;
  const TStrList toks(src, ',');
  TTypeList<wxImage> images(4, false);
  short imgState = 0;
  for (size_t i = 0; i < toks.Count(); i++)  {
    const size_t ei = toks[i].IndexOf('=');
    if (ei == InvalidIndex) {
      continue;
    }
    const olxstr dest = toks[i].SubStringTo(ei),
      fn = toks[i].SubStringFrom(ei + 1);
    olx_object_ptr<wxFSFile> fsFile = TFileHandlerManager::GetFSFileHandler(fn);
    if (fsFile == 0) {
      TBasicApp::NewLogEntry(logError) << __OlxSrcInfo <<
        ": could not locate image '" << fn << '\'';
      continue;
    }
    wxImage* img;
    {
      wxLogNull bl;
      img = new wxImage(*(fsFile->GetStream()), wxBITMAP_TYPE_ANY);
      if (!img->IsOk()) {
        TBasicApp::NewLogEntry(logError) << "Failed to load image: " <<
          fn;
        delete img;
        continue;
      }
    }
    if (dest.Equalsi("up")) {
      imgState |= TImgButton::stUp;
      images.Set(0, img);
    }
    else if (dest.Equalsi("down")) {
      imgState |= TImgButton::stDown;
      images.Set(1, img);
    }
    else if (dest.Equalsi("disabled")) {
      imgState |= TImgButton::stDisabled;
      images.Set(2, img);
    }
    else if (dest.Equalsi("hover")) {
      imgState |= TImgButton::stHover;
      images.Set(3, img);
    }
    else {
      delete img;
    }
  }
  images.Pack();
  SetImages(images, imgState, w, h);
}
//..............................................................................
void TImgButton::SetImages(const TTypeList<wxImage>& images, short imgState,
  int w, int h)
{
  if (images.IsEmpty()) {
    return;
  }
  if (w == -1) {
    w = images[0].GetWidth();
  }
  if (h == -1) {
    h = images[0].GetHeight();
  }
  wxPanel::SetSize(w, h);
  size_t img_index = 0;
  if ((imgState & stUp) != 0) {
    bmpUp = BmpFromImage(images[img_index++], w, h);
  }
  if (img_index < images.Count() && (imgState & stDown) != 0) {
    bmpDown = BmpFromImage(images[img_index++], w, h);
  }
  if (img_index < images.Count() && (imgState & stHover) != 0) {
    bmpHover = BmpFromImage(images[img_index++], w, h);
  }
  if (img_index < images.Count() && (imgState & stDisabled) != 0) {
    bmpDisabled = BmpFromImage(images[img_index++], w, h);
  }
}
//..............................................................................
void TImgButton::SetDown(bool v) {
  if (IsDown() != v) {
    olx_swap(bmpUp, bmpDown);
  }
  _SetDown(v);
}
//..............................................................................

#endif
