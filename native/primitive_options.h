#pragma once
#include "estrlist.h"
#include <cstdint>
#include <vector>
namespace olex2::harmony::ui {
// A synchronous copy of display-only state. The dialog never retains a raw
// graphics-object reference and never applies its result to that object.
struct PrimitiveDisplaySnapshot {
  std::vector<olxstr> labels;
  uint32_t mask=0;
  bool hasApplyScope=false;
  unsigned currentLevel=0;
};
}
