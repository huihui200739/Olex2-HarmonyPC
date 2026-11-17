#pragma once
#include "native_widget.h"
#include <arkui/native_node.h>
namespace olex2::harmony::ui {
// The public ImageKit/ArkUI APIs own real decoded pixels, not a URL placeholder.
ImageRef DecodeArkUiButtonImage(const std::vector<uint8_t>& encoded);
void WriteArkUiButtonImage(ArkUI_NativeNodeAPI_1&,ArkUI_NodeHandle,const ImageRef&);
void DispatchNativeButtonPointer(ArkUI_NodeEvent*,ArkUI_NodeHandle,const std::function<void(const NodeEvent&)>&);
}
