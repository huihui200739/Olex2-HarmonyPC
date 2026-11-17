#pragma once
#include <cstddef>
struct Olex2NativePickerValues {
  const char* date="2000-01-01"; // YYYY-MM-DD; empty only with allowNone.
  const char* color="#000000FF"; // #RRGGBB or #RRGGBBAA.
  bool allowNone=false,readOnly=false,enabled=true;
};
// kind: 1=calendar, 2=color. The canonical UTF-8 value is valid synchronously
// during the callback only; nonzero reports a caller error.
using Olex2NativePickerChange=int(*)(void*,int,const char*);
extern "C" {
void* olex2_native_pickers_mount(void*,const Olex2NativePickerValues*,Olex2NativePickerChange,void*,char*,std::size_t);
int olex2_native_pickers_update(void*,const Olex2NativePickerValues*,char*,std::size_t);
int olex2_native_pickers_unmount(void*,char*,std::size_t);
}
