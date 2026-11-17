#pragma once
#include <functional>
#include <memory>
#include <string>
namespace olex2::harmony::ui {
// System window geometry is physical pixels; never substitute ArkUI vp here.
struct FrameRectPx {int left=0,top=0,width=0,height=0;};
struct FrameResult {
  bool ok=false,cancelled=false;
  std::string uri,error;int dialogResult=0;
};
using FrameCompletion=std::function<void(const FrameResult&)>;
struct FrameFileRequest {
  std::string caption,filter,defaultFolderUri,defaultFileName;bool open=true;
};
class FramePlatform {
public:
  virtual ~FramePlatform()=default;
  virtual FrameRectPx ReadRect()=0;
  virtual void Move(int leftPx,int topPx,FrameCompletion)=0;
  virtual void Resize(int widthPx,int heightPx,FrameCompletion)=0;
  virtual void SetTitle(const std::string&,FrameCompletion)=0;
  virtual void PickFile(const FrameFileRequest&,FrameCompletion)=0;
};
// Bind an actual HarmonyMainFramePlatform object in its creating N-API env.
// This adapter invokes its real Window and DocumentViewPicker companion, not wx.
std::shared_ptr<FramePlatform> CreateNapiFramePlatform(void* environment,void* platformObject);
}
