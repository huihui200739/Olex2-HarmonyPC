#include "native_input_widgets.h"
#include "native_editable.h"
#include "spinctrlext.h"
#include "comboext.h"
#include "listboxext.h"
#include "texteditext.h"
#include "eutf8.h"
#include <arkui/native_node.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace olex2::harmony::ui;
namespace {
void Error(char* output,std::size_t capacity,const char* message)noexcept{if(output&&capacity){auto length=std::min(capacity-1,std::strlen(message));std::memcpy(output,message,length);output[length]='\0';}}
olxstr Decode(const char* text){if(!text)throw std::invalid_argument("Native input text is absent");return TUtf8::Decode(text,std::strlen(text));}
void Selection(int value,std::size_t count){if(value < -1||(value>=0&&static_cast<std::size_t>(value)>=count))throw std::out_of_range("Native input selection is outside its items");}
void Validate(const Olex2NativeInputValues* values,std::size_t count){
  if(!values)throw std::invalid_argument("Native input values are absent");
  if(values->spinMinimum>values->spinMaximum||values->spinValue<values->spinMinimum||values->spinValue>values->spinMaximum)throw std::out_of_range("Native spin value/range is invalid");
  Selection(values->comboSelection,count);Selection(values->listSelection,count);Decode(values->text);Decode(values->comboText);
}
struct InputView;
struct Forward final:AActionHandler {InputView* owner;explicit Forward(InputView* value):owner(value){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override;};
struct InputView {
  // Original action queues cannot outlive their handler.
  Forward forward;
  Widget column;
  ctrl_ext::TSpinCtrl spin;
  ctrl_ext::TComboBox combo;
  ctrl_ext::TListBox list;
  ctrl_ext::TTextEdit text;
  ArkUI_NodeContentHandle content=nullptr;
  Olex2NativeInputChange callback=nullptr;void* context=nullptr;
  bool attached=false;std::thread::id thread=std::this_thread::get_id();
  InputView(const char* const* items,std::size_t count,const Olex2NativeInputValues& values,Olex2NativeInputChange cb,void* data)
    :forward(this),column(ArkUiBackend(),NodeKind::Column),spin(ArkUiBackend(),&column,-1,"0",{},Size{180,32}),
     combo(ArkUiBackend(),&column,-1,EmptyString(),{},Size{320,32}),list(ArkUiBackend(),&column,-1,{},Size{320,160}),
     text(ArkUiBackend(),&column,-1,EmptyString(),{},Size{320,values.multiline?80:40},values.multiline?InputMultiline:0),callback(cb),context(data){
    column.SetWidth(340);column.SetHeight(values.multiline?328:288);column.SetNumber(Property::RowSpacing,8);
    for(std::size_t i=0;i<count;++i){auto label=Decode(items[i]);combo.AddObject(label);list.AddObject(label);}
    combo.Widget::SetText(TextProperty::Id,"olex2-original-input-combo");list.Widget::SetText(TextProperty::Id,"olex2-original-input-list");text.Widget::SetText(TextProperty::Id,"olex2-original-input-text");spin.Widget::SetText(TextProperty::Id,"olex2-original-input-spin");
    combo.Widget::SetText(TextProperty::Accessibility,"原生组合框");list.Widget::SetText(TextProperty::Accessibility,"原生列表");text.Widget::SetText(TextProperty::Accessibility,"原生文字输入");spin.Widget::SetText(TextProperty::Accessibility,"原生整数输入");
    Apply(values);spin.OnChange.Add(&forward);combo.OnChange.Add(&forward);list.OnSelect.Add(&forward);text.OnChange.Add(&forward);
  }
  void Apply(const Olex2NativeInputValues& values){
    if(values.multiline!=text.IsMultiLine())throw std::invalid_argument("Changing native multiline kind requires a new input view");
    spin.SetRange(values.spinMinimum,values.spinMaximum);spin.SetValue(values.spinValue);
    combo.SetReadOnly(values.readOnly);text.SetReadOnly(values.readOnly);
    if(values.comboSelection>=0)combo.SetText(combo.GetString(static_cast<std::size_t>(values.comboSelection)));else combo.SetText(Decode(values.comboText));
    list.SetSelection(values.listSelection);text.SetText(Decode(values.text));
    column.Enable(values.enabled);spin.Enable(values.enabled);combo.Enable(values.enabled);list.Enable(values.enabled);text.Enable(values.enabled);
  }
  void Pin(const std::shared_ptr<InputView>& self){column.SetEventLifetime(self);}
  void CheckThread()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Native input owner requires its creating UI thread");}
  void CheckErrors()const{if(!spin.LastEventError().empty()||!combo.LastEventError().empty()||!list.LastEventError().empty()||!text.LastEventError().empty())throw std::runtime_error("An original input callback reported an error");}
};
bool Forward::Execute(const IOlxObject* sender,const IOlxObject*,TActionQueue*){
  if(!owner->attached||!owner->callback)return true;
  int kind=0,value=-1;olxstr text;
  if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->spin)){kind=Olex2InputSpin;value=owner->spin.GetValue();text=olxstr(value);}
  else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->combo)){kind=Olex2InputCombo;value=owner->combo.GetSelection();text=owner->combo.GetText();}
  else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->list)){kind=Olex2InputList;value=owner->list.GetSelection();if(value>=0)text=owner->list.GetValue(static_cast<std::size_t>(value));}
  else if(sender==static_cast<ctrl_ext::AOlxCtrl*>(&owner->text)){kind=Olex2InputText;text=owner->text.GetText();}
  else throw std::runtime_error("Unknown original input action sender");
  auto utf8=TUtf8::Encode(text);auto callback=owner->callback;auto context=owner->context;
  if(callback(context,kind,utf8.c_str(),value)!=0)throw std::runtime_error("Native input callback failed");return true;
}
struct Registry {std::mutex mutex;uintptr_t next=0;std::map<void*,std::shared_ptr<InputView>> owners;};
Registry& Views(){static Registry value;return value;}
std::shared_ptr<InputView> Find(void* key){auto& registry=Views();std::lock_guard<std::mutex> lock(registry.mutex);auto found=registry.owners.find(key);if(found==registry.owners.end())throw std::invalid_argument("Native input owner is inactive");return found->second;}
}
extern "C" void* olex2_native_inputs_mount(void* content,const char* const* items,std::size_t count,const Olex2NativeInputValues* values,Olex2NativeInputChange callback,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!content||!callback||(count&&!items))throw std::invalid_argument("Native inputs need content, item storage and callback");if(count>=static_cast<std::size_t>(std::numeric_limits<int>::max()-1))throw std::length_error("Too many native input items");Validate(values,count);
    for(std::size_t i=0;i<count;++i)Decode(items[i]);
    auto view=std::make_shared<InputView>(items,count,*values,callback,context);view->Pin(view);view->content=static_cast<ArkUI_NodeContentHandle>(content);
    auto& registry=Views();void* key=nullptr;{std::lock_guard<std::mutex> lock(registry.mutex);if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::runtime_error("Native input owner sequence exhausted");key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,view);}
    if(OH_ArkUI_NodeContent_AddNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0){std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);throw std::runtime_error("ArkUI could not attach original input controls");}
    view->attached=true;return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native input construction failed");}return nullptr;
}
extern "C" int olex2_native_inputs_update(void* key,const Olex2NativeInputValues* values,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();Validate(values,view->combo.GetCount());view->CheckErrors();view->Apply(*values);return 0;}
  catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native input update failed");}return -1;
}
extern "C" int olex2_native_inputs_unmount(void* key,char* error,std::size_t capacity){
  try{Error(error,capacity,"");auto view=Find(key);view->CheckThread();if(OH_ArkUI_NodeContent_RemoveNode(view->content,static_cast<ArkUI_NodeHandle>(view->column.Handle()))!=0)throw std::runtime_error("Native input detach failed; owned nodes retained");view->attached=false;view->content=nullptr;auto& registry=Views();{std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);}return 0;}
  catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native input detach failed; owner retained");}return -1;
}
