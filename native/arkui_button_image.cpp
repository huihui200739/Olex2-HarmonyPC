#include "arkui_button_image.h"
#include <arkui/drawable_descriptor.h>
#include <arkui/ui_input_event.h>
#include <multimedia/image_framework/image/image_source_native.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
void ImageCheck(Image_ErrorCode value,const char* operation){if(value!=IMAGE_SUCCESS)throw std::runtime_error(std::string(operation)+" failed ("+std::to_string(value)+")");}
template<class T,Image_ErrorCode(*Release)(T*)> struct ImageOwner {
 T* value=nullptr;~ImageOwner(){if(value)Release(value);}ImageOwner()=default;ImageOwner(const ImageOwner&)=delete;
};
struct ImageState {
 // Encoded data outlives the native source and every pixel/descriptor owner.
 std::vector<uint8_t> encoded;ImageOwner<OH_ImageSourceNative,OH_ImageSourceNative_Release> source;ImageOwner<OH_PixelmapNative,OH_PixelmapNative_Release> pixelmap;ArkUI_DrawableDescriptor* drawable=nullptr;
 ~ImageState(){if(drawable)OH_ArkUI_DrawableDescriptor_Dispose(drawable);}
};
struct PlatformImage final:ImageAsset {
 std::shared_ptr<ImageState> state;PlatformImage(uint32_t w,uint32_t h,std::shared_ptr<ImageState> s):ImageAsset(w,h),state(std::move(s)){}
};
}
ImageRef DecodeArkUiButtonImage(const std::vector<uint8_t>& encoded){
 if(encoded.empty()||encoded.size()>64u*1024u*1024u)throw std::invalid_argument("Button image must contain at most 64 MiB of encoded data");
 auto state=std::make_shared<ImageState>();state->encoded=encoded;
 ImageCheck(OH_ImageSourceNative_CreateFromData(state->encoded.data(),state->encoded.size(),&state->source.value),"Create button image source");if(!state->source.value)throw std::runtime_error("ImageKit returned no button source");
 ImageOwner<OH_ImageSource_Info,OH_ImageSourceInfo_Release> sourceInfo;ImageCheck(OH_ImageSourceInfo_Create(&sourceInfo.value),"Create button image metadata");if(!sourceInfo.value)throw std::runtime_error("ImageKit returned no button metadata");
 ImageCheck(OH_ImageSourceNative_GetImageInfo(state->source.value,0,sourceInfo.value),"Read button image metadata");uint32_t w=0,h=0;ImageCheck(OH_ImageSourceInfo_GetWidth(sourceInfo.value,&w),"Read button width");ImageCheck(OH_ImageSourceInfo_GetHeight(sourceInfo.value,&h),"Read button height");
 if(!w||!h||w>8192||h>8192||uint64_t(w)*h>16u*1024u*1024u)throw std::invalid_argument("Button image dimensions exceed the native control budget");
 ImageOwner<OH_DecodingOptions,OH_DecodingOptions_Release> options;ImageCheck(OH_DecodingOptions_Create(&options.value),"Create button decode options");if(!options.value)throw std::runtime_error("ImageKit returned no button decode options");
 ImageCheck(OH_DecodingOptions_SetPixelFormat(options.value,PIXEL_FORMAT_RGBA_8888),"Select RGBA button pixels");
 ImageCheck(OH_ImageSourceNative_CreatePixelmap(state->source.value,options.value,&state->pixelmap.value),"Decode button pixels");if(!state->pixelmap.value)throw std::runtime_error("ImageKit returned no button pixels");
 ImageOwner<OH_Pixelmap_ImageInfo,OH_PixelmapImageInfo_Release> pixelInfo;ImageCheck(OH_PixelmapImageInfo_Create(&pixelInfo.value),"Create decoded button metadata");if(!pixelInfo.value)throw std::runtime_error("ImageKit returned no decoded button metadata");
 ImageCheck(OH_PixelmapNative_GetImageInfo(state->pixelmap.value,pixelInfo.value),"Read decoded button metadata");uint32_t pw=0,ph=0;ImageCheck(OH_PixelmapImageInfo_GetWidth(pixelInfo.value,&pw),"Read decoded button width");ImageCheck(OH_PixelmapImageInfo_GetHeight(pixelInfo.value,&ph),"Read decoded button height");if(pw!=w||ph!=h)throw std::runtime_error("ImageKit changed the requested button dimensions");
 state->drawable=OH_ArkUI_DrawableDescriptor_CreateFromPixelMap(state->pixelmap.value);if(!state->drawable)throw std::runtime_error("ArkUI returned no button drawable");
 return std::make_shared<PlatformImage>(w,h,std::move(state));
}
void WriteArkUiButtonImage(ArkUI_NativeNodeAPI_1& api,ArkUI_NodeHandle node,const ImageRef& image){
 if(!image){if(api.resetAttribute(node,NODE_IMAGE_SRC)!=0)throw std::runtime_error("ArkUI could not clear the button image");return;}
 auto* platform=dynamic_cast<const PlatformImage*>(image.get());if(!platform)throw std::invalid_argument("Button image belongs to a different native backend");
 ArkUI_AttributeItem item{};item.object=platform->state->drawable;if(api.setAttribute(node,NODE_IMAGE_SRC,&item)!=0)throw std::runtime_error("ArkUI could not display the decoded button image");
}
void DispatchNativeButtonPointer(ArkUI_NodeEvent* event,ArkUI_NodeHandle node,const std::function<void(const NodeEvent&)>& receiver){
 auto* input=OH_ArkUI_NodeEvent_GetInputEvent(event);if(!input)throw std::runtime_error("Native pointer event has no input payload");
 const auto type=OH_ArkUI_UIInputEvent_GetType(input);const auto action=OH_ArkUI_UIInputEvent_GetAction(input);PointerEvent pointer;
 if(type==ARKUI_UIINPUTEVENT_TYPE_TOUCH){
  switch(action){case UI_TOUCH_EVENT_ACTION_DOWN:pointer.phase=PointerPhase::Down;break;case UI_TOUCH_EVENT_ACTION_UP:pointer.phase=PointerPhase::Up;break;case UI_TOUCH_EVENT_ACTION_MOVE:pointer.phase=PointerPhase::Move;break;case UI_TOUCH_EVENT_ACTION_CANCEL:pointer.phase=PointerPhase::Cancel;break;default:return;}
 }else if(type==ARKUI_UIINPUTEVENT_TYPE_MOUSE){
  switch(action){case UI_MOUSE_EVENT_ACTION_PRESS:pointer.phase=PointerPhase::Down;break;case UI_MOUSE_EVENT_ACTION_RELEASE:pointer.phase=PointerPhase::Up;break;case UI_MOUSE_EVENT_ACTION_MOVE:pointer.phase=PointerPhase::Move;break;case UI_MOUSE_EVENT_ACTION_CANCEL:pointer.phase=PointerPhase::Cancel;break;default:return;}
  const auto button=OH_ArkUI_MouseEvent_GetMouseButton(input);if((pointer.phase==PointerPhase::Down||pointer.phase==PointerPhase::Up)&&button!=UI_MOUSE_EVENT_BUTTON_LEFT)return;
 }else return;
 const auto count=OH_ArkUI_PointerEvent_GetPointerCount(input);
 // These are single-pointer buttons. Additional contacts cancel an active press
 // instead of inventing a changed-pointer id that only API15 can supply.
 if(pointer.phase!=PointerPhase::Cancel&&count!=1)pointer.phase=PointerPhase::Cancel;
 if(pointer.phase!=PointerPhase::Cancel){pointer.id=OH_ArkUI_PointerEvent_GetPointerId(input,0);pointer.x=OH_ArkUI_PointerEvent_GetX(input);pointer.y=OH_ArkUI_PointerEvent_GetY(input);pointer.windowX=OH_ArkUI_PointerEvent_GetWindowX(input);pointer.windowY=OH_ArkUI_PointerEvent_GetWindowY(input);ArkUI_IntSize size{};if(OH_ArkUI_NodeUtils_GetLayoutSize(node,&size)!=0||size.width<=0||size.height<=0)throw std::runtime_error("Native button has no measured pointer area");pointer.inside=std::isfinite(pointer.x)&&std::isfinite(pointer.y)&&pointer.x>=0&&pointer.y>=0&&pointer.x<size.width&&pointer.y<size.height;}
 NodeEvent converted{EventKind::Pointer};converted.pointer=&pointer;receiver(converted);
 // A callback may detach the view. Do not touch its node or record afterwards.
}
}
