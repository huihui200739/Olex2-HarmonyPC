#pragma once
#include <cstddef>
#include <cstdint>
// Private platform-only UI entry points. No macros, application Python or
// computation is dispatched. N-API values must belong to the provided UI env.
struct Olex2NativeFrameValues {
  const char* title = "";
  const char* className = "";
  const char* scenesFolder = "";
  int contentWidthVp = 640, contentHeightVp = 480;
};
enum class Olex2NativeFrameAction : int {
  ReadRect = 0, SavePosition, RestorePosition, Move, Resize, Title,
  OpenFile, SaveFile, Alert, CloseAlert
};
struct Olex2NativeFrameRequest {
  Olex2NativeFrameAction action = Olex2NativeFrameAction::ReadRect;
  int first = 0, second = 0;
  const char* text = "";
  const char* title = "";
  const char* filter = "";
  const char* folderUri = "";
  const char* fileName = "";
  uint32_t flags = 1;
};
struct Olex2NativeFrameResult {
  bool ok = false, cancelled = false;
  const char* uri = "";
  const char* error = "";
  int dialogResult = 0;
  int leftPx = 0, topPx = 0, widthPx = 0, heightPx = 0;
};
// Result strings are borrowed only for the duration of this completion call.
using Olex2NativeFrameCompleted = void(*)(void*, Olex2NativeFrameAction, const Olex2NativeFrameResult*);
extern "C" {
void* olex2_native_frame_mount(void* napiEnvironment, void* napiPlatformObject,
  void* nodeContent, const Olex2NativeFrameValues*, Olex2NativeFrameCompleted,
  void* context, char* error, std::size_t);
int olex2_native_frame_request(void*, const Olex2NativeFrameRequest*, char* error, std::size_t);
// 0 detached, 1 waiting for a real pending completion/detach, -1 failed and
// ownership retained. Callers must retry rather than free context prematurely.
int olex2_native_frame_unmount(void*, char* error, std::size_t);
}
