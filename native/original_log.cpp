#include "original_log.h"
#include "eutf8.h"
#include "platform_sha256.h"
#include <atomic>
#include <cctype>
#include <deque>
#include <mutex>
namespace olex2::harmony {
namespace {
std::string Digest(const std::string& bytes){return PlatformSha256(bytes);}
size_t Utf8Prefix(const std::string& text,size_t maximum){
  size_t end=std::min(text.size(),maximum);
  // UTF-8 produced by the original encoder is canonical. Never retain a
  // partial code point when a multibyte character crosses the byte budget.
  if(end<text.size())while(end>0 && (static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;
  return end;
}
}
struct OriginalLogCapture::Impl {
  mutable std::mutex mutex;
  std::deque<OriginalLogEntry> entries;
  uint64_t posted=0,dropped=0,truncated=0,clearCount=0,clearedEntries=0;
  size_t retainedBytes=0;
  std::atomic<uint64_t> failures{0};
};
OriginalLogCapture::OriginalLogCapture(TLog& log):impl_(new Impl),log_(log){
  SetToDelete(false);
  volatile olx_scope_cs guard(TLog::GetCriticalSection());
  log_.OnPost.Add(this);
}
OriginalLogCapture::~OriginalLogCapture(){
  volatile olx_scope_cs guard(TLog::GetCriticalSection());
  log_.OnPost.Remove(this);
}
bool OriginalLogCapture::Execute(const IOlxObject* sender,const IOlxObject* data,TActionQueue* queue) noexcept {
  if(sender!=&log_ || queue!=&log_.OnPost)return false;
  const auto* text=dynamic_cast<const olxstr*>(data);
  if(!text)return false;
  try{
    const olxcstr encoded=TUtf8::Encode(*text);
    const std::string bytes(encoded.c_str(),encoded.Length());
    OriginalLogEntry entry;entry.text=bytes.substr(0,Utf8Prefix(bytes,MaximumOriginalLogEntryBytes));entry.truncated=entry.text.size()!=bytes.size();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    entry.sequence=impl_->posted+1;
    // Allocate before eviction or counter changes, so a failed allocation
    // cannot leave a partial visible entry or an invented successful post.
    impl_->entries.push_back(entry);++impl_->posted;
    if(entry.truncated)++impl_->truncated;
    impl_->retainedBytes+=entry.text.size();
    while(impl_->entries.size()>MaximumOriginalLogEntries || impl_->retainedBytes>MaximumOriginalLogBytes){impl_->retainedBytes-=impl_->entries.front().text.size();impl_->entries.pop_front();++impl_->dropped;}
  }catch(...){
    // TLog emits from a destructor: observer failure must not escape through it.
    impl_->failures.fetch_add(1,std::memory_order_relaxed);
  }
  return false; // Never consume or suppress another original log consumer.
}
OriginalLogSnapshot OriginalLogCapture::Snapshot() const {
  OriginalLogSnapshot s;std::string bytes;
  {std::lock_guard<std::mutex> lock(impl_->mutex);s.attached=true;s.posted=impl_->posted;s.dropped=impl_->dropped;s.truncated=impl_->truncated;s.clearCount=impl_->clearCount;s.clearedEntries=impl_->clearedEntries;s.retainedBytes=impl_->retainedBytes;s.entries.assign(impl_->entries.begin(),impl_->entries.end());}
  s.captureFailures=impl_->failures.load(std::memory_order_relaxed);
  for(const auto& e:s.entries)bytes+=e.text;
  s.textSha256=Digest(bytes);return s;
}
void OriginalLogCapture::Clear(){std::lock_guard<std::mutex> lock(impl_->mutex);impl_->clearedEntries+=impl_->entries.size();impl_->entries.clear();impl_->retainedBytes=0;++impl_->clearCount;}
}
