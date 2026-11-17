#pragma once
#include <cstddef>
// Three explicit version-1 private C entries. Existing ABI symbols remain
// unchanged; no original C++ controls or platform node types cross the DSO seam.
using Olex2NativeZoomChange=int(*)(void* context,int tenths);
extern "C" {
void* olex2_native_zoom_mount(void* nodeContent,int tenths,bool enabled,
  Olex2NativeZoomChange,void* context,char* error,std::size_t capacity);
int olex2_native_zoom_update(void* owner,int tenths,bool enabled,char* error,std::size_t capacity);
int olex2_native_zoom_unmount(void* owner,char* error,std::size_t capacity);
}
