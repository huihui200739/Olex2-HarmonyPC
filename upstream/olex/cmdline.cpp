/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "cmdline.h"
#include "eutf8.h"
#include "native_text_positions.h"
#include <algorithm>
#include <limits>
using namespace olex2::harmony::ui;
namespace {
std::string PromptText(const olxstr& value){
  TextUtf16Offset(value,value.Length());auto bytes=TUtf8::Encode(value);
  std::string result(bytes.c_str(),bytes.Length());
  if(result.find('\0')!=std::string::npos)throw std::invalid_argument("Command prompt contains an embedded NUL");
  return result;
}
long CommandStyle(long value){if(value&~(InputReadOnly|InputAlignRight))throw std::invalid_argument("Command input is single-line and accepts only native readonly/alignment flags");return value;}
bool Modifiers(const KeyEvent& key){return key.shift||key.control||key.alt||key.meta;}
long Position(std::size_t value){if(value>static_cast<std::size_t>(std::numeric_limits<long>::max()))throw std::length_error("Command position exceeds the original control range");return static_cast<long>(value);}
}
TCmdLine::Input::Input(NodeBackend& backend,TCmdLine& owner,long style)
 :TTextEdit(backend,&owner,-1,EmptyString(),{},Size{0,36},CommandStyle(style)),owner_(owner){SetNumber(Property::LayoutWeight,1);}
