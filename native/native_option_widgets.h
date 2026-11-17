#pragma once
#include <cstddef>
using Olex2NativeOptionsChange=int(*)(void*,int,bool);
extern "C" {
// Private ordinary form-control factory. Mounts genuine original choice and
// checkbox classes in the supplied NodeContent; it executes no domain command.
void* olex2_native_options_mount(void*,const char* const*,std::size_t,int,bool,bool,Olex2NativeOptionsChange,void*,char*,std::size_t);
int olex2_native_options_update(void*,int,bool,bool,char*,std::size_t);
int olex2_native_options_unmount(void*,char*,std::size_t);
}
