// Link-only check. Not an emulator observation and not part of the application.
#include "system_font_catalog.h"
int main(){olex2::harmony::SystemFontCatalog catalog;return catalog.Json().empty()?1:0;}
