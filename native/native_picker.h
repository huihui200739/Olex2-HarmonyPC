#pragma once
#include "native_widget.h"
namespace olex2::harmony::ui {
constexpr long PickerReadOnly=1,DateAllowNone=2;
// Proleptic Gregorian civil dates, not timestamps. Empty is exactly {0,0,0}.
// The custom native calendar supports years 1..9999 without a platform wheel's
// default start/end truncation; local time is used only for the Today action.
struct CalendarDate {
  int year=0,month=0,day=0;
  bool IsEmpty()const{return year==0&&month==0&&day==0;}
  bool IsValid()const;
  static bool IsLeapYear(int);
  static int DaysInMonth(int,int);
  int Weekday()const; // Monday=0, Sunday=6; 0001-01-01 was Monday.
  CalendarDate ShiftMonths(int)const;
  CalendarDate ShiftYears(int)const;
  std::string Iso()const;
  static bool TryParse(const std::string&,CalendarDate&);
  static bool TryYear(const std::string&,int&);
  static CalendarDate Today();
};
inline bool operator==(const CalendarDate& a,const CalendarDate& b){return a.year==b.year&&a.month==b.month&&a.day==b.day;}
inline bool operator!=(const CalendarDate& a,const CalendarDate& b){return !(a==b);}
bool SameColor(Color,Color);
uint32_t PackColor(Color);
Color UnpackColor(uint32_t);
std::string ColorHex(Color); // #RRGGBBAA
bool TryColorHex(const std::string&,Color&); // #RRGGBB or #RRGGBBAA
class NativeDateControl:public Widget {
  CalendarDate value_;
  bool writing_=false;
protected:
  void OnNativeEvent(const NodeEvent&)override;
  bool ApplyDateEvent(const NodeEvent&);
public:
  NativeDateControl(NodeBackend&,Widget*,int,const CalendarDate&,Point={},Size={},long=0);
  const CalendarDate& GetValue()const{return value_;}
  void SetValue(const CalendarDate&);
  bool IsReadOnly()const{return GetNumber(Property::PickerReadOnly)!=0;}
  bool AllowsNone()const{return GetNumber(Property::PickerAllowEmpty)!=0;}
  void SetReadOnly(bool value){SetWindowStyle(value?GetWindowStyle()|PickerReadOnly:GetWindowStyle()&~PickerReadOnly);}
  void SetWindowStyle(long)override;
  void OpenPopup();void ClosePopup();
};
class NativeColorControl:public Widget {
  Color value_;
  bool writing_=false;
protected:
  void OnNativeEvent(const NodeEvent&)override;
  bool ApplyColorEvent(const NodeEvent&);
public:
  NativeColorControl(NodeBackend&,Widget*,int,Color,Point={},Size={},long=0);
  Color GetValue()const{return value_;}
  void SetValue(Color);
  bool IsReadOnly()const{return GetNumber(Property::PickerReadOnly)!=0;}
  void SetReadOnly(bool value){SetWindowStyle(value?PickerReadOnly:0);}
  void SetWindowStyle(long)override;
  void OpenPopup();void ClosePopup();
};
}
