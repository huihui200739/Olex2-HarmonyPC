/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_ctrl_trackbar_H
#define __olx_ctrl_trackbar_H
#include "olxctrlbase.h"

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
namespace ctrl_ext {
class TTrackBar:public olex2::harmony::ui::Widget,public AOlxCtrl {
  olxstr Data;
  int this_Val;
protected:
  void OnNativeEvent(const olex2::harmony::ui::NodeEvent&)override;
  void ScrollEvent(double);
  void MouseUpEvent();
public:
  TTrackBar(olex2::harmony::ui::NodeBackend&,olex2::harmony::ui::Widget* parent=nullptr,int id=-1,int value=0,int minimum=0,int maximum=100,olex2::harmony::ui::Point={},olex2::harmony::ui::Size={},long style=0);
  DefPropC(olxstr, Data)
  int GetValue()const;
  void SetValue(int);
  int GetMin()const;
  void SetMin(int);
  int GetMax()const;
  void SetMax(int);
  void SetRange(int,int);
  AOlxCtrl::ActionQueue& OnChange;
  AOlxCtrl::ActionQueue& OnMouseUp;
};
}
#else
namespace ctrl_ext  {

  class TTrackBar: public wxSlider, public AOlxCtrl {
  protected:
    void ScrollEvent(wxCommandEvent& event);
    void MouseUpEvent(wxMouseEvent& event);
    olxstr Data;
    int this_Val;  // needed to call events only if value has changed
  public:
    // on gtk, the size cannot be changed after the creation!
    TTrackBar(wxWindow *Parent, wxWindowID id = -1,
      int value = 0, int min_v = 0, int max_v = 100,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      long style = wxSL_HORIZONTAL | wxSL_AUTOTICKS);

    DefPropC(olxstr, Data)

    inline int GetValue() const {  return wxSlider::GetValue(); }
    inline void SetValue(int v) { wxSlider::SetValue(v); }
    inline int GetMin() const {  return wxSlider::GetMin(); }
    inline void SetMin(int v) { wxSlider::SetRange(this->GetMax(), v); }
    inline int GetMax() const {  return wxSlider::GetMax(); }
    inline void SetMax(int v) { wxSlider::SetRange(v, this->GetMin()); }

    AOlxCtrl::ActionQueue &OnChange, &OnMouseUp;
  };
}; // end namespace ctrl_ext
#endif // native widgets / original wx
#endif
