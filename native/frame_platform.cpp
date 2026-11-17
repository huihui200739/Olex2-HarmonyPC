#include "frame_platform.h"
#include <napi/native_api.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace olex2::harmony::ui {
namespace {
void Need(napi_env env, napi_status status, const char* action) {
  if (status == napi_ok) return;
  if (status == napi_pending_exception) {
    napi_value ignored = nullptr;
    napi_get_and_clear_last_exception(env, &ignored);
  }
  throw std::runtime_error(std::string("System window adapter could not ") + action);
}
std::string Text(napi_env env, napi_value value, std::size_t limit) {
  napi_valuetype type;
  Need(env, napi_typeof(env, value, &type), "inspect text");
  if (type != napi_string) throw std::invalid_argument("System window response text has the wrong type");
  std::size_t size = 0;
  Need(env, napi_get_value_string_utf8(env, value, nullptr, 0, &size), "measure text");
  if (size > limit) throw std::out_of_range("System window response text is too long");
  std::vector<char> bytes(size + 1);
  std::size_t copied = 0;
  Need(env, napi_get_value_string_utf8(env, value, bytes.data(), bytes.size(), &copied), "read text");
  std::string result(bytes.data(), copied);
  if (copied != size || result.find('\0') != std::string::npos)
    throw std::invalid_argument("System window response text is invalid");
  return result;
}
struct AdapterState;
struct CallbackData { std::weak_ptr<AdapterState> state; uint64_t ticket = 0; };
// The hook owns state until disposal on the environment thread, even if an
// incorrectly threaded client drops its last adapter reference elsewhere.
struct CleanupData { std::shared_ptr<AdapterState> state; };
struct Pending {
  napi_ref callback = nullptr;
  bool picker = false;
  FrameCompletion completion;
  Pending() = default;
  Pending(const Pending&) = delete;
  Pending& operator=(const Pending&) = delete;
  Pending(Pending&& other) noexcept
    : callback(std::exchange(other.callback, nullptr)), picker(other.picker),
      completion(std::move(other.completion)) {}
};
struct AdapterState {
  napi_env env = nullptr;
  std::thread::id thread = std::this_thread::get_id();
  bool alive = true;
  napi_ref object = nullptr;
  std::array<napi_ref, 5> methods{};
  std::map<uint64_t, Pending> requests;
  uint64_t next = 0;
  CleanupData* cleanup = nullptr;
  void Check() const {
    if (std::this_thread::get_id() != thread)
      throw std::runtime_error("System window operations require their creating UI environment thread");
    if (!alive) throw std::runtime_error("System window environment has ended");
  }
  napi_value Call(std::size_t method, std::size_t argc, napi_value* argv) {
    Check();
    napi_value receiver = nullptr, fn = nullptr, result = nullptr;
    Need(env, napi_get_reference_value(env, object, &receiver), "read its platform object");
    Need(env, napi_get_reference_value(env, methods.at(method), &fn), "read its platform method");
    Need(env, napi_call_function(env, receiver, fn, argc, argv, &result), "call its platform method");
    return result;
  }
  void Delete(Pending& pending) noexcept {
    if (pending.callback) napi_delete_reference(env, std::exchange(pending.callback, nullptr));
  }
  void Complete(uint64_t ticket, const FrameResult& result) noexcept {
    auto found = requests.find(ticket);
    if (found == requests.end()) return;
    auto completion = std::move(found->second.completion);
    Delete(found->second);
    requests.erase(found); // Once-only, before any reentrant client callback.
    try { if (completion) completion(result); } catch (...) { /* Never unwind through a VM callback. */ }
  }
  void Shutdown() noexcept {
    if (!alive || std::this_thread::get_id() != thread) return;
    alive = false;
    for (auto& request : requests) Delete(request.second);
    requests.clear(); // Disposed owners are not asynchronously called back.
    for (auto& method : methods) {
      if (method) napi_delete_reference(env, std::exchange(method, nullptr));
    }
    if (object) napi_delete_reference(env, std::exchange(object, nullptr));
    if (cleanup) {
      auto* data = std::exchange(cleanup, nullptr);
      if (napi_remove_env_cleanup_hook(env, EnvironmentCleanup, data) == napi_ok) delete data;
    }
  }
  static void EnvironmentCleanup(void* raw) noexcept {
    auto* data = static_cast<CleanupData*>(raw);
    auto state = data->state;
    state->cleanup = nullptr;
    state->alive = false;
    state->requests.clear();
    state->object = nullptr;
    state->methods.fill(nullptr);
    // The VM owns remaining references and callback finalizers at teardown.
    delete data;
  }
  static void FinalizeCallback(napi_env, void* raw, void*) noexcept {
    delete static_cast<CallbackData*>(raw);
  }
  static napi_value Finish(napi_env env, napi_callback_info info) noexcept {
    std::shared_ptr<AdapterState> state;
    uint64_t ticket = 0;
    try {
      std::size_t argc = 4;
      napi_value args[4]{};
      void* raw = nullptr;
      Need(env, napi_get_cb_info(env, info, &argc, args, nullptr, &raw), "read completion");
      if (raw) {
        auto* data = static_cast<CallbackData*>(raw);
        ticket = data->ticket;
        state = data->state.lock();
      }
      if (state && state->alive) {
        state->Check();
        auto found = state->requests.find(ticket);
        if (found != state->requests.end()) {
          if (argc != 3) throw std::invalid_argument("System window completion must have three fields");
          FrameResult result;
          result.error = Text(env, args[0], 4096);
          napi_valuetype type;
          Need(env, napi_typeof(env, args[1], &type), "inspect cancellation");
          if (type != napi_boolean) throw std::invalid_argument("System window cancellation is not boolean");
          Need(env, napi_get_value_bool(env, args[1], &result.cancelled), "read cancellation");
          result.uri = Text(env, args[2], 65536);
          if ((!found->second.picker && (result.cancelled || !result.uri.empty())) ||
              (!result.error.empty() && (result.cancelled || !result.uri.empty())) ||
              (result.cancelled && !result.uri.empty()) ||
              (found->second.picker && result.error.empty() && !result.cancelled && result.uri.empty()))
            throw std::invalid_argument("System window completion fields are inconsistent");
          result.ok = result.error.empty();
          state->Complete(ticket, result);
        }
      }
    } catch (...) {
      if (state && state->alive) {
        FrameResult failure;
        failure.error = "The system window service returned an invalid completion";
        state->Complete(ticket, failure);
      }
    }
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
  }
};
class NapiFramePlatform final : public FramePlatform {
  std::shared_ptr<AdapterState> state_;
  napi_value Number(int value) {
    napi_value result = nullptr;
    Need(state_->env, napi_create_int32(state_->env, value, &result), "create a coordinate");
    return result;
  }
  napi_value String(const std::string& value) {
    if (value.find('\0') != std::string::npos || value.size() > 65536)
      throw std::invalid_argument("System window request text is invalid");
    napi_value result = nullptr;
    Need(state_->env, napi_create_string_utf8(state_->env, value.data(), value.size(), &result), "create request text");
    return result;
  }
  void Invoke(std::size_t method, std::vector<napi_value> args, bool picker, FrameCompletion completion) {
    auto state = state_;
    state->Check();
    if (!completion) throw std::invalid_argument("System window completion is required");
    if (state->next == std::numeric_limits<uint64_t>::max()) throw std::overflow_error("System window tickets exhausted");
    const auto ticket = ++state->next;
    auto data = std::make_unique<CallbackData>();
    data->state = state;
    data->ticket = ticket;
    napi_value callback = nullptr;
    Need(state->env, napi_create_function(state->env, "olex2SystemWindowDone", NAPI_AUTO_LENGTH,
      AdapterState::Finish, data.get(), &callback), "create completion");
    Need(state->env, napi_add_finalizer(state->env, callback, data.get(),
      AdapterState::FinalizeCallback, nullptr, nullptr), "register completion cleanup");
    data.release();
    Pending pending;
    pending.picker = picker;
    pending.completion = std::move(completion);
    try {
      Need(state->env, napi_create_reference(state->env, callback, 1, &pending.callback), "retain completion");
      state->requests.emplace(ticket, std::move(pending));
    } catch (...) { state->Delete(pending); throw; }
    try {
      args.push_back(callback);
      state->Call(method, args.size(), args.data());
    } catch (...) {
      FrameResult failure;
      failure.error = "The system window operation could not be requested";
      // A service that completed synchronously and then threw cannot cause a
      // second completion or turn that completed operation into a new failure.
      state->Complete(ticket, failure);
    }
  }
public:
  NapiFramePlatform(napi_env env, napi_value object) : state_(std::make_shared<AdapterState>()) {
    if (!env || !object) throw std::invalid_argument("System window environment and companion object are required");
    state_->env = env;
    try {
      napi_valuetype type;
      Need(env, napi_typeof(env, object, &type), "inspect its companion object");
      if (type != napi_object) throw std::invalid_argument("The system window companion is not an object");
      Need(env, napi_create_reference(env, object, 1, &state_->object), "retain its companion object");
      const char* names[] = {"readWindowRect", "moveWindow", "resizeWindow", "setWindowTitle", "pickFile"};
      for (std::size_t i = 0; i < state_->methods.size(); ++i) {
        napi_value fn = nullptr;
        Need(env, napi_get_named_property(env, object, names[i], &fn), "find its platform method");
        Need(env, napi_typeof(env, fn, &type), "inspect its platform method");
        if (type != napi_function) throw std::invalid_argument("The system window companion has a missing method");
        Need(env, napi_create_reference(env, fn, 1, &state_->methods[i]), "retain its platform method");
      }
      auto cleanup = std::make_unique<CleanupData>();
      cleanup->state = state_;
      Need(env, napi_add_env_cleanup_hook(env, AdapterState::EnvironmentCleanup, cleanup.get()), "register environment cleanup");
      state_->cleanup = cleanup.release();
    } catch (...) { state_->Shutdown(); throw; }
  }
  ~NapiFramePlatform() override { state_->Shutdown(); }
  FrameRectPx ReadRect() override {
    auto state = state_;
    state->Check();
    auto value = state->Call(0, 0, nullptr);
    const char* names[] = {"left", "top", "width", "height"};
    std::array<int, 4> fields{};
    for (std::size_t i = 0; i < fields.size(); ++i) {
      napi_value member = nullptr;
      napi_valuetype type;
      Need(state->env, napi_get_named_property(state->env, value, names[i], &member), "read window geometry");
      Need(state->env, napi_typeof(state->env, member, &type), "inspect window geometry");
      if (type != napi_number) throw std::invalid_argument("System window geometry is not numeric");
      double number = 0;
      Need(state->env, napi_get_value_double(state->env, member, &number), "decode window geometry");
      if (!std::isfinite(number) || std::trunc(number) != number ||
          number < std::numeric_limits<int32_t>::min() || number > std::numeric_limits<int32_t>::max() ||
          (i >= 2 && number <= 0)) throw std::out_of_range("System window geometry is outside its pixel range");
      fields[i] = static_cast<int>(number);
    }
    return {fields[0], fields[1], fields[2], fields[3]};
  }
  void Move(int left, int top, FrameCompletion completion) override {
    state_->Check();
    Invoke(1, {Number(left), Number(top)}, false, std::move(completion));
  }
  void Resize(int width, int height, FrameCompletion completion) override {
    state_->Check();
    if (width <= 0 || height <= 0) throw std::out_of_range("System window pixel dimensions must be positive");
    Invoke(2, {Number(width), Number(height)}, false, std::move(completion));
  }
  void SetTitle(const std::string& title, FrameCompletion completion) override {
    state_->Check();
    if (title.size() > 4096) throw std::out_of_range("System window title is too long");
    Invoke(3, {String(title)}, false, std::move(completion));
  }
  void PickFile(const FrameFileRequest& request, FrameCompletion completion) override {
    state_->Check();
    napi_value value = nullptr;
    Need(state_->env, napi_create_object(state_->env, &value), "create a picker request");
    const char* names[] = {"caption", "filter", "defaultFolderUri", "defaultFileName"};
    const std::string* fields[] = {&request.caption, &request.filter, &request.defaultFolderUri, &request.defaultFileName};
    for (std::size_t i = 0; i < 4; ++i)
      Need(state_->env, napi_set_named_property(state_->env, value, names[i], String(*fields[i])), "set a picker option");
    napi_value open = nullptr;
    Need(state_->env, napi_get_boolean(state_->env, request.open, &open), "create a picker mode");
    Need(state_->env, napi_set_named_property(state_->env, value, "open", open), "set a picker mode");
    Invoke(4, {value}, true, std::move(completion));
  }
};
}
std::shared_ptr<FramePlatform> CreateNapiFramePlatform(void* environment, void* object) {
  return std::make_shared<NapiFramePlatform>(static_cast<napi_env>(environment), static_cast<napi_value>(object));
}
}
