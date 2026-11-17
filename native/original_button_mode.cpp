#include "original_button_mode.h"
#include "olxstate.h"
namespace olex2::harmony::ui {
bool OriginalButtonModeActive(const olxstr& mode){return TModeRegistry::CheckMode(mode);}
}
