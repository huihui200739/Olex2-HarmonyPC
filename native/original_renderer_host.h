#pragma once
#include <stddef.h>
#include <cstdint>
#include <string>
#include "display_font_metrics.h"
// Private same-build document-view request; both DSOs use the same libc++.
// This is not a stable third-party plugin ABI or a security sandbox.
enum class Olex2DocumentOperation { Render, Open, Close, Export, ResetView, Pick, ClearSelection, SelectAll, InvertSelection, CaptureViewport, RestoreViewport, ToggleCellVisibility, ToggleBasisVisibility, ToggleFrameVisibility, ToggleGridVisibility, ShowAllVisible, CenterModel, ViewAlong100, ViewAlong010, ViewAlong001, ViewAlong110, ViewAlong101, ViewAlong011, ViewAlong111, ViewAlongSelection, HideSelection, SetDrawingQualityLow, SetDrawingQualityMedium, SetDrawingQualityHigh, ToggleHydrogensVisibility, ToggleHydrogenBondsVisibility, ToggleQPeaksVisibility, ToggleQPeakBondsVisibility, ToggleLabelsVisibility, ToggleStructureVisibility, FragmentSelectAll, FragmentSelectAtoms, FragmentSelectBonds, FragmentHide, FragmentShowOnly };
struct Olex2DocumentRequest {
  Olex2DocumentOperation operation=Olex2DocumentOperation::Render;
  std::string name,bytes;
  double yaw=32.,pitch=-22.,distance=4.4f;
  std::string* exportedBytes=nullptr;
  std::string viewport_snapshot; // strictly validated data, never a command
  std::string* viewport_result=nullptr; // synchronous borrowed output, both DSOs same build
  uint64_t context_generation=0; // stable while the original document context lives
  double pointer_x=0.5,pointer_y=0.5; // normalized top-left viewport coordinates, finite [0,1]
  unsigned selection_mode=0; // Pick: 0 replace, 1 add, 2 toggle; does not edit document data
};
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
  const Olex2DocumentRequest* document_request=nullptr;
};
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost*);
