#pragma once

#include "gxapp.h"
#include <cstddef>

namespace olex2::harmony {
// Registers the original application's file parsers on a non-wx TGXApp.
// Throws if an extension alias is missing after registration.
std::size_t RegisterOriginalFileFormats(gxlib::TGXApp& app);
}