void TCmdLine::Input::OnNativeEvent(const NodeEvent& event){owner_.OnInputEvent(event);}
void TCmdLine::Input::OnInteractionBlocked(const NodeEvent& event){
  if(event.key)event.key->handled=true;
  if(event.kind==EventKind::TextChange)SetValue(GetValue());
}
TCmdLine::TCmdLine(NodeBackend& backend,Widget* parent,int flags)
 :Widget(backend,NodeKind::Row,parent,-1,{},Size{480,36}),AOlxCtrl(this),
  prompt_(backend,NodeKind::Text,this),input_(backend,*this,flags),
  PromptStr(">>"),OnCommand(Actions.New("ONCOMMAND")),
  OnChange(AOlxCtrl::ActionQueue::New(Actions,evt_change_id)),
  OnReturn(AOlxCtrl::ActionQueue::New(Actions,evt_on_return_id)),
  OnLeave(AOlxCtrl::ActionQueue::New(Actions,evt_on_mouse_leave_id)),
  OnEnter(AOlxCtrl::ActionQueue::New(Actions,evt_on_mouse_enter_id)),
  OnClick(AOlxCtrl::ActionQueue::New(Actions,evt_on_click_id)),
  OnChar(AOlxCtrl::ActionQueue::New(Actions,evt_on_char_id)),
  OnKeyDown(AOlxCtrl::ActionQueue::New(Actions,evt_on_key_down_id)) {
  prompt_.SetText(TextProperty::Content,">>");prompt_.SetNumber(Property::Focusable,0);
  SetNumber(Property::RowSpacing,6);SetWindowStyle(flags);SetInsertionPointEnd();
  input_.Widget::SetText(TextProperty::Id,"olex2-original-command-entry");
  input_.Widget::SetText(TextProperty::Accessibility,"命令文字输入");
  prompt_.SetText(TextProperty::Accessibility,"命令提示符，不可编辑");
}
TCmdLine::~TCmdLine()=default;
void TCmdLine::SetWindowStyle(long value){input_.SetWindowStyle(CommandStyle(value));RememberWindowStyle(value);}
std::string TCmdLine::LastEventError()const{if(!Widget::LastEventError().empty())return Widget::LastEventError();return input_.LastEventError();}
void TCmdLine::SetPromptStr(const olxstr& value){
  auto text=PromptText(value);prompt_.SetText(TextProperty::Content,text);PromptStr=value;
}
void TCmdLine::SetText(const olxstr& value){
  if(!value.StartsFrom(PromptStr))throw std::invalid_argument("Command text must retain its configured prompt");
  SetCommand(value.SubStringFrom(PromptStr.Length()));
}
void TCmdLine::SetCommand(const olxstr& value){input_.SetText(value);committedCommand_=value;SetInsertionPointEnd();}
long TCmdLine::GetInsertionPoint()const{return Position(PromptStr.Length()+static_cast<std::size_t>(input_.GetInsertionPoint()));}
void TCmdLine::SetInsertionPoint(long value){
  if(value==-1){SetInsertionPointEnd();return;}
  if(value<0)throw std::out_of_range("Negative command insertion point");
  const auto prefix=PromptStr.Length(),length=prefix+input_.GetValue().Length();
  input_.SetInsertionPoint(Position(std::clamp(static_cast<std::size_t>(value),prefix,length)-prefix));
}
void TCmdLine::SetInsertionPointEnd(){input_.SetInsertionPointEnd();}
void TCmdLine::GetSelection(long* first,long* last)const{
  long start=0,end=0;input_.GetSelection(&start,&end);
  if(first)*first=Position(PromptStr.Length()+static_cast<std::size_t>(start));
  if(last)*last=Position(PromptStr.Length()+static_cast<std::size_t>(end));
}
void TCmdLine::SetSelection(long first,long last){
  if(first==-1&&last==-1){input_.SetSelection(-1,-1);return;}
  if(first<0||last<0)throw std::out_of_range("Negative command selection");
  const auto prefix=PromptStr.Length(),length=prefix+input_.GetValue().Length();
  input_.SetSelection(Position(std::clamp(static_cast<std::size_t>(first),prefix,length)-prefix),Position(std::clamp(static_cast<std::size_t>(last),prefix,length)-prefix));
}
void TCmdLine::SubmitCommand(){
  const auto cmd=GetCommand();if(Commands.IsEmpty()||Commands.GetLastString()!=cmd)Commands.Add(cmd);
  CmdIndex=Commands.Count();OnCommand.Execute(static_cast<AOlxCtrl*>(this));
}
bool TCmdLine::ProcessKey(KeyEvent& key){
  if(!key.down)return false;
  if(key.logical==LogicalKey::Left){
    long first=0,last=0;input_.GetSelection(&first,&last);
    if(!Modifiers(key))return first==last&&last==0;
    if(key.control&&!key.alt&&!key.meta){if(first!=last)return false;auto before=GetCommand().SubStringTo(static_cast<std::size_t>(std::max(0L,last)));return !before.TrimWhiteChars(true).Contains(' ');}
  }else if(key.logical==LogicalKey::Backspace){
    long first=0,last=0;input_.GetSelection(&first,&last);if(first==last&&last==0)return true;
  }else if(key.logical==LogicalKey::Up&&key.control&&!key.shift&&!key.alt&&!key.meta){SetInsertionPoint(Position(PromptStr.Length()));return true;}
  else if(key.logical==LogicalKey::Up){
    if(IsReadOnly())return false;if(CmdIndex>0)--CmdIndex;
    if(CmdIndex<Commands.Count())SetCommand(Commands[CmdIndex]);return true;
  }else if(key.logical==LogicalKey::Down){
    if(IsReadOnly())return false;
    if(Commands.IsEmpty()){CmdIndex=0;return true;}
    if(CmdIndex+1<Commands.Count())++CmdIndex;else CmdIndex=Commands.Count()-1;
    SetCommand(Commands[CmdIndex]);return true;
  }else if(key.logical==LogicalKey::Enter&&!Modifiers(key)){
    key.handled=true;if(!IsReadOnly())SubmitCommand();return true;
  }else if(key.logical==LogicalKey::Escape&&!Modifiers(key)){
    if(!IsReadOnly())SetCommand(EmptyString());return true;
  }else if(key.logical==LogicalKey::Home&&!Modifiers(key)){
    SetInsertionPoint(Position(PromptStr.Length()));return true;
  }
  return false;
}
bool TCmdLine::WillProcessKey(KeyEvent& key){
  if(Modifiers(key)||!key.down)return false;
  if(key.logical==LogicalKey::Delete)return GetInsertionPoint()<Position(GetText().Length());
  if(key.logical==LogicalKey::Backspace)return GetInsertionPoint()>=Position(PromptStr.Length());
  return false;
}
const olxstr& TCmdLine::GetLastCommand(const olxstr& name)const{
  for(std::size_t i=Commands.Count();i>0;--i){const auto& cmd=Commands[i-1];
    if(cmd.StartsFromi(name)&&cmd.Length()>name.Length()&&cmd[name.Length()]==' ')return cmd;
  }
  return EmptyString();
}
void TCmdLine::OnInputEvent(const NodeEvent& event){
  if(event.kind==EventKind::TextChange){if(input_.ApplyTextEvent(event))OnChange.Execute(static_cast<AOlxCtrl*>(this));}
  else if(event.kind==EventKind::Submit){if(!IsReadOnly())SubmitCommand();}
  else if(event.kind==EventKind::Focus)OnEnter.Execute(static_cast<AOlxCtrl*>(this));
  else if(event.kind==EventKind::DoubleClick)OnClick.Execute(static_cast<AOlxCtrl*>(this));
  else if(event.kind==EventKind::Blur){
    const auto life=ObjectLifetime();const auto value=GetCommand();
    if(value!=committedCommand_){committedCommand_=value;OnChange.Execute(static_cast<AOlxCtrl*>(this));if(life.expired())return;}
    OnLeave.Execute(static_cast<AOlxCtrl*>(this));
  }else if(event.kind==EventKind::Key&&event.key&&event.key->down){
    auto& key=*event.key;
    // Set consumed before dispatch: a handler is allowed to release this owner.
    if(key.logical==LogicalKey::Enter&&!Modifiers(key)){key.handled=true;if(!IsReadOnly())SubmitCommand();return;}
    if(ProcessKey(key)){key.handled=true;return;}
    const auto life=ObjectLifetime();TKeyEvent payload(key);OnKeyDown.Execute(static_cast<AOlxCtrl*>(this),&payload);
    if(life.expired())return;
    if(!key.handled&&key.unicode&&!key.control&&!key.alt&&!key.meta)OnChar.Execute(static_cast<AOlxCtrl*>(this),&payload);
  }
}

