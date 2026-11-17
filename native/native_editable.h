#pragma once
#include "native_choice.h"
#include <functional>
namespace olex2::harmony::ui {
// Platform-neutral flags, not wx style constants. Actual input nodes implement
// readonly through insert/delete vetoes; hardware keys are bound on API14+.
constexpr long InputReadOnly=1, InputMultiline=2, InputAlignRight=4, SpinArrowKeys=8;
class NativeTextControl:public Widget {
  olxstr value_;
  bool multiline_=false,writing_=false;
  std::function<void(const NodeEvent&)> receiver_;
protected:
  void OnNativeEvent(const NodeEvent& event)override{if(receiver_)receiver_(event);}
public:
  NativeTextControl(NodeBackend&,Widget*,int id,const olxstr&,Point={},Size={},long style=0,std::function<void(const NodeEvent&)> receiver={});
  const olxstr& GetValue()const{return value_;}
  void SetValue(const olxstr&);
  bool ApplyTextEvent(const NodeEvent&);
  long GetInsertionPoint()const;
  void SetInsertionPoint(long);
  void SetInsertionPointEnd();
  void GetSelection(long*,long*)const;
  void SetSelection(long,long);
  bool IsReadOnly()const{return GetNumber(Property::Editable)==0;}
  bool IsMultiLine()const{return multiline_;}
  void SetReadOnly(bool value){SetWindowStyle(value?GetWindowStyle()|InputReadOnly:GetWindowStyle()&~InputReadOnly);}
  void SetWindowStyle(long)override;
};
class NativeComboList:public NativeChoiceList {
  NativeTextControl entry_;
  EventWidget popup_;
  void Forward(const NodeEvent&);
public:
  NativeComboList(NodeBackend&,Widget*,int id,const olxstr&,Point={},Size={},long style=0);
  const olxstr& GetValue()const{return entry_.GetValue();}
  void SetValue(const olxstr& value){entry_.SetValue(value);}
  void SetSelection(int);
  void Clear();
  bool ApplyTextEvent(const NodeEvent&);
  bool ApplySelectionEvent(double);
  bool IsReadOnly()const{return entry_.IsReadOnly();}
  void SetReadOnly(bool value){SetWindowStyle(value?GetWindowStyle()|InputReadOnly:GetWindowStyle()&~InputReadOnly);}
  void SetWindowStyle(long)override;
  std::string LastEventError()const;
  NodeHandle EntryHandle()const{return entry_.Handle();}
  NodeHandle PopupHandle()const{return popup_.Handle();}
};
class NativeSpinControl:public Widget {
  int current_=0,minimum_=0,maximum_=100;
  NativeTextControl entry_;
  EventWidget decrease_,increase_;
  static bool Parse(const olxstr&,int&);
protected:
  bool ApplyTextEvent(const NodeEvent&);
  void CommitText();
  void Step(int direction);
public:
  NativeSpinControl(NodeBackend&,Widget*,int id,const olxstr&,Point={},Size={},long style=SpinArrowKeys|InputAlignRight);
  int GetValue()const{return current_;}
  int GetMin()const{return minimum_;} int GetMax()const{return maximum_;}
  void SetValue(int);
  void SetRange(int minimum,int maximum);
  void SetWindowStyle(long)override;
  std::string LastEventError()const;
  NodeHandle EntryHandle()const{return entry_.Handle();}
  NodeHandle DecreaseHandle()const{return decrease_.Handle();}
  NodeHandle IncreaseHandle()const{return increase_.Handle();}
};
}
