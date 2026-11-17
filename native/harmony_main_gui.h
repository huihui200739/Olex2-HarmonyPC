#pragma once
#include <string>
namespace gxlib { class TGXApp; }
namespace olex2::harmony {
// Dispatches only original main-form actions that have a native Harmony UI.
bool DispatchHarmonyMainGuiAction(gxlib::TGXApp& app, const std::string& action);
}
