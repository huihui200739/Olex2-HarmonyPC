#pragma once
#include "arkui_input_events.h"
#include <arkui/native_node.h>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony::ui {
// A real visible scroll/column/button list, not a dropdown or recording backend.
class ArkUiListBox {
  struct Item {ArkUiListBox* owner=nullptr;ArkUI_NodeHandle node=nullptr;int index=-1;std::unique_ptr<ArkUiDoubleClick> doubleClick;};
  struct Graph {ArkUI_NodeHandle column=nullptr;std::vector<std::unique_ptr<Item>> rows;bool attached=false;};
  ArkUI_NativeNodeAPI_1& api_;ArkUI_NodeHandle root_;int selected_=-1;
  std::unique_ptr<Graph> graph_;
  std::function<void(EventKind,int)> receiver_;
  std::unique_ptr<Graph> Build(const std::vector<std::string>&);
  void Clear(Graph&,bool dispose)noexcept;
  void Emit(EventKind,int);
  static void Receive(ArkUI_NodeEvent*)noexcept;
public:
  ArkUiListBox(ArkUI_NativeNodeAPI_1&,ArkUI_NodeHandle,std::function<void(EventKind,int)>);
  ~ArkUiListBox();
  void SetOptions(const std::vector<std::string>&);
  void SetSelected(int);
  int Selected()const{return selected_;}
};
}
