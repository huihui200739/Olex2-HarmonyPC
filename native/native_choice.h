#pragma once
#include "native_widget.h"
#include "estrlist.h"
#include <stdexcept>
namespace olex2::harmony::ui {
// Real picker storage seam used by the original TItemList, not a wx substitute.
class NativeChoiceList:public Widget {
  std::vector<olxstr> items_;
  std::vector<void*> clientData_;
  int selection_=-1;
  NodeHandle optionNode_=nullptr;
protected:
  bool ApplyUserSelection(double);
  void ConfigureOptionsNode(NodeHandle);
  NodeHandle OptionsNode()const{return optionNode_;}
public:
  NativeChoiceList(NodeBackend&,Widget*,int id=-1,Point={},Size={},long style=0,NodeKind kind=NodeKind::Choice);
  void Append(const olxstr&);
  void Clear();
  size_t GetCount()const{return items_.size();}
  bool IsEmpty()const{return items_.empty();}
  const olxstr& GetString(size_t i)const{return items_.at(i);}
  void* GetClientData(size_t i)const{return clientData_.at(i);}
  void SetClientData(size_t i,void* value){clientData_.at(i)=value;}
  int GetSelection()const{return selection_;}
  void SetSelection(int);
};
}
