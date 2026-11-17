#pragma once
#include <arkui/native_dialog.h>
#include <arkui/native_node.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony::ui {
// API12 native button + scrollable native choice dialog. No delimiter encoding,
// no API19 opaque picker structs, and no lossy replacement of arbitrary labels.
class ArkUiChoicePopup {
  struct Item {ArkUiChoicePopup* owner;ArkUI_NodeHandle node=nullptr;int index;};
  ArkUI_NativeNodeAPI_1& api_;
  ArkUI_NodeHandle root_;
  ArkUI_NativeDialogAPI_1* dialogs_=nullptr;
  ArkUI_NativeDialogHandle dialog_=nullptr;
  ArkUI_NodeHandle column_=nullptr,scroll_=nullptr,list_=nullptr;
  std::vector<std::unique_ptr<Item>> rows_;
  std::vector<std::string> options_;
  std::function<void(int)> changed_;
  int selected_=-1;
  bool compact_=false;
  void Label();
  void ClearNodes()noexcept;
  static void Receive(ArkUI_NodeEvent*)noexcept;
public:
  ArkUiChoicePopup(ArkUI_NativeNodeAPI_1&,ArkUI_NodeHandle,std::function<void(int)>);
  ~ArkUiChoicePopup();
  void SetOptions(const std::vector<std::string>&);
  void SetCompact(bool value){compact_=value;Label();}
  void SetSelected(int);
  int Selected()const{return selected_;}
  void Open();
  void Close();
};
}
