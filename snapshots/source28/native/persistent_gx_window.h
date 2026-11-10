#pragma once
#include <string>
#include "platform_window_scene.h"
#include "gxapp.h"
namespace olex2::harmony {
struct ApplicationRuntimeStatus;
class OriginalLogCapture;
// Caller owns the runtime mutex and the nonrecursive original interpreter guard.
std::string RunPersistentGXWindowLocked(const Olex2LegacyHost&, const ApplicationRuntimeStatus&,
                                       TGXApp&, PlatformWindowScene&, PlatformSceneLifetime&, OriginalLogCapture* = nullptr);
}
