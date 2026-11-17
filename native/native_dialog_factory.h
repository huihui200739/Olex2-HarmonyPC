#pragma once
#include <cstddef>
#include <cstdint>
struct Olex2NativeDialogLayout {
  const char* title=nullptr;int width=480,height=320,left=0,top=0;bool centered=true;
};
struct Olex2NativeDialogValues {
  Olex2NativeDialogLayout layout;
  const char* className="NativeDialog";bool resizable=true,messageBox=false;
  const char* message="";const char* rememberText="Remember my decision";
  uint32_t messageFlags=1u|(1u<<7);bool showRemember=false,rememberChecked=false;
  long timeoutSeconds=0;int timeoutResult=0;
};
// UTF-8 code is valid only during this callback: O/Y/N/C and optional R prefix.
// Basic dialogs return an empty code. Modeless dialogs have no modal result.
using Olex2NativeDialogCompleted=int(*)(void* context,int result,const char* code);
extern "C" {
// Creates the real original update-options window; reuse dialog show/close/release.
void* olex2_native_update_options_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
void* olex2_native_bond_length_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_bond_length_result(void* handle,double* length,int* applied,char* error,std::size_t);
// Original generation-options dialog (Structure -> Generate...). Values are read
// by the caller before release: [accepted, AFrom, ATo, BFrom, BTo, CFrom, CTo].
void* olex2_native_generate_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_generate_values(void* owner,double* values,char* error,std::size_t);
// Original periodic-table dialog (atom type "More..."). Selecting an element
// ends the modal dialog; its symbol is read before release.
void* olex2_native_periodic_table_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_periodic_table_symbol(void* owner,char* symbol,std::size_t symbolCapacity,char* error,std::size_t);
// Original material editor (Graphics -> Draw style...). On OK the applied
// material is set on the current selection; result reports signature/applied.
void* olex2_native_material_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_material_result(void* owner,char* signature,std::size_t capacity,int* applied,int* sceneChanged,char* error,std::size_t);
// Original scene-properties dialog (Graphics -> Draw style... Scene settings).
// OK applies and closes; the result reports whether the renderer light model
// changed.
void* olex2_native_scene_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_scene_result(void* owner,char* signature,std::size_t capacity,int* sceneChanged,char* error,std::size_t);
// Original background gradient dialog. On OK the colours are applied through
// the runtime and the window closes.
void* olex2_native_gradient_create(void* napiEnvironment,void* runtime,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_gradient_result(void* owner,int* applied,int* uncertain,char* error,std::size_t);
// Original primitives dialog for the current selection. OK returns the mask
// and apply scope; the result call applies them through TGXApp::Individualise.
void* olex2_native_primitive_create(void* napiEnvironment,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_primitive_result(void* owner,int* mask,int* level,int* applied,char* error,std::size_t);
void* olex2_native_dialog_create(void* napiEnvironment,const Olex2NativeDialogValues*,Olex2NativeDialogCompleted,void* context,char* error,std::size_t);
int olex2_native_dialog_show(void* owner,bool modal,char* error,std::size_t);
int olex2_native_dialog_update(void* owner,const Olex2NativeDialogLayout*,char* error,std::size_t);
int olex2_native_dialog_close(void* owner,int result,char* error,std::size_t);
// 0: released after actual detach (or never shown); 1: close pending, retain
// the handle and callback context; -1: error, owner still retained for retry.
int olex2_native_dialog_release(void* owner,char* error,std::size_t);
}
