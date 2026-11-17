#include "native_main_frame.h"
#include "frameext.h"
#include "msgbox.h"
#include "eutf8.h"
#include "native_text_positions.h"
#include <arkui/native_node.h>
#include <napi/native_api.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace olex2::harmony::ui;
namespace {
void Error(char* buffer, std::size_t capacity, const char* text) noexcept {
  if (!buffer || !capacity) return;
  auto count = std::min(capacity - 1, std::strlen(text));
  std::memcpy(buffer, text, count); buffer[count] = '\0';
}
olxstr Decode(const char* value) {
  if (!value) throw std::invalid_argument("Main-frame text is absent");
  auto length = std::strlen(value);
  if (length > 65536) throw std::out_of_range("Main-frame text is too long");
  auto result = TUtf8::Decode(value, length);
  TextUtf16Offset(result, result.Length());
  return result;
}
class MainFrame final : public TMainFrame {
  olxstr scenes_;
public:
  MainFrame(std::shared_ptr<FramePlatform> platform, std::shared_ptr<DialogBackend> dialogs,
    std::shared_ptr<UiScheduler> scheduler, const Olex2NativeFrameValues& values)
    : TMainFrame(ArkUiBackend(), std::move(platform), std::move(dialogs), std::move(scheduler),
      Decode(values.title), Decode(values.className), {values.contentWidthVp, values.contentHeightVp}),
      scenes_(Decode(values.scenesFolder)) {}
  void SetScenesFolder(const olxstr& value) override { scenes_ = value; }
  const olxstr& GetScenesFolder() const override { return scenes_; }
};
struct Owner;
struct Cleanup { std::weak_ptr<Owner> owner; };
struct Registry {
  std::mutex mutex;
  std::map<void*, std::shared_ptr<Owner>> owners;
  uintptr_t next = 0;
};
Registry& Owners() { static auto* registry = new Registry; return *registry; }
struct Owner : std::enable_shared_from_this<Owner> {
  std::thread::id thread = std::this_thread::get_id();
  std::shared_ptr<UiScheduler> scheduler;
  std::shared_ptr<DialogBackend> dialogs;
  std::unique_ptr<MainFrame> frame;
  ArkUI_NodeContentHandle content = nullptr;
  bool attached = false, environmentAlive = true;
  napi_env env = nullptr;
  Cleanup* cleanup = nullptr;
  void* key = nullptr;
  Olex2NativeFrameCompleted completed = nullptr;
  void* context = nullptr;
  ~Owner() {
    if (cleanup && environmentAlive && thread == std::this_thread::get_id()) {
      if (napi_remove_env_cleanup_hook(env, EnvironmentCleanup, cleanup) == napi_ok) delete cleanup;
    }
  }
  void Check() const {
    if (thread != std::this_thread::get_id()) throw std::runtime_error("Main frame requires its creating UI thread");
    if (!environmentAlive) throw std::runtime_error("Main-frame UI environment has ended");
  }
  void Complete(Olex2NativeFrameAction action, const FrameResult& result, FrameRectPx rect = {}) {
    if (!environmentAlive || !completed) return;
    Olex2NativeFrameResult value{result.ok, result.cancelled, result.uri.c_str(), result.error.c_str(),
      result.dialogResult, rect.left, rect.top, rect.width, rect.height};
    completed(context, action, &value);
  }
  FrameCompletion Done(Olex2NativeFrameAction action) {
    std::weak_ptr<Owner> weak = shared_from_this();
    return [weak, action](const FrameResult& result) { if (auto owner = weak.lock()) owner->Complete(action, result); };
  }
  static void EnvironmentCleanup(void* raw) noexcept {
    auto* hook = static_cast<Cleanup*>(raw);
    auto owner = hook->owner.lock();
    delete hook;
    if (!owner) return;
    owner->cleanup = nullptr;
    owner->environmentAlive = false;
    owner->completed = nullptr;
    try {
      owner->frame->SilenceCompletions();
      if (owner->frame->HasAlert()) owner->frame->CloseAlert(DialogCancelled);
      // Never destroy nodes that the UI still owns. A failed detach or an
      // alert still closing quarantines this owner until process teardown.
      if (owner->frame->HasAlert()) return;
      if (owner->attached && OH_ArkUI_NodeContent_RemoveNode(owner->content,
          static_cast<ArkUI_NodeHandle>(owner->frame->Handle())) != 0) return;
      owner->attached = false;
      auto& registry = Owners();
      std::lock_guard<std::mutex> lock(registry.mutex);
      registry.owners.erase(owner->key);
    } catch (...) { /* Keep the complete owner, not dangling platform/UI nodes. */ }
  }
};
std::shared_ptr<Owner> Find(void* key) {
  auto& registry = Owners();
  std::lock_guard<std::mutex> lock(registry.mutex);
  auto found = registry.owners.find(key);
  if (found == registry.owners.end()) throw std::invalid_argument("Unknown main-frame owner");
  return found->second;
}
}
extern "C" void* olex2_native_frame_mount(void* environment, void* companion, void* content,
  const Olex2NativeFrameValues* values, Olex2NativeFrameCompleted completed,
  void* context, char* error, std::size_t capacity) {
  try {
    Error(error, capacity, "");
    if (!environment || !companion || !content || !values || !completed)
      throw std::invalid_argument("Main frame requires environment, system-window companion, content and completion");
    if (values->contentWidthVp <= 0 || values->contentHeightVp <= 0 ||
        values->contentWidthVp > 16384 || values->contentHeightVp > 16384)
      throw std::out_of_range("Main-frame content is outside its vp range");
    auto owner = std::make_shared<Owner>();
    owner->env = static_cast<napi_env>(environment);
    owner->content = static_cast<ArkUI_NodeContentHandle>(content);
    owner->completed = completed; owner->context = context;
    owner->scheduler = CreateNapiUiScheduler(environment);
    owner->dialogs = CreateArkUiDialogBackend(owner->scheduler);
    owner->frame = std::make_unique<MainFrame>(CreateNapiFramePlatform(environment, companion),
      owner->dialogs, owner->scheduler, *values);
    auto& registry = Owners();
    {
      std::lock_guard<std::mutex> lock(registry.mutex);
      if (registry.next == std::numeric_limits<uintptr_t>::max()) throw std::overflow_error("Main-frame handles exhausted");
      owner->key = reinterpret_cast<void*>(++registry.next);
      registry.owners.emplace(owner->key, owner);
    }
    auto hook = std::make_unique<Cleanup>(); hook->owner = owner;
    if (napi_add_env_cleanup_hook(owner->env, Owner::EnvironmentCleanup, hook.get()) != napi_ok) {
      std::lock_guard<std::mutex> lock(registry.mutex); registry.owners.erase(owner->key);
      throw std::runtime_error("Main-frame cleanup could not be registered");
    }
    owner->cleanup = hook.release();
    if (OH_ArkUI_NodeContent_AddNode(owner->content, static_cast<ArkUI_NodeHandle>(owner->frame->Handle())) != 0) {
      std::lock_guard<std::mutex> lock(registry.mutex); registry.owners.erase(owner->key);
      throw std::runtime_error("ArkUI could not attach the original main-frame content");
    }
    owner->attached = true;
    return owner->key;
  } catch (const std::exception& e) { Error(error, capacity, e.what()); }
  catch (...) { Error(error, capacity, "Main-frame mount failed"); }
  return nullptr;
}
extern "C" int olex2_native_frame_request(void* key, const Olex2NativeFrameRequest* value,
  char* error, std::size_t capacity) {
  try {
    Error(error, capacity, "");
    if (!value) throw std::invalid_argument("Main-frame request is absent");
    auto owner = Find(key); owner->Check();
    const auto action = value->action;
    auto done = owner->Done(action);
    if (owner->frame->HasPendingRequests() && action != Olex2NativeFrameAction::CloseAlert)
      throw std::logic_error("A main-frame request is still pending");
    switch (action) {
      case Olex2NativeFrameAction::ReadRect: {
        FrameResult result; result.ok = true; owner->Complete(action, result, owner->frame->ReadWindowRect()); break;
      }
      case Olex2NativeFrameAction::SavePosition: {
        owner->frame->SavePosition(*owner->frame); FrameResult result; result.ok = true; done(result); break;
      }
      case Olex2NativeFrameAction::RestorePosition: owner->frame->RestorePosition(*owner->frame, std::move(done)); break;
      case Olex2NativeFrameAction::Move: owner->frame->MoveWindow(value->first, value->second, std::move(done)); break;
      case Olex2NativeFrameAction::Resize: owner->frame->ResizeWindow(value->first, value->second, std::move(done)); break;
      case Olex2NativeFrameAction::Title: owner->frame->SetWindowTitle(Decode(value->text), std::move(done)); break;
      case Olex2NativeFrameAction::OpenFile:
      case Olex2NativeFrameAction::SaveFile:
        owner->frame->PickFile(Decode(value->title), Decode(value->filter), Decode(value->folderUri),
          Decode(value->fileName), action == Olex2NativeFrameAction::OpenFile, std::move(done)); break;
      case Olex2NativeFrameAction::Alert:
        owner->frame->Alert(Decode(value->text), Decode(value->title), value->flags, std::move(done)); break;
      case Olex2NativeFrameAction::CloseAlert:
        // This closes the original pending Alert; it does not fabricate a
        // second completed alert or claim that Close() already detached it.
        owner->frame->CloseAlert(value->first); break;
      default: throw std::invalid_argument("Unknown main-frame platform action");
    }
    return 0;
  } catch (const std::exception& e) { Error(error, capacity, e.what()); }
  catch (...) { Error(error, capacity, "Main-frame request failed"); }
  return -1;
}
extern "C" int olex2_native_frame_unmount(void* key, char* error, std::size_t capacity) {
  try {
    Error(error, capacity, "");
    auto owner = Find(key); owner->Check();
    if (owner->frame->HasAlert()) owner->frame->CloseAlert(DialogCancelled);
    if (owner->frame->HasPendingRequests() || owner->frame->HasAlert()) {
      Error(error, capacity, "Waiting for the actual window, picker or dialog completion; owner retained"); return 1;
    }
    if (OH_ArkUI_NodeContent_RemoveNode(owner->content, static_cast<ArkUI_NodeHandle>(owner->frame->Handle())) != 0)
      throw std::runtime_error("Main-frame content detach failed; owner retained");
    owner->attached = false; owner->content = nullptr;
    auto& registry = Owners();
    std::lock_guard<std::mutex> lock(registry.mutex); registry.owners.erase(key);
    return 0;
  } catch (const std::exception& e) { Error(error, capacity, e.what()); }
  catch (...) { Error(error, capacity, "Main-frame detach failed; owner retained"); }
  return -1;
}
