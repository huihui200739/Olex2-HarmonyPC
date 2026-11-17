#include "native_picker.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
Size Natural(Size size,int width,int height){if(size.width<0)size.width=width;if(size.height<0)size.height=height;return size;}
bool Decimal(const char* first,const char* last,int& value){if(first==last)return false;for(auto p=first;p!=last;++p)if(*p<'0'||*p>'9')return false;auto result=std::from_chars(first,last,value);return result.ec==std::errc()&&result.ptr==last;}
int Hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';if(c>='A'&&c<='F')return 10+c-'A';return -1;}
}
bool CalendarDate::IsLeapYear(int year){return year%4==0&&(year%100!=0||year%400==0);}
int CalendarDate::DaysInMonth(int year,int month){static constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(year<1||year>9999||month<1||month>12)throw std::out_of_range("Calendar year/month is outside 1..9999 / 1..12");return days[month-1]+(month==2&&IsLeapYear(year));}
bool CalendarDate::IsValid()const{return year>=1&&year<=9999&&month>=1&&month<=12&&day>=1&&day<=DaysInMonth(year,month);}
int CalendarDate::Weekday()const{
  if(!IsValid())throw std::invalid_argument("An invalid calendar date has no weekday");int previous=year-1;int days=365*previous+previous/4-previous/100+previous/400;
  for(int m=1;m<month;++m)days+=DaysInMonth(year,m);return (days+day-1)%7;
}
CalendarDate CalendarDate::ShiftMonths(int count)const{
  if(!IsValid())throw std::invalid_argument("Cannot navigate an invalid calendar date");int64_t index=static_cast<int64_t>(year-1)*12+month-1+static_cast<int64_t>(count);index=std::clamp<int64_t>(index,0,9999*12-1);int y=static_cast<int>(index/12)+1,m=static_cast<int>(index%12)+1;return {y,m,std::min(day,DaysInMonth(y,m))};
}
CalendarDate CalendarDate::ShiftYears(int count)const{if(!IsValid())throw std::invalid_argument("Cannot navigate an invalid calendar date");int y=static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(year)+count,1,9999));return {y,month,std::min(day,DaysInMonth(y,month))};}
std::string CalendarDate::Iso()const{if(IsEmpty())return {};if(!IsValid())throw std::invalid_argument("Invalid Gregorian calendar date");char text[11]{};std::snprintf(text,sizeof(text),"%04d-%02d-%02d",year,month,day);return text;}
bool CalendarDate::TryParse(const std::string& text,CalendarDate& result){
  if(text.empty()){result={};return true;}if(text.size()!=10||text[4]!='-'||text[7]!='-')return false;CalendarDate candidate;
  if(!Decimal(text.data(),text.data()+4,candidate.year)||!Decimal(text.data()+5,text.data()+7,candidate.month)||!Decimal(text.data()+8,text.data()+10,candidate.day)||!candidate.IsValid())return false;result=candidate;return true;
}
bool CalendarDate::TryYear(const std::string& text,int& result){int candidate=0;if(text.size()>4||!Decimal(text.data(),text.data()+text.size(),candidate)||candidate<1||candidate>9999)return false;result=candidate;return true;}
CalendarDate CalendarDate::Today(){std::time_t now=std::time(nullptr);std::tm local{};if(!localtime_r(&now,&local))throw std::runtime_error("Local calendar time is unavailable");CalendarDate date{local.tm_year+1900,local.tm_mon+1,local.tm_mday};if(!date.IsValid())throw std::runtime_error("Local calendar time is outside supported civil years");return date;}
bool SameColor(Color a,Color b){return PackColor(a)==PackColor(b);}
uint32_t PackColor(Color c){return static_cast<uint32_t>(c.alpha)<<24|static_cast<uint32_t>(c.red)<<16|static_cast<uint32_t>(c.green)<<8|c.blue;}
Color UnpackColor(uint32_t color){return {static_cast<uint8_t>(color>>16),static_cast<uint8_t>(color>>8),static_cast<uint8_t>(color),static_cast<uint8_t>(color>>24)};}
std::string ColorHex(Color color){char text[10]{};std::snprintf(text,sizeof(text),"#%02X%02X%02X%02X",color.red,color.green,color.blue,color.alpha);return text;}
bool TryColorHex(const std::string& text,Color& result){
  if((text.size()!=7&&text.size()!=9)||text[0]!='#')return false;uint8_t bytes[4]{0,0,0,255};for(size_t i=0;i<(text.size()-1)/2;++i){int a=Hex(text[1+i*2]),b=Hex(text[2+i*2]);if(a<0||b<0)return false;bytes[i]=static_cast<uint8_t>(a*16+b);}result={bytes[0],bytes[1],bytes[2],bytes[3]};return true;
}
NativeDateControl::NativeDateControl(NodeBackend& backend,Widget* parent,int id,const CalendarDate& value,Point pos,Size size,long style)
 :Widget(backend,NodeKind::DateField,parent,id,pos,Natural(size,200,36)){
  SetWindowStyle(style);SetValue(value);SetNumber(Property::Focusable,1);Listen({EventKind::Click,EventKind::DateChange});
}
void NativeDateControl::SetWindowStyle(long style){if(style&~(PickerReadOnly|DateAllowNone))throw std::invalid_argument("Unsupported native calendar style");if(!value_.IsEmpty()||style&DateAllowNone||GetWindowStyle()==0){SetNumber(Property::PickerAllowEmpty,(style&DateAllowNone)?1:0);}else throw std::invalid_argument("A selected date is required before disabling empty dates");SetNumber(Property::PickerReadOnly,(style&PickerReadOnly)?1:0);RememberWindowStyle(style);}
void NativeDateControl::SetValue(const CalendarDate& value){
  AssertThread();if(!value.IsValid()&&!(value.IsEmpty()&&AllowsNone()))throw std::invalid_argument("A valid Gregorian date is required");auto text=value.Iso();auto old=value_;value_=value;writing_=true;
  try{Widget::SetText(TextProperty::Content,text);writing_=false;}catch(...){writing_=false;value_=old;throw;}
}
bool NativeDateControl::ApplyDateEvent(const NodeEvent& event){if(event.kind!=EventKind::DateChange||writing_||IsReadOnly())return false;CalendarDate next;if(!CalendarDate::TryParse(event.text,next)||(!next.IsValid()&&!AllowsNone()))throw std::invalid_argument("Native calendar emitted an invalid civil date");SetValue(next);return true;}
void NativeDateControl::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Click)OpenPopup();}
void NativeDateControl::OpenPopup(){AssertThread();if(!IsReadOnly()&&IsEventAllowed())Backend().OpenPicker(Handle());}
void NativeDateControl::ClosePopup(){AssertThread();Backend().ClosePicker(Handle());}
NativeColorControl::NativeColorControl(NodeBackend& backend,Widget* parent,int id,Color value,Point pos,Size size,long style)
 :Widget(backend,NodeKind::ColorField,parent,id,pos,Natural(size,200,36)){
  SetWindowStyle(style);SetValue(value);SetNumber(Property::Focusable,1);Listen({EventKind::Click,EventKind::ColorChange});
}
void NativeColorControl::SetWindowStyle(long style){if(style&~PickerReadOnly)throw std::invalid_argument("Unsupported native color-picker style");SetNumber(Property::PickerReadOnly,(style&PickerReadOnly)?1:0);RememberWindowStyle(style);}
void NativeColorControl::SetValue(Color value){AssertThread();auto old=value_;value_=value;writing_=true;try{Widget::SetText(TextProperty::Content,ColorHex(value));writing_=false;}catch(...){writing_=false;value_=old;throw;}}
bool NativeColorControl::ApplyColorEvent(const NodeEvent& event){if(event.kind!=EventKind::ColorChange||writing_||IsReadOnly())return false;if(!std::isfinite(event.value)||std::floor(event.value)!=event.value||event.value<0||event.value>std::numeric_limits<uint32_t>::max())throw std::invalid_argument("Native color selection is not an exact ARGB value");SetValue(UnpackColor(static_cast<uint32_t>(event.value)));return true;}
void NativeColorControl::OnNativeEvent(const NodeEvent& event){if(event.kind==EventKind::Click)OpenPopup();}
void NativeColorControl::OpenPopup(){AssertThread();if(!IsReadOnly()&&IsEventAllowed())Backend().OpenPicker(Handle());}
void NativeColorControl::ClosePopup(){AssertThread();Backend().ClosePicker(Handle());}
}
