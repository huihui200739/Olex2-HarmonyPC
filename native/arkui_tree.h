#pragma once
#include "native_widget.h"
#include "arkui_input_events.h"
#include <arkui/native_node.h>
#include <memory>
namespace olex2::harmony::ui {
class ArkUiTree {
  struct Item {
    ArkUiTree* owner=nullptr;int index=0;
    ArkUI_NodeHandle row=nullptr,toggle=nullptr,label=nullptr;
    std::unique_ptr<ArkUiDoubleClick> doubleClick;
    bool editing=false;std::string draft;
  };
  struct Graph {ArkUI_NodeHandle horizontal=nullptr,column=nullptr;bool attached=false;std::vector<std::unique_ptr<Item>> rows;};
  ArkUI_NativeNodeAPI_1& api_;ArkUI_NodeHandle root_;float width_=480;bool updating_=false;
  std::unique_ptr<Graph> graph_;
  std::function<void(const NodeEvent&)> receiver_;
  std::unique_ptr<Graph> Build(const std::vector<TreeRow>&);
  void Clear(Graph&,bool dispose)noexcept;
  void Emit(EventKind,int,const std::string& text={});
  static void Receive(ArkUI_NodeEvent*)noexcept;
public:
  ArkUiTree(ArkUI_NativeNodeAPI_1&,ArkUI_NodeHandle,std::function<void(const NodeEvent&)>);
  ~ArkUiTree();
  void SetRows(const std::vector<TreeRow>&);
  void SetViewportWidth(float);
  double Scroll(bool horizontal)const;
  void SetScroll(bool horizontal,double);
};
}
