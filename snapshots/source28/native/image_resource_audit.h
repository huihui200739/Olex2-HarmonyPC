#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace olex2::harmony {
// Bounded, allocation-free cleanup bookkeeping. Destructors never throw, retry
// close(), or close a descriptor that this scope did not itself open.
struct ImageHandleAudit {
  const char* name="";
  bool creationObserved=false, created=false, releaseAttempted=false;
  int creationCode=0, releaseCode=0;
};
struct ImageFileAudit {
  bool openAttempted=false, opened=false, identityKnown=false;
  bool closeAttempted=false, scanCompleted=false;
  int descriptor=-1, openError=0, identityError=0, closeCode=0, closeError=0;
  uint64_t device=0,inode=0,mode=0;
  unsigned remainingInputDescriptors=0, scanStatFailures=0;
  int scanError=0;
};
struct ImageResourceAudit {
  std::array<ImageHandleAudit,5> handles{};
  size_t handleCount=0;
  bool overflow=false;
  const char* inputKind="file-descriptor";
  size_t encodedBytes=0;
  bool sdkReceivedFileDescriptor=false, bufferReleasedAfterHandles=false;
  ImageFileAudit file;
  bool OwnedScopeBalanced() const;
  bool Balanced() const;
};
std::string ImageResourceAuditJson(const ImageResourceAudit&);

template<class T,auto Release> class AuditedImageHandle {
  ImageResourceAudit& audit_;
  ImageHandleAudit* record_=nullptr;
public:
  T* value=nullptr;
  AuditedImageHandle(ImageResourceAudit& audit,const char* name):audit_(audit) {
    if(audit.handleCount<audit.handles.size()) {
      record_=&audit.handles[audit.handleCount++];record_->name=name;
    } else audit.overflow=true;
  }
  template<class Code> Code Created(Code code) noexcept {
    if(record_) {record_->creationObserved=true;record_->creationCode=static_cast<int>(code);record_->created=value!=nullptr;}
    return code;
  }
  ~AuditedImageHandle() noexcept {
    if(value) {
      if(record_) {record_->created=true;record_->releaseAttempted=true;}
      const int code=static_cast<int>(Release(value));
      if(record_)record_->releaseCode=code;
    }
  }
  AuditedImageHandle(const AuditedImageHandle&)=delete;
  AuditedImageHandle& operator=(const AuditedImageHandle&)=delete;
};
class AuditedImageFile {
  ImageResourceAudit& audit_;
public:
  int value=-1;
  AuditedImageFile(const std::string& path,ImageResourceAudit& audit);
  ~AuditedImageFile() noexcept;
  AuditedImageFile(const AuditedImageFile&)=delete;
  AuditedImageFile& operator=(const AuditedImageFile&)=delete;
};
// The encoded bytes outlive every public ImageSource/PixelMap owner. The file
// is closed inside this constructor, before the decoder receives a byte pointer.
class AuditedImageBuffer {
  ImageResourceAudit& audit_;
public:
  std::vector<uint8_t> bytes;
  AuditedImageBuffer(const std::string& path,ImageResourceAudit& audit);
  ~AuditedImageBuffer() noexcept;
  AuditedImageBuffer(const AuditedImageBuffer&)=delete;
  AuditedImageBuffer& operator=(const AuditedImageBuffer&)=delete;
};
}

#include "image_resource_audit_inl.h"
