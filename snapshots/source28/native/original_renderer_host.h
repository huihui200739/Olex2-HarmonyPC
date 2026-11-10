#pragma once
#include <stddef.h>
#include "display_font_metrics.h"
// Private same-build host ABI: borrows the existing runtime; no second singleton.
struct Olex2LegacyHost {
  void* (*resolve)(const char*);
  void (*framebuffer_size)(int*, int*);
  int (*make_current)();
  const char* files_dir;
  const char* license_bytes;
  size_t license_length;
  void* existing_runtime;
  unsigned run;
  void (*gpu_snapshot)(unsigned, const char*);
  int* completed;
  bool draw_platform_fonts=false;
  bool load_platform_textures=false; // private same-build route, default old diagnostics
  olex2::harmony::FontDisplayMetrics font_display_metrics;
};
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost*);
