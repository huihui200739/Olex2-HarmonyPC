#pragma once
#include "native_widget.h"
#include <arkui/native_gesture.h>
#include <arkui/native_node.h>
#include <functional>
namespace olex2::harmony::ui {
// No strong imports of API14 functions are introduced into the API12 library.
// Controls requesting hardware keys require the real API14 symbols at runtime;
// absent capability is an explicit construction error, never a fabricated key.
bool NativeKeyboardAvailable();
void RequireNativeKeyboard();
void DispatchNativeKeyboard(ArkUI_NodeEvent*,const std::function<void(const NodeEvent&)>&)noexcept;
class ArkUiDoubleClick {
  ArkUI_NativeGestureAPI_1* api_=nullptr;
  ArkUI_NodeHandle node_=nullptr;
  ArkUI_GestureRecognizer* gesture_=nullptr;
  std::function<void()> receiver_;
  static void Receive(ArkUI_GestureEvent*,void*)noexcept;
  static void Ignore(ArkUI_GestureEvent*,void*)noexcept{}
public:
  ArkUiDoubleClick(ArkUI_NodeHandle,std::function<void()>);
  ~ArkUiDoubleClick();
  ArkUiDoubleClick(const ArkUiDoubleClick&)=delete;
};
}
