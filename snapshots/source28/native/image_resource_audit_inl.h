#pragma once
#include "image_resource_audit.h"
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace olex2::harmony {
inline bool ImageResourceAudit::OwnedScopeBalanced() const {
  if(overflow || handleCount>handles.size())return false;
  for(size_t i=0;i<handleCount;++i) {
    const auto& h=handles[i];
    if(h.created && (!h.creationObserved||!h.releaseAttempted||h.releaseCode!=0))return false;
    if(!h.created && (h.releaseAttempted || (h.creationObserved && h.creationCode==0)))return false;
  }
  if(!file.openAttempted)return handleCount==0;
  if(!file.opened)return file.openError!=0 && handleCount==0;
  if(!file.identityKnown || !file.closeAttempted || file.closeCode!=0 || file.closeError!=0)return false;
  bool sourceCreated=false;
  for(size_t i=0;i<handleCount;++i)if(std::strcmp(handles[i].name,"source")==0&&handles[i].created)sourceCreated=true;
  if(sourceCreated && std::strcmp(inputKind,"owned-memory-buffer")==0)
    return !sdkReceivedFileDescriptor && encodedBytes>0 && bufferReleasedAfterHandles;
  return true;
}
inline bool ImageResourceAudit::Balanced() const {
  if(!OwnedScopeBalanced())return false;
  return !file.opened || (file.scanCompleted && file.remainingInputDescriptors==0);
}
inline AuditedImageFile::AuditedImageFile(const std::string& path,ImageResourceAudit& audit):audit_(audit) {
  auto& f=audit.file;f.openAttempted=true;
  value=open(path.c_str(),O_RDONLY|O_CLOEXEC);f.descriptor=value;
  if(value<0) {f.openError=errno;return;}
  f.opened=true;struct stat state{};
  if(fstat(value,&state)!=0) {f.identityError=errno;return;}
  f.identityKnown=true;f.device=state.st_dev;f.inode=state.st_ino;f.mode=state.st_mode;
}
inline AuditedImageFile::~AuditedImageFile() noexcept {
  if(value<0)return;
  auto& f=audit_.file;f.closeAttempted=true;
  f.closeCode=close(value);f.closeError=f.closeCode==0?0:errno;value=-1;
  // Inspect identities only, never file contents. A surviving SDK duplicate of
  // this synthetic input inode must fail even if its descriptor number changed.
  DIR* dir=opendir("/proc/self/fd");if(!dir) {f.scanError=errno;return;}
  const int own=dirfd(dir);bool complete=true;
  errno=0;
  while(auto* entry=readdir(dir)) {
    char* end=nullptr;long fd=std::strtol(entry->d_name,&end,10);
    if(!end||end==entry->d_name||*end||fd<0||fd==own) {errno=0;continue;}
    struct stat state{};
    if(fstat(static_cast<int>(fd),&state)==0) {
      if(f.identityKnown && uint64_t(state.st_dev)==f.device && uint64_t(state.st_ino)==f.inode)
        ++f.remainingInputDescriptors;
    } else if(errno!=EBADF) {complete=false;++f.scanStatFailures;f.scanError=errno;} // Concurrent close is not our input.
    errno=0;
  }
  if(errno!=0) {complete=false;f.scanError=errno;}
  if(closedir(dir)!=0) {complete=false;f.scanError=errno;}
  f.scanCompleted=complete;
}
inline AuditedImageBuffer::AuditedImageBuffer(const std::string& path,ImageResourceAudit& audit):audit_(audit) {
  audit.inputKind="owned-memory-buffer";
  AuditedImageFile file(path,audit);
  if(file.value<0)throw std::runtime_error("Cannot open image: "+std::string(std::strerror(audit.file.openError)));
  struct stat state{};
  if(fstat(file.value,&state)!=0 || !S_ISREG(state.st_mode) || state.st_size<=0 || uint64_t(state.st_size)>64u*1024u*1024u)
    throw std::runtime_error("Expected an ordinary nonempty encoded image at most 64 MiB");
  bytes.resize(static_cast<size_t>(state.st_size));size_t offset=0;
  while(offset<bytes.size()) {
    const auto count=read(file.value,bytes.data()+offset,bytes.size()-offset);
    if(count<0 && errno==EINTR)continue;
    if(count<=0)throw std::runtime_error("Cannot read complete encoded image");
    offset+=static_cast<size_t>(count);
  }
  uint8_t extra=0;ssize_t tail=0;
  do {tail=read(file.value,&extra,1);}while(tail<0&&errno==EINTR);
  if(tail!=0)throw std::runtime_error("Encoded image changed size during read");
  audit.encodedBytes=bytes.size();
}
inline AuditedImageBuffer::~AuditedImageBuffer() noexcept {
  bool after=true;
  for(size_t i=0;i<audit_.handleCount;++i)if(audit_.handles[i].created&&!audit_.handles[i].releaseAttempted)after=false;
  audit_.bufferReleasedAfterHandles=after;
}
inline std::string ImageResourceAuditJson(const ImageResourceAudit& audit) {
  std::ostringstream out;const auto& f=audit.file;
  out<<"{\"scope\":\"owned input descriptor and public image handle release results only\",\"balanced\":"<<(audit.Balanced()?"true":"false")
    <<",\"owned_scope_balanced\":"<<(audit.OwnedScopeBalanced()?"true":"false")<<",\"input_kind\":\""<<audit.inputKind<<"\",\"encoded_bytes\":"<<audit.encodedBytes
    <<",\"sdk_received_file_descriptor\":"<<(audit.sdkReceivedFileDescriptor?"true":"false")<<",\"buffer_released_after_handles\":"<<(audit.bufferReleasedAfterHandles?"true":"false")
    <<",\"overflow\":"<<(audit.overflow?"true":"false")<<",\"file\":{\"open_attempted\":"<<(f.openAttempted?"true":"false")
    <<",\"opened\":"<<(f.opened?"true":"false")<<",\"descriptor\":"<<f.descriptor<<",\"open_errno\":"<<f.openError
    <<",\"identity_known\":"<<(f.identityKnown?"true":"false")<<",\"device\":"<<f.device<<",\"inode\":"<<f.inode<<",\"mode\":"<<f.mode
    <<",\"identity_errno\":"<<f.identityError<<",\"close_attempted\":"<<(f.closeAttempted?"true":"false")<<",\"close_code\":"<<f.closeCode
    <<",\"close_errno\":"<<f.closeError<<",\"scan_completed\":"<<(f.scanCompleted?"true":"false")<<",\"remaining_input_descriptors\":"<<f.remainingInputDescriptors
    <<",\"scan_errno\":"<<f.scanError<<",\"scan_stat_failures\":"<<f.scanStatFailures<<"},\"handles\":[";
  for(size_t i=0;i<audit.handleCount&&i<audit.handles.size();++i) {
    const auto& h=audit.handles[i];if(i)out<<',';
    out<<"{\"name\":\""<<h.name<<"\",\"creation_observed\":"<<(h.creationObserved?"true":"false")<<",\"creation_code\":"<<h.creationCode
      <<",\"created\":"<<(h.created?"true":"false")<<",\"release_attempted\":"<<(h.releaseAttempted?"true":"false")<<",\"release_code\":"<<h.releaseCode<<'}';
  }
  out<<"]}";return out.str();
}
}
