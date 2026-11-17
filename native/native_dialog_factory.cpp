#include "updateoptions.h"
#include "edit.h"
#include "dgenopt.h"
#include "ptable.h"
#include "matprop.h"
#include "scenep.h"
#include "dgrad.h"
#include "primtvs.h"
#include "xatom.h"
#include "xbond.h"
#include "xline.h"
#include "gxapp.h"
#include "glgroup.h"
#include "native_dialog_factory.h"
#include "ctrls.h"
#include "msgbox.h"
#include "eutf8.h"
#include "native_text_positions.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace {
void Error(char* buffer,std::size_t capacity,const char* text)noexcept{if(buffer&&capacity){auto n=std::min(capacity-1,std::strlen(text));std::memcpy(buffer,text,n);buffer[n]='\0';}}
olxstr Decode(const char* text){if(!text)throw std::invalid_argument("Native dialog text is absent");auto result=TUtf8::Decode(text,std::strlen(text));TextUtf16Offset(result,result.Length());return result;}
DialogGeometry Geometry(const Olex2NativeDialogLayout& value){Decode(value.title);if(value.width<240||value.width>16384||value.height<140||value.height>16384)throw std::out_of_range("Dialog dimensions must be 240..16384 by 140..16384");return {{value.left,value.top},{value.width,value.height},value.centered};}
std::shared_ptr<DialogPositionStore> Positions(){static thread_local auto value=std::make_shared<DialogPositionStore>();return value;}
struct Owner:std::enable_shared_from_this<Owner> {
  std::thread::id thread=std::this_thread::get_id();
  std::shared_ptr<UiScheduler> scheduler;std::shared_ptr<DialogBackend> backend;
  std::unique_ptr<TDialog> dialog;TdlgMsgBox* message=nullptr;
  Olex2NativeDialogCompleted completed=nullptr;void* context=nullptr;
  bool materialApplied=false;std::string materialInitial,materialValue;
  std::string sceneInitial;
  void Check()const{if(thread!=std::this_thread::get_id())throw std::runtime_error("Dialog owner requires its creating UI thread");}
  void Apply(const Olex2NativeDialogLayout& value){Check();auto geometry=Geometry(value);dialog->SetTitle(std::string(value.title));dialog->SetPosition(geometry.offset,geometry.centered);if(message)message->SetSize(geometry.size);else dialog->SetSize(geometry.size);}
  void Show(bool modal){
    Check();if(!modal){if(message)message->Show();else dialog->Show();return;}
    std::weak_ptr<Owner> weak=shared_from_this();auto finish=[weak](int result){auto self=weak.lock();if(!self||!self->completed)return;olxcstr bytes;if(self->message)bytes=TUtf8::Encode(self->message->Result(result));if(self->completed(self->context,result,bytes.c_str())!=0)throw std::runtime_error("Native dialog receiver rejected the actual modal result");};
    if(message)message->ShowModalEx(true,std::move(finish));else dialog->ShowModalEx(true,std::move(finish));
  }
};
struct Registry{std::mutex mutex;std::map<void*,std::shared_ptr<Owner>> owners;uintptr_t next=0;};
Registry& Owners(){static auto* registry=new Registry;return *registry;}
std::shared_ptr<Owner> Find(void* key){auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);auto i=registry.owners.find(key);if(i==registry.owners.end())throw std::invalid_argument("Unknown native dialog owner");return i->second;}
}
extern "C" void* olex2_native_dialog_create(void* environment,const Olex2NativeDialogValues* value,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment||!value)throw std::invalid_argument("Dialog environment and values are required");auto geometry=Geometry(value->layout);auto title=Decode(value->layout.title);auto name=Decode(value->className);auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    if(value->messageBox){auto message=std::make_unique<TdlgMsgBox>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,Decode(value->message),title,Decode(value->rememberText),value->messageFlags,value->showRemember,value->timeoutSeconds,value->timeoutResult,Positions());owner->message=message.get();message->SetChecked(value->rememberChecked);owner->dialog=std::move(message);owner->Apply(value->layout);}
    else owner->dialog=std::make_unique<TDialog>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,title,name,geometry,Positions(),value->resizable);
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native dialog creation failed");}return nullptr;
}
extern "C" int olex2_native_dialog_show(void* key,bool modal,char* error,std::size_t capacity){try{Error(error,capacity,"");Find(key)->Show(modal);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native dialog show failed");}return -1;}
extern "C" int olex2_native_dialog_update(void* key,const Olex2NativeDialogLayout* value,char* error,std::size_t capacity){try{Error(error,capacity,"");if(!value)throw std::invalid_argument("Native dialog layout is absent");auto owner=Find(key);owner->Check();if(owner->dialog->IsClosing())throw std::logic_error("Dialog is still closing");owner->Apply(*value);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native dialog update failed");}return -1;}
extern "C" int olex2_native_dialog_close(void* key,int result,char* error,std::size_t capacity){try{Error(error,capacity,"");auto owner=Find(key);owner->Check();if(owner->dialog->IsClosing())owner->dialog->RetryClose();else owner->dialog->EndModal(result);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native dialog close failed; owner retained");}return -1;}
extern "C" int olex2_native_dialog_release(void* key,char* error,std::size_t capacity){try{Error(error,capacity,"");auto owner=Find(key);owner->Check();if(owner->dialog->IsShown()||owner->dialog->IsClosing()){if(owner->dialog->IsClosing())owner->dialog->RetryClose();else owner->dialog->EndModal(DialogCancelled);if(owner->dialog->IsShown()||owner->dialog->IsClosing())return 1;}auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);registry.owners.erase(key);return 0;}catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Native dialog release failed; owner retained");}return -1;}

extern "C" void* olex2_native_generate_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Generate dialog requires its UI environment");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->dialog=std::make_unique<TdlgGenerate>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,Positions());
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original generation dialog failed");}
  return nullptr;
}
extern "C" int olex2_native_generate_values(void* key,double* values,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!values)throw std::invalid_argument("Generate values destination is absent");
    auto owner=Find(key);auto* dialog=dynamic_cast<TdlgGenerate*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a generation dialog");
    values[0]=dialog->ResultAccepted()?1:0;values[1]=dialog->GetAFrom();values[2]=dialog->GetATo();
    values[3]=dialog->GetBFrom();values[4]=dialog->GetBTo();values[5]=dialog->GetCFrom();values[6]=dialog->GetCTo();
    return 0;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Generate values read failed");}
  return -1;
}
extern "C" void* olex2_native_periodic_table_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Periodic table requires its UI environment");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->dialog=std::make_unique<TPTableDlg>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,Positions());
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original periodic table failed");}
  return nullptr;
}
extern "C" int olex2_native_periodic_table_symbol(void* key,char* symbol,std::size_t capacity,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");if(!symbol||capacity==0)throw std::invalid_argument("Periodic table symbol destination is absent");
    symbol[0]='\0';auto owner=Find(key);auto* dialog=dynamic_cast<TPTableDlg*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a periodic table");
    cm_Element* element=dialog->GetSelected();if(!element)return 0;
    const std::string text(element->symbol.c_str());const auto length=std::min(capacity-1,text.size());
    std::memcpy(symbol,text.data(),length);symbol[length]='\0';return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Periodic table symbol read failed");}
  return -1;
}
extern "C" void* olex2_native_material_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Material editor requires its UI environment");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    glObj::TGlMaterial current;
    {auto& stream=TGXApp::GetInstance().GetRenderer().GetSelection();
     for(size_t i=0;i<stream.Count();++i)if(auto* group=dynamic_cast<TGlGroup*>(&stream[i])){current=group->GetGlM();break;}}
    owner->materialInitial=std::string(TUtf8::Encode(current.ToString()).c_str());
    Owner* raw=owner.get();
    owner->dialog=std::make_unique<TdlgMatProp>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,current,
      [raw](const glObj::TGlMaterial& material){
        raw->materialApplied=true;raw->materialValue=std::string(TUtf8::Encode(material.ToString()).c_str());
        auto& stream=TGXApp::GetInstance().GetRenderer().GetSelection();
        for(size_t i=0;i<stream.Count();++i)if(auto* group=dynamic_cast<TGlGroup*>(&stream[i]))group->SetGlM(material);
      },Positions());
    // The editor's default window is taller than this device's usable height,
    // which leaves its Cancel/OK footer beyond the bottom edge. Use a modest
    // centered size that fits the screen; the content scrolls internally.
    owner->dialog->SetPosition(Point{0,0},true);owner->dialog->SetSize(Size{640,700});
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original material editor failed");}
  return nullptr;
}
extern "C" int olex2_native_material_result(void* key,char* signature,std::size_t capacity,int* applied,int* sceneChanged,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");if(!signature||capacity==0)throw std::invalid_argument("Material signature destination is absent");
    signature[0]='\0';auto owner=Find(key);auto* dialog=dynamic_cast<TdlgMatProp*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a material editor");
    const std::string current(TUtf8::Encode(dialog->GetCurrent().ToString()).c_str());
    const auto length=std::min(capacity-1,current.size());std::memcpy(signature,current.data(),length);signature[length]='\0';
    if(applied)*applied=owner->materialApplied?1:0;
    std::string scene;
    {auto& stream=TGXApp::GetInstance().GetRenderer().GetSelection();
     for(size_t i=0;i<stream.Count();++i)if(auto* group=dynamic_cast<TGlGroup*>(&stream[i])){scene=std::string(TUtf8::Encode(group->GetGlM().ToString()).c_str());break;}}
    if(sceneChanged)*sceneChanged=(owner->materialApplied&&scene!=owner->materialInitial)?1:0;
    return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Material result read failed");}
  return -1;
}
std::string SceneSignature(const TGlLightModel& model){
  std::ostringstream o;o<<(model.IsLocalViewer()?1:0)<<(model.IsTwoSides()?1:0)<<(model.IsSmoothShade()?1:0);
  const auto& ambient=model.GetAmbientColor();const auto& clear=model.GetClearColor();
  for(int i=0;i<4;++i)o<<','<<ambient[i];
  for(int i=0;i<4;++i)o<<','<<clear[i];
  return o.str();
}
extern "C" void* olex2_native_scene_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Scene settings require their UI environment");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->sceneInitial=SceneSignature(TGXApp::GetInstance().GetRenderer().LightModel);
    owner->dialog=std::make_unique<TdlgSceneProps>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,Positions());
    owner->dialog->SetPosition(Point{0,0},true);owner->dialog->SetSize(Size{720,760});
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original scene settings failed");}
  return nullptr;
}
extern "C" int olex2_native_scene_result(void* key,char* signature,std::size_t capacity,int* sceneChanged,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");if(!signature||capacity==0)throw std::invalid_argument("Scene signature destination is absent");
    signature[0]='\0';auto owner=Find(key);auto* dialog=dynamic_cast<TdlgSceneProps*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a scene settings dialog");
    const std::string current=SceneSignature(TGXApp::GetInstance().GetRenderer().LightModel);
    const auto length=std::min(capacity-1,current.size());std::memcpy(signature,current.data(),length);signature[length]='\0';
    if(sceneChanged)*sceneChanged=current!=owner->sceneInitial?1:0;
    return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Scene result read failed");}
  return -1;
}
extern "C" void* olex2_native_gradient_create(void* environment,void* runtime,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment||!runtime)throw std::invalid_argument("Gradient dialog requires its UI environment and runtime");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->dialog=std::make_unique<TdlgGradient>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,*static_cast<olex2::harmony::ApplicationRuntime*>(runtime),Positions());
    owner->dialog->SetPosition(Point{0,0},true);owner->dialog->SetSize(Size{720,700});
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original background dialog failed");}
  return nullptr;
}
extern "C" int olex2_native_gradient_result(void* key,int* applied,int* uncertain,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");auto owner=Find(key);auto* dialog=dynamic_cast<TdlgGradient*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a background gradient dialog");
    if(applied)*applied=dialog->ColorsApplied()?1:0;
    if(uncertain)*uncertain=dialog->BackgroundMayHaveChanged()?1:0;
    return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Gradient result read failed");}
  return -1;
}
extern "C" void* olex2_native_primitive_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Primitives dialog requires its UI environment");
    auto& selection=TGXApp::GetInstance().GetRenderer().GetSelection();
    if(selection.Count()==0)throw std::invalid_argument("Select an original graphics object first");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->dialog=std::make_unique<TdlgPrimitive>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,selection[0],Positions());
    owner->dialog->SetPosition(Point{0,0},true);owner->dialog->SetSize(Size{560,620});
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original primitives dialog failed");}
  return nullptr;
}
extern "C" int olex2_native_primitive_result(void* key,int* mask,int* level,int* applied,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");auto owner=Find(key);auto* dialog=dynamic_cast<TdlgPrimitive*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a primitives dialog");
    if(mask)*mask=dialog->Mask;
    if(level)*level=dialog->Level;
    if(!dialog->ResultAccepted()){if(applied)*applied=0;return 0;}
    auto& app=TGXApp::GetInstance();auto& selection=app.GetRenderer().GetSelection();
    TXAtomPList atoms;TXBondPList bonds;
    for(size_t i=0;i<selection.Count();++i){
      if(selection[i].Is<TXAtom>())atoms.Add(static_cast<TXAtom&>(selection[i]));
      else if(selection[i].Is<TXBond>())bonds.Add(static_cast<TXBond&>(selection[i]));
    }
    if(!atoms.IsEmpty())app.Individualise(atoms,dialog->Level,dialog->Mask);
    else if(!bonds.IsEmpty())app.Individualise(bonds,dialog->Level,dialog->Mask);
    else throw std::invalid_argument("Selection has no atom or bond for primitive application");
    if(applied)*applied=1;
    return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Primitive result read failed");}
  return -1;
}
namespace {
TXLine* BondLengthTarget(TGXApp& app){
  auto& selection=app.GetRenderer().GetSelection();
  for(size_t i=0;i<selection.Count();++i)if(selection[i].Is<TXLine>())return reinterpret_cast<TXLine*>(&selection[i]);
  if(app.LineCount()>0)return &app.GetLines()[0];
  return nullptr;
}
}
extern "C" void* olex2_native_bond_length_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Bond length dialog requires its UI environment");
    TXLine* bond=BondLengthTarget(TGXApp::GetInstance());
    if(!bond)throw std::invalid_argument("Create or select an original line (TXLine) first");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    auto dialog=std::make_unique<TdlgEdit>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,false,Positions());
    dialog->SetTitle("Please input the line length");
    dialog->SetText(bond->GetLength());
    owner->dialog=std::move(dialog);
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception& e){Error(error,capacity,e.what());}catch(...){Error(error,capacity,"Original bond length dialog failed");}
  return nullptr;
}
extern "C" int olex2_native_bond_length_result(void* key,double* length,int* applied,char* error,std::size_t errorCapacity){
  try{
    Error(error,errorCapacity,"");auto owner=Find(key);auto* dialog=dynamic_cast<TdlgEdit*>(owner->dialog.get());
    if(!dialog)throw std::invalid_argument("Native dialog is not a bond length dialog");
    if(!dialog->ResultAccepted()){if(applied)*applied=0;return 0;}
    const olxstr text=dialog->GetText();const double value=text.ToDouble();
    if(!(value>0)||!(value<1000))throw std::invalid_argument("Bond length must be a positive number below 1000");
    auto& app=TGXApp::GetInstance();
    TXLine* bond=BondLengthTarget(app);
    if(!bond)throw std::invalid_argument("Create or select an original line (TXLine) first");
    bond->SetLength(value);bond->UpdateLabel();
    if(length)*length=value;
    if(applied)*applied=1;
    return 0;
  }catch(const std::exception& e){Error(error,errorCapacity,e.what());}catch(...){Error(error,errorCapacity,"Bond length result read failed");}
  return -1;
}
extern "C" void* olex2_native_update_options_create(void* environment,Olex2NativeDialogCompleted completed,void* context,char* error,std::size_t capacity){
  try{
    Error(error,capacity,"");if(!environment)throw std::invalid_argument("Update options require their UI environment");
    auto owner=std::make_shared<Owner>();owner->scheduler=CreateNapiUiScheduler(environment);owner->backend=CreateArkUiDialogBackend(owner->scheduler);owner->completed=completed;owner->context=context;
    owner->dialog=std::make_unique<TdlgUpdateOptions>(ArkUiBackend(),owner->backend,owner->scheduler,nullptr,CreateOriginalUpdateOptionsStore(),Positions());
    auto& registry=Owners();std::lock_guard<std::mutex> lock(registry.mutex);
    if(registry.next==std::numeric_limits<uintptr_t>::max())throw std::overflow_error("Native dialog handles exhausted");
    auto key=reinterpret_cast<void*>(++registry.next);registry.owners.emplace(key,std::move(owner));return key;
  }catch(const std::exception&){Error(error,capacity,"Original update options could not be opened; existing settings are retained");}catch(...){Error(error,capacity,"Original update options creation failed");}
  return nullptr;
}
