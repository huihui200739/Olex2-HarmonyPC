#pragma once
#include <cstddef>
// Private ordinary text input interface. Submission is a callback, not a macro
// interpreter, application Python entry point or scientific calculation.
struct Olex2NativeCommandLineValues {
  const char* prompt=">>";
  const char* command="";
  int width=480;
  bool enabled=true,readOnly=false;
};
using Olex2NativeCommandSubmit=int(*)(void*,const char*);
extern "C" {
void* olex2_native_commandline_mount(void*,const Olex2NativeCommandLineValues*,Olex2NativeCommandSubmit,void*,char*,std::size_t);
int olex2_native_commandline_update(void*,const Olex2NativeCommandLineValues*,char*,std::size_t);
int olex2_native_commandline_unmount(void*,char*,std::size_t);
}
