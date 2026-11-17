#pragma once
#include "library.h"
#include <cstddef>
namespace olex2::harmony {
// Register original application utility functions whose behavior is not
// coupled to the wx desktop controls.
std::size_t RegisterHarmonyXpprocs(esdl::TLibrary& library);
}
