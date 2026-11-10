#pragma once
#include "log.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony {
struct OriginalLogEntry {
  uint64_t sequence=0;
  std::string text;
  bool truncated=false;
};
struct OriginalLogSnapshot {
  bool attached=false;
  uint64_t posted=0,dropped=0,truncated=0,captureFailures=0,clearCount=0,clearedEntries=0;
  size_t retainedBytes=0;
  std::string textSha256;
  std::vector<OriginalLogEntry> entries;
};
constexpr size_t MaximumOriginalLogEntries=256;
constexpr size_t MaximumOriginalLogBytes=64*1024;
constexpr size_t MaximumOriginalLogEntryBytes=4096;
// Observes the actual TLog::OnPost notification, not raw stream writes or a
// replacement logger. OnPost does not supply severity; none is invented.
// Platform callers serialize all original-library work with OriginalInterpreterGuard.
// This observer does not certify unrelated legacy callers as globally thread-safe.
// The TLog must outlive this observer. Attach/detach use its original critical
// section, while snapshots never log or acquire the original interpreter lock.
class OriginalLogCapture final:public AActionHandler {
  struct Impl;
  std::unique_ptr<Impl> impl_;
  TLog& log_;
  bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*) noexcept override;
public:
  explicit OriginalLogCapture(TLog& log);
  ~OriginalLogCapture() override;
  OriginalLogCapture(const OriginalLogCapture&)=delete;
  OriginalLogCapture& operator=(const OriginalLogCapture&)=delete;
  OriginalLogSnapshot Snapshot() const;
  void Clear();
};
constexpr const char* OriginalLogSource(){return "upstream/sdl/log.cpp: actual TLog::OnPost notifications -> bounded UTF-8 in-memory observer; no severity or direct-stream capture claim";}
}