#else
#include "cmdline.h"
#include "glmouse.h"
#include "bapp.h"

TCmdLine::TCmdLine(wxWindow* parent, int flags) :
  TTextEdit(parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, flags),
  OnCommand(Actions.New("ONCOMMAND"))
{
  PromptStr = ">>";
  SetText(PromptStr);
  SetInsertionPointEnd();
  CmdIndex = 0;
}
//..............................................................................
TCmdLine::~TCmdLine()  {}
//..............................................................................
bool TCmdLine::ProcessKey(wxKeyEvent& evt) {
  if ((evt.GetKeyCode() == WXK_LEFT)) {
    if (evt.GetModifiers() == 0) {
      if (GetInsertionPoint() <= (long)PromptStr.Length())
        return true;
    }
    else if (evt.ControlDown())
      return !GetText().SubStringTo(GetInsertionPoint())
      .TrimWhiteChars(true).Contains(' ');
  }
  else if ((evt.GetKeyCode() == WXK_BACK)) {
    if (GetInsertionPoint() <= (long)PromptStr.Length())
      return true;
  }
  else if ((evt.GetKeyCode() == WXK_UP)) {
    if (--CmdIndex < 0)
      CmdIndex = 0;
    if (CmdIndex < (int)Commands.Count())
      SetCommand(Commands[CmdIndex]);
    return true;
  }
  else if ((evt.GetKeyCode() == WXK_DOWN)) {
    if (++CmdIndex >= (int)Commands.Count()) {
      CmdIndex = Commands.Count()-1;
    }
    if (CmdIndex < (int)Commands.Count() && Commands.Count() != 0)
      SetCommand(Commands[CmdIndex]);
    return true;
  }
  else if ((evt.GetKeyCode() == WXK_RETURN) && evt.GetModifiers() == 0) {
    olxstr cmd = GetCommand();
    if (!Commands.IsEmpty() && (Commands.GetLastString() == cmd))
      ;
    else
      Commands.Add(GetCommand());
    CmdIndex = Commands.Count();
    OnCommand.Execute(dynamic_cast<IOlxObject*>(this));
    return true;
  }
  else if ((evt.GetKeyCode() == WXK_ESCAPE) && evt.GetModifiers() == 0) {
    SetCommand(EmptyString());
    return true;
  }
  else if ((evt.GetKeyCode() == WXK_HOME) && evt.GetModifiers() == 0) {
    SetInsertionPoint(PromptStr.Length());
    return true;
  }
  else if ((evt.GetKeyCode() == WXK_UP) && evt.ControlDown()) {
    SetInsertionPoint(PromptStr.Length());
    return true;
  }
  return false;
}
//..............................................................................
void TCmdLine::SetCommand(const olxstr& cmd)  {
  SetText(PromptStr + cmd);
  SetInsertionPointEnd();
}
//..............................................................................
const olxstr& TCmdLine::GetLastCommand(const olxstr &name) const {
  for (size_t i=Commands.Count()-1; i != InvalidIndex; i--) {
    if (Commands[i].StartsFromi(name) &&
        Commands[i].Length() > name.Length() &&
        Commands[i][name.Length()] == ' ')
    {
      return Commands[i];
    }
  }
  return EmptyString();
}
//..............................................................................
bool TCmdLine::WillProcessKey(wxKeyEvent& evt) {
  if (evt.HasModifiers()) return false;
  olxstr tx = GetText();
  if (evt.GetKeyCode() == WXK_DELETE)
    return GetInsertionPoint() < (long)tx.Length();
  else if (evt.GetKeyCode() == WXK_BACK)
    return GetInsertionPoint() >= (long)PromptStr.Length();
  return false;
}
//..............................................................................

#endif // native command line / original wx
