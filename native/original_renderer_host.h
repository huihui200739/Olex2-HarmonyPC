#pragma once
#include <stddef.h>
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
};
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost*);
