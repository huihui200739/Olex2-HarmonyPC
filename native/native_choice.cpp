#include "native_choice.h"
#include "eutf8.h"
#include <cmath>
#include <limits>
namespace olex2::harmony::ui {
namespace {
std::vector<std::string> Encode(const std::vector<olxstr>& values){
  std::vector<std::string> result;result.reserve(values.size());
  for(const auto& value:values){auto bytes=TUtf8::Encode(value);result.emplace_back(bytes.c_str(),bytes.Length());}
  return result;
}
}
NativeChoiceList::NativeChoiceList(NodeBackend& backend,Widget* parent,int id,Point pos,Size size,long style,NodeKind kind)
  :Widget(backend,kind,parent,id,pos,size,style){
  if(kind==NodeKind::Choice||kind==NodeKind::ListBox){ConfigureOptionsNode(Handle());SetNumber(Property::Focusable,1);}
  else if(kind!=NodeKind::Row)throw std::invalid_argument("Unsupported native item-list container");
}
void NativeChoiceList::ConfigureOptionsNode(NodeHandle handle){
  AssertThread();if(!handle)throw std::invalid_argument("Native item list needs an option target");
  Backend().SetOptions(handle,{});Backend().SetNumber(handle,Property::ChoiceSelection,-1);optionNode_=handle;
}
void NativeChoiceList::Append(const olxstr& value){
  AssertThread();if(items_.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Too many native choice items");
  auto next=items_;auto clients=clientData_;next.push_back(value);clients.push_back(nullptr);
  Backend().SetOptions(optionNode_,Encode(next));items_.swap(next);clientData_.swap(clients);
  Backend().SetNumber(optionNode_,Property::ChoiceSelection,selection_);
}
void NativeChoiceList::Clear(){
  AssertThread();Backend().SetOptions(optionNode_,{});items_.clear();clientData_.clear();selection_=-1;Backend().SetNumber(optionNode_,Property::ChoiceSelection,-1);
}
void NativeChoiceList::SetSelection(int value){
  AssertThread();if(value < -1 || (value>=0&&static_cast<size_t>(value)>=items_.size()))throw std::out_of_range("Native choice selection is outside its items");
  const auto old=selection_;selection_=value;try{Backend().SetNumber(optionNode_,Property::ChoiceSelection,value);}catch(...){selection_=old;throw;}
}
bool NativeChoiceList::ApplyUserSelection(double value){
  AssertThread();if(!std::isfinite(value)||std::floor(value)!=value||value < -1||value>std::numeric_limits<int>::max())return false;
  const int index=static_cast<int>(value);if(index>=0&&static_cast<size_t>(index)>=items_.size())return false;
  selection_=index;return true;
}
}
