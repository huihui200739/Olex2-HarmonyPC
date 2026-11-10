#pragma once
#include <EGL/egl.h>
#include <string>
#include "display_font_metrics.h"
struct SharedOriginalWindowResult {unsigned run=0;bool completed=false;std::string json;};
SharedOriginalWindowResult RunSharedOriginalWindow(EGLDisplay,EGLSurface,EGLContext,int,int,void*,const std::string&,bool loadTextures=false,bool fontProbe=false,const olex2::harmony::FontDisplayMetrics& fontDisplay={});
