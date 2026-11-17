#include "arkui_input_events.h"
#include <arkui/native_interface.h>
#include <arkui/native_key_event.h>
#include <arkui/ui_input_event.h>
#include <dlfcn.h>
#include <array>
#include <stdexcept>
#include <vector>
namespace olex2::harmony::ui {
namespace {
template<class T>T Symbol(const char* name){return reinterpret_cast<T>(dlsym(RTLD_DEFAULT,name));}
struct Keyboard {
  decltype(&OH_ArkUI_KeyEvent_GetType) type=Symbol<decltype(type)>("OH_ArkUI_KeyEvent_GetType");
  decltype(&OH_ArkUI_KeyEvent_GetKeyCode) code=Symbol<decltype(code)>("OH_ArkUI_KeyEvent_GetKeyCode");
  decltype(&OH_ArkUI_KeyEvent_GetUnicode) unicode=Symbol<decltype(unicode)>("OH_ArkUI_KeyEvent_GetUnicode");
  decltype(&OH_ArkUI_KeyEvent_GetKeyText) text=Symbol<decltype(text)>("OH_ArkUI_KeyEvent_GetKeyText");
  decltype(&OH_ArkUI_KeyEvent_SetConsumed) consume=Symbol<decltype(consume)>("OH_ArkUI_KeyEvent_SetConsumed");
  decltype(&OH_ArkUI_UIInputEvent_GetPressedKeys) pressed=Symbol<decltype(pressed)>("OH_ArkUI_UIInputEvent_GetPressedKeys");
  bool Available()const{return type&&code&&unicode&&text&&consume&&pressed;}
};
Keyboard& Keys(){static Keyboard value;return value;}
LogicalKey Logical(int key){switch(key){
 case ARKUI_KEYCODE_DPAD_UP:return LogicalKey::Up;case ARKUI_KEYCODE_DPAD_DOWN:return LogicalKey::Down;
 case ARKUI_KEYCODE_DPAD_LEFT:return LogicalKey::Left;case ARKUI_KEYCODE_DPAD_RIGHT:return LogicalKey::Right;
 case ARKUI_KEYCODE_MOVE_HOME:return LogicalKey::Home;case ARKUI_KEYCODE_MOVE_END:return LogicalKey::End;
 case ARKUI_KEYCODE_NUMPAD_ENTER:case ARKUI_KEYCODE_ENTER:return LogicalKey::Enter;case ARKUI_KEYCODE_SPACE:return LogicalKey::Space;
 case ARKUI_KEYCODE_ESCAPE:return LogicalKey::Escape;case ARKUI_KEYCODE_TAB:return LogicalKey::Tab;
 case ARKUI_KEYCODE_PAGE_UP:return LogicalKey::PageUp;case ARKUI_KEYCODE_PAGE_DOWN:return LogicalKey::PageDown;
 case ARKUI_KEYCODE_DEL:return LogicalKey::Backspace;case ARKUI_KEYCODE_FORWARD_DEL:return LogicalKey::Delete;
 default:return LogicalKey::Other;
}}
void Need(int32_t value,const char* what){if(value)throw std::runtime_error(std::string("ArkUI input ")+what+" failed ("+std::to_string(value)+")");}
}
bool NativeKeyboardAvailable(){return Keys().Available();}
void RequireNativeKeyboard(){if(!Keys().Available())throw std::runtime_error("Original native input controls require the platform's API14 hardware-key interface; API12 base widgets remain available");}
void DispatchNativeKeyboard(ArkUI_NodeEvent* event,const std::function<void(const NodeEvent&)>& receiver)noexcept{
  try{
    if(!receiver||!event)return;RequireNativeKeyboard();auto* input=OH_ArkUI_NodeEvent_GetInputEvent(event);if(!input)throw std::runtime_error("Native keyboard event has no input payload");
    auto& api=Keys();KeyEvent key;key.code=api.code(input);key.logical=Logical(key.code);key.down=api.type(input)==ARKUI_KEY_EVENT_DOWN;key.unicode=api.unicode(input);if(auto* text=api.text(input))key.text=text;
    std::vector<int32_t> held(128);int32_t count=static_cast<int32_t>(held.size());int32_t result=api.pressed(input,held.data(),&count);
    if(result==ARKUI_ERROR_CODE_BUFFER_SIZE_NOT_ENOUGH&&count>static_cast<int32_t>(held.size())&&count<=4096){held.resize(static_cast<size_t>(count));result=api.pressed(input,held.data(),&count);}
    Need(result,"pressed-key state");if(count<0||static_cast<size_t>(count)>held.size())throw std::runtime_error("Invalid native pressed-key count");
    for(int32_t i=0;i<count;++i){switch(held[static_cast<size_t>(i)]){
      case ARKUI_KEYCODE_SHIFT_LEFT:case ARKUI_KEYCODE_SHIFT_RIGHT:key.shift=true;break;
      case ARKUI_KEYCODE_CTRL_LEFT:case ARKUI_KEYCODE_CTRL_RIGHT:key.control=true;break;
      case ARKUI_KEYCODE_ALT_LEFT:case ARKUI_KEYCODE_ALT_RIGHT:key.alt=true;break;
      case ARKUI_KEYCODE_META_LEFT:case ARKUI_KEYCODE_META_RIGHT:key.meta=true;break;default:break;
    }}
    NodeEvent converted{EventKind::Key};converted.key=&key;receiver(converted);
    // Callback may have released its owner. Only local event/API state is used.
    if(key.handled)api.consume(input,true);
  }catch(const std::exception& e){try{if(receiver){NodeEvent error{EventKind::BackendError};error.text=e.what();receiver(error);}}catch(...){} }
  catch(...){try{if(receiver){NodeEvent error{EventKind::BackendError};error.text="Native keyboard conversion failed";receiver(error);}}catch(...){} }
}
ArkUiDoubleClick::ArkUiDoubleClick(ArkUI_NodeHandle node,std::function<void()> receiver):node_(node),receiver_(std::move(receiver)){
  OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_GESTURE,ArkUI_NativeGestureAPI_1,api_);if(!api_)throw std::runtime_error("ArkUI native double-tap API12 is unavailable");
  gesture_=api_->createTapGesture(2,1);if(!gesture_)throw std::runtime_error("ArkUI double-tap creation failed");
  try{Need(api_->setGestureEventTarget(gesture_,GESTURE_EVENT_ACTION_ACCEPT,this,Receive),"double-tap receiver");Need(api_->addGestureToNode(node_,gesture_,PARALLEL,NORMAL_GESTURE_MASK),"double-tap attachment");}
  catch(...){api_->dispose(gesture_);gesture_=nullptr;throw;}
}
ArkUiDoubleClick::~ArkUiDoubleClick(){
  if(!gesture_)return;
  // Retarget first: even a platform detach failure must not retain a callback
  // into this C++ owner. Do not dispose a recognizer still attached to a node.
  api_->setGestureEventTarget(gesture_,GESTURE_EVENT_ACTION_ACCEPT,nullptr,Ignore);
  if(api_->removeGestureFromNode(node_,gesture_)==0)api_->dispose(gesture_);
}
void ArkUiDoubleClick::Receive(ArkUI_GestureEvent*,void* extra)noexcept{try{if(!extra)return;auto callback=static_cast<ArkUiDoubleClick*>(extra)->receiver_;if(callback)callback();}catch(...){} }
}
