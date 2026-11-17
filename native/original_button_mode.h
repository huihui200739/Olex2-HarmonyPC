#pragma once
#include "estrlist.h"
namespace olex2::harmony::ui {
// Narrow GUI-mode query. The shipping definition calls the original registry;
// ordinary host tests explicitly provide a separate synthetic mode fixture.
bool OriginalButtonModeActive(const olxstr& mode);
}
