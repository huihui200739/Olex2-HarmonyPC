#include "native_editable.h"
#include "native_text_positions.h"
#include "eutf8.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
std::string Encode(const olxstr& value){auto bytes=TUtf8::Encode(value);std::string text(bytes.c_str(),bytes.Length());if(text.find('\0')!=std::string::npos)throw std::invalid_argument("Native text cannot contain an embedded NUL");return text;}
Size Natural(Size value,int width,int height){if(value.width<0)value.width=width;if(value.height<0)value.height=height;return value;}
}
NativeTextControl::NativeTextControl(NodeBackend& backend,Widget* parent,int id,const olxstr& value,Point pos,Size size,long style,std::function<void(const NodeEvent&)> receiver)
 :Widget(backend,(style&InputMultiline)?NodeKind::TextArea:NodeKind::TextInput,parent,id,pos,Natural(size,200,(style&InputMultiline)?90:32)),multiline_((style&InputMultiline)!=0),receiver_(std::move(receiver)){
  SetWindowStyle(style);SetValue(value);SetNumber(Property::Focusable,1);
  Listen({EventKind::TextChange,EventKind::Submit,EventKind::Focus,EventKind::Blur,EventKind::Key,EventKind::DoubleClick});
}
long NativeTextControl::GetInsertionPoint()const{AssertThread();const auto caret=Backend().GetTextCaret(Handle());if(caret<0)throw std::runtime_error("Native text caret is negative");return static_cast<long>(TextStringOffset(value_,std::min(static_cast<std::size_t>(caret),TextUtf16Offset(value_,value_.Length()))));}
void NativeTextControl::GetSelection(long* first,long* last)const{
  AssertThread();auto range=Backend().GetTextSelection(Handle());
  if(range.start<0||range.end<0)throw std::runtime_error("Native text selection is negative");
  const auto maximum=TextUtf16Offset(value_,value_.Length());
  if(first)*first=static_cast<long>(TextStringOffset(value_,std::min(static_cast<std::size_t>(range.start),maximum)));
  if(last)*last=static_cast<long>(TextStringOffset(value_,std::min(static_cast<std::size_t>(range.end),maximum)));
}
void NativeTextControl::SetSelection(long first,long last){
  AssertThread();if(first==-1&&last==-1){first=0;last=static_cast<long>(value_.Length());}
  if(first<0||last<0)throw std::out_of_range("Negative native text selection");
  auto a=TextUtf16Offset(value_,static_cast<std::size_t>(first)),b=TextUtf16Offset(value_,static_cast<std::size_t>(last));
  if(a>b)std::swap(a,b);Backend().SetTextSelection(Handle(),TextSelection{static_cast<int32_t>(a),static_cast<int32_t>(b)});
}
void NativeTextControl::SetInsertionPoint(long point){if(point==-1){SetInsertionPointEnd();return;}SetSelection(point,point);}
void NativeTextControl::SetInsertionPointEnd(){SetInsertionPoint(static_cast<long>(value_.Length()));}
void NativeTextControl::SetWindowStyle(long value){
  if(value&~(InputReadOnly|InputMultiline|InputAlignRight))throw std::invalid_argument("Unsupported native text-input style");
  if(bool(value&InputMultiline)!=multiline_)throw std::invalid_argument("Changing single-line/multiline node kind requires recreating the original text control");
  SetNumber(Property::Editable,(value&InputReadOnly)?0:1);SetNumber(Property::TextAlign,(value&InputAlignRight)?1:0);RememberWindowStyle(value);
}
void NativeTextControl::SetValue(const olxstr& value){
  AssertThread();TextUtf16Offset(value,value.Length());auto text=Encode(value);auto old=value_;value_=value;writing_=true;
  try{Widget::SetText(TextProperty::Content,text);writing_=false;}catch(...){writing_=false;value_=old;throw;}
}
bool NativeTextControl::ApplyTextEvent(const NodeEvent& event){
  AssertThread();if(event.kind!=EventKind::TextChange||writing_||IsReadOnly())return false;
  if(event.text.find('\0')!=std::string::npos)throw std::invalid_argument("Native input event contains an embedded NUL");
  auto next=TUtf8::Decode(event.text.data(),event.text.size());if(next==value_)return false;value_=next;return true;
}
NativeComboList::NativeComboList(NodeBackend& backend,Widget* parent,int id,const olxstr& value,Point pos,Size size,long style)
 :NativeChoiceList(backend,parent,id,pos,Natural(size,260,32),0,NodeKind::Row),
  entry_(backend,this,-1,value,{},Size{std::max(1,GetWidth()-34),GetHeight()},style,[this](const NodeEvent& e){Forward(e);}),
  popup_(backend,NodeKind::Choice,this,{EventKind::Click,EventKind::ChoiceChange,EventKind::Focus,EventKind::Blur},[this](const NodeEvent& e){Forward(e);}){
  ConfigureOptionsNode(popup_.Handle());popup_.SetWidth(32);popup_.SetHeight(GetHeight());popup_.SetNumber(Property::Focusable,0);Backend().SetNumber(popup_.Handle(),Property::CompactOptions,1);RememberWindowStyle(style);
}
void NativeComboList::SetWindowStyle(long value){
  if(value&~(InputReadOnly|InputAlignRight))throw std::invalid_argument("Unsupported native combo style");
  entry_.SetWindowStyle(value);RememberWindowStyle(value);
}
void NativeComboList::Forward(const NodeEvent& event){
  if(event.kind==EventKind::Click){Backend().OpenOptions(popup_.Handle());return;}
  OnNativeEvent(event);
}
void NativeComboList::SetSelection(int value){
  NativeChoiceList::SetSelection(value);entry_.SetValue(value>=0?GetString(static_cast<size_t>(value)):EmptyString());
}
void NativeComboList::Clear(){entry_.SetValue(EmptyString());NativeChoiceList::Clear();}
bool NativeComboList::ApplyTextEvent(const NodeEvent& event){if(!entry_.ApplyTextEvent(event))return false;NativeChoiceList::SetSelection(-1);return true;}
bool NativeComboList::ApplySelectionEvent(double value){if(!ApplyUserSelection(value))return false;SetSelection(GetSelection());return true;}
std::string NativeComboList::LastEventError()const{if(!Widget::LastEventError().empty())return Widget::LastEventError();if(!entry_.LastEventError().empty())return entry_.LastEventError();return popup_.LastEventError();}
bool NativeSpinControl::Parse(const olxstr& value,int& result){
  auto text=Encode(value);auto first=text.find_first_not_of(" \t\r\n"),last=text.find_last_not_of(" \t\r\n");if(first==std::string::npos)return false;
  const char* start=text.data()+first;const char* end=text.data()+last+1;if(start!=end&&*start=='+'){++start;if(start==end||*start=='-'||*start=='+')return false;}if(start==end)return false;
  int64_t number=0;auto parsed=std::from_chars(start,end,number,10);if(parsed.ec!=std::errc()||parsed.ptr!=end||number<std::numeric_limits<int>::min()||number>std::numeric_limits<int>::max())return false;
  result=static_cast<int>(number);return true;
}
NativeSpinControl::NativeSpinControl(NodeBackend& backend,Widget* parent,int id,const olxstr& value,Point pos,Size size,long style)
 :Widget(backend,NodeKind::Row,parent,id,pos,Natural(size,160,32)),
  entry_(backend,this,-1,value,{},Size{std::max(1,GetWidth()-64),GetHeight()},style&InputAlignRight,[this](const NodeEvent& e){OnNativeEvent(e);}),
  decrease_(backend,NodeKind::Button,this,{EventKind::Click},[this](const NodeEvent&){OnNativeEvent(NodeEvent{EventKind::SpinStep,-1});}),
  increase_(backend,NodeKind::Button,this,{EventKind::Click},[this](const NodeEvent&){OnNativeEvent(NodeEvent{EventKind::SpinStep,1});}){
  decrease_.SetText(TextProperty::Content,"−");increase_.SetText(TextProperty::Content,"+");
  decrease_.SetText(TextProperty::Accessibility,"减一");increase_.SetText(TextProperty::Accessibility,"加一");
  for(auto* button:{&decrease_,&increase_}){button->SetWidth(30);button->SetHeight(GetHeight());button->SetNumber(Property::Focusable,0);}
  int initial=0;if(!value.IsEmpty()&&!Parse(value,initial))throw std::invalid_argument("Initial native spin value is not a decimal integer");
  SetValue(initial);SetWindowStyle(style);
}
void NativeSpinControl::SetWindowStyle(long value){if(value&~(SpinArrowKeys|InputAlignRight))throw std::invalid_argument("Unsupported native spin style");entry_.SetWindowStyle(value&InputAlignRight);RememberWindowStyle(value);}
void NativeSpinControl::SetValue(int value){AssertThread();int next=std::clamp(value,minimum_,maximum_);entry_.SetValue(olxstr(next));current_=next;}
void NativeSpinControl::SetRange(int minimum,int maximum){if(minimum>maximum)throw std::invalid_argument("Spin minimum exceeds maximum");int next=std::clamp(current_,minimum,maximum);entry_.SetValue(olxstr(next));minimum_=minimum;maximum_=maximum;current_=next;}
bool NativeSpinControl::ApplyTextEvent(const NodeEvent& event){if(!entry_.ApplyTextEvent(event))return false;int next;if(!Parse(entry_.GetValue(),next))return false;current_=std::clamp(next,minimum_,maximum_);return true;}
void NativeSpinControl::CommitText(){int next;if(Parse(entry_.GetValue(),next))current_=std::clamp(next,minimum_,maximum_);entry_.SetValue(olxstr(current_));}
void NativeSpinControl::Step(int direction){int64_t next=int64_t(current_)+(direction>0?1:-1);SetValue(static_cast<int>(std::clamp(next,int64_t(minimum_),int64_t(maximum_))));}
std::string NativeSpinControl::LastEventError()const{if(!Widget::LastEventError().empty())return Widget::LastEventError();if(!entry_.LastEventError().empty())return entry_.LastEventError();if(!decrease_.LastEventError().empty())return decrease_.LastEventError();return increase_.LastEventError();}
}
