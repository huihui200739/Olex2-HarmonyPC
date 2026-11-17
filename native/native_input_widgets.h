#pragma once
#include <cstddef>
// Private native form interface; no document loading, commands or calculations.
struct Olex2NativeInputValues {
  int spinValue=0,spinMinimum=0,spinMaximum=100;
  int comboSelection=-1,listSelection=-1;
  const char* comboText="";
  const char* text="";
  bool enabled=true,readOnly=false,multiline=false;
};
enum Olex2NativeInputKind {Olex2InputSpin=1,Olex2InputCombo=2,Olex2InputList=3,Olex2InputText=4};
// UTF-8 text is valid only for this synchronous callback. The integer is the
// spin value, the selected index, or -1 for the text editor. Nonzero is an error.
using Olex2NativeInputChange=int(*)(void*,int,const char*,int);
extern "C" {
void* olex2_native_inputs_mount(void*,const char* const*,std::size_t,const Olex2NativeInputValues*,Olex2NativeInputChange,void*,char*,std::size_t);
int olex2_native_inputs_update(void*,const Olex2NativeInputValues*,char*,std::size_t);
int olex2_native_inputs_unmount(void*,char*,std::size_t);
}
