#pragma once
#include <EGL/egl.h>
#include <string>
struct SharedOriginalWindowResult {unsigned run=0;bool completed=false;std::string json;};
SharedOriginalWindowResult RunSharedOriginalWindow(EGLDisplay,EGLSurface,EGLContext,int,int,void*,const std::string&);
