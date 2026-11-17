#pragma once
#include "library.h"
namespace olex2::harmony {
// Owns a Harmony-only compatibility library backed by implementations from
// the original xpprocs translation unit.
esdl::TLibrary* CreateHarmonyMainFormLibrary();
}
