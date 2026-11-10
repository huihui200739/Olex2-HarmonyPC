// CPU-only checks of the patched original light constructor; no GL context.
#include "gllight.h"
#include <iostream>
#include <new>
#include <type_traits>
#include <cstring>

int main() {
  using Light = glObj::TGlLight;
  static_assert(std::is_same_v<decltype(std::declval<Light>().GetSpotExponent()), short>,
                "Preserve the original short-valued light ABI");
  // Nonzero backing bytes ensure the check does not accidentally rely on a
  // zero-filled allocation. Never inspect any uninitialized original member.
  for (unsigned fill : {0x00u, 0x55u, 0xaau, 0xffu}) {
    alignas(Light) unsigned char bytes[sizeof(Light)];
    std::memset(bytes, fill, sizeof(bytes));
    Light* light = new(bytes) Light();
    if (light->GetSpotExponent() != 0 || light->GetSpotCutoff() != 180 || light->IsEnabled()) {
      light->~Light(); std::cerr << "FAIL original light defaults\n"; return 1;
    }
    light->SetSpotExponent(19);
    if (light->GetSpotExponent() != 19) {
      light->~Light(); std::cerr << "FAIL original light setter\n"; return 1;
    }
    light->~Light();
  }
  std::cout << "PASS original light defaults/setter, 4 nonzero/zero storage patterns; CPU only\n";
}
