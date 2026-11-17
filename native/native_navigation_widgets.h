#pragma once
#include <cstddef>
struct Olex2NativeNavigationValues {
 const char* const* lines;
 std::size_t lineCount;
 const char* contextDefinition;
 int width,height;
 long flags;
 int enabled;
};
// action: 1 selection, 2 activation, 3 edit. The callback receives ordinary
// UTF8 labels and owned item text, not a scientific data interpretation.
using Olex2NativeNavigationAction=int(*)(void*,int action,const char* label,const char* data);
extern "C" void* olex2_native_navigation_mount(void* nodeContent,const Olex2NativeNavigationValues*,Olex2NativeNavigationAction,void*,char*,std::size_t);
extern "C" int olex2_native_navigation_set_enabled(void*,int,char*,std::size_t);
extern "C" int olex2_native_navigation_unmount(void*,char*,std::size_t);
