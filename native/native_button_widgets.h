#pragma once
#include <cstddef>
struct Olex2NativeButtonValues {
 const char* caption;const char* bitmapSource;const char* imageSources;
 const char* hint;const char* drawParameters;int imageWidth;int imageHeight;int enabled;
};
// kind: 1 text, 2 bitmap, 3 multi-state image; action: 1 click, 2 down, 3 up.
// logicalDown is the original action-time state, not an invented post-click value.
using Olex2NativeButtonAction=int(*)(void*,int kind,int action,int logicalDown);
extern "C" void* olex2_native_buttons_mount(void* nodeContent,const Olex2NativeButtonValues*,Olex2NativeButtonAction,void* context,char* error,std::size_t capacity);
extern "C" int olex2_native_buttons_set_enabled(void* owner,int enabled,char* error,std::size_t capacity);
extern "C" int olex2_native_buttons_unmount(void* owner,char* error,std::size_t capacity);
