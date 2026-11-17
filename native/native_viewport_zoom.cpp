#include "native_viewport_zoom.h"
#include "native_widget.h"
#include "labelext.h"
#include "trackbarext.h"
#include <arkui/native_node.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace olex2::harmony::ui;
namespace {
void Error(char* output,std::size_t capacity,const char* message){if(output&&capacity){const auto length=std::min(capacity-1,std::strlen(message));std::memcpy(output,message,length);output[length]=0;}}
void ValueRange(int value){if(value<22||value>100)throw std::invalid_argument("Viewport zoom must be 22..100 tenths");}
struct ZoomView;
struct ChangeForwarder:AActionHandler {
  ZoomView* owner;
  explicit ChangeForwarder(ZoomView* value):owner(value){SetToDelete(false);}
  bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;
};
struct ZoomView {
  ChangeForwarder forward; // Outlives action queues in the child controls.
  Widget row;
  ctrl_ext::TLabel label;
  ctrl_ext::TTrackBar slider;
  ArkUI_NodeContentHandle content=nullptr;
  Olex2NativeZoomChange callback=nullptr;
  void* context=nullptr;
  std::thread::id thread=std::this_thread::get_id();
  bool attached=false;
  ZoomView(int value,bool enabled,Olex2NativeZoomChange change,void* data)
    :forward(this),row(ArkUiBackend(),NodeKind::Row),label(ArkUiBackend(),&row,L"缩放"),
     slider(ArkUiBackend(),&row,-1,value,22,100,{},Size{105,32}),callback(change),context(data) {
    row.SetWidth(145);row.SetHeight(32);row.SetNumber(Property::RowSpacing,8);
    label.SetWidth(32);label.SetNumber(Property::FontSize,11);label.SetNumber(Property::FontColor,0xff89958e);
    label.SetText(TextProperty::Id,"olex2-original-zoom-label");
    slider.SetText(TextProperty::Id,"olex2-view-zoom");slider.SetText(TextProperty::Accessibility,"缩放");
    slider.SetNumber(Property::SliderSelectedColor,0xff438563);slider.SetNumber(Property::SliderTrackColor,0xffe4ece6);
    slider.Enable(enabled);slider.OnChange.Add(&forward);
  }
  void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native zoom owner requires its UI thread");}
  void Pin(const std::shared_ptr<ZoomView>& self){row.SetEventLifetime(self);label.SetEventLifetime(self);slider.SetEventLifetime(self);}
};
bool ChangeForwarder::Execute(const IOlxObject*,const IOlxObject*,TActionQueue*){if(owner->attached&&owner->callback&&owner->callback(owner->context,owner->slider.GetValue())!=0)throw std::runtime_error("Native zoom callback failed");return true;}
struct Views {std::mutex mutex;std::map<void*,std::shared_ptr<ZoomView>> owners;};
Views& Registry(){static Views registry;return registry;}
std::shared_ptr<ZoomView> Find(void* handle){auto& registry=Registry();std::lock_guard<std::mutex> lock(registry.mutex);auto p=registry.owners.find(handle);if(p==registry.owners.end())throw std::invalid_argument("Native zoom owner is no longer active");return p->second;}
}
extern "C" void* olex2_native_zoom_mount(void* nodeContent,int tenths,bool enabled,Olex2NativeZoomChange callback,void* context,char* error,std::size_t capacity){
  try {
    Error(error,capacity,"");ValueRange(tenths);if(!nodeContent||!callback)throw std::invalid_argument("Native zoom needs content and callback");
    auto owner=std::make_shared<ZoomView>(tenths,enabled,callback,context);owner->Pin(owner);owner->content=static_cast<ArkUI_NodeContentHandle>(nodeContent);
    void* handle=owner.get();auto& registry=Registry();{std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.emplace(handle,owner);}
    const int result=OH_ArkUI_NodeContent_AddNode(owner->content,static_cast<ArkUI_NodeHandle>(owner->row.Handle()));
    if(result!=0){std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(handle);throw std::runtime_error("ArkUI could not mount the original zoom controls");}
    owner->attached=true;return handle;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original native zoom construction failed");}return nullptr;
}
extern "C" int olex2_native_zoom_update(void* handle,int tenths,bool enabled,char* error,std::size_t capacity){
  try{Error(error,capacity,"");ValueRange(tenths);auto owner=Find(handle);owner->CheckThread();if(!owner->slider.LastEventError().empty())throw std::runtime_error(owner->slider.LastEventError());owner->slider.SetValue(tenths);owner->slider.Enable(enabled);return 0;}
  catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original native zoom update failed");}return -1;
}
extern "C" int olex2_native_zoom_unmount(void* handle,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");auto owner=Find(handle);owner->CheckThread();
    if(OH_ArkUI_NodeContent_RemoveNode(owner->content,static_cast<ArkUI_NodeHandle>(owner->row.Handle()))!=0)throw std::runtime_error("Native zoom detach failed; its owned nodes were retained");
    owner->attached=false;owner->content=nullptr;auto& registry=Registry();{std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(handle);}return 0;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original native zoom detach failed; owner retained");}return -1;
}
