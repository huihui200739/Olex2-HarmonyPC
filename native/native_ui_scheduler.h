#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace olex2::harmony::ui {
// All methods, callbacks and destruction belong to the creating ArkTS/UI thread.
// Dead environments/cancelled tickets never dispatch an original action.
class UiScheduler {
public:
  using Ticket=uint64_t;
  virtual ~UiScheduler()=default;
  virtual Ticket Start(uint64_t milliseconds,bool oneShot,std::function<void()>)=0;
  virtual void Stop(Ticket)=0;
  virtual bool IsActive(Ticket)const=0;
  virtual uint64_t NowMs()const=0;
  virtual std::string LastError()const=0;
};
// Uses this environment's standard timer functions and N-API function lifetimes.
// No uv_default_loop(), deprecated napi_get_uv_event_loop(), polling worker,
// background ArkUI mutation or application interpreter execution is involved.
std::unique_ptr<UiScheduler> CreateNapiUiScheduler(void* environment);
}
