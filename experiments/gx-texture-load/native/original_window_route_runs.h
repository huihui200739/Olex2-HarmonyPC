#pragma once
#include <limits>
#include <stdexcept>
namespace olex2::harmony {
// Serialized by the caller's existing window-probe mutex. Each route has its
// own generation sequence, even though it borrows the same renderer/context.
class OriginalWindowRouteRuns {
  unsigned original_ = 0;
  unsigned textureLoad_ = 0;
public:
  unsigned Next(bool loadTextures) {
    unsigned& counter = loadTextures ? textureLoad_ : original_;
    if (counter == std::numeric_limits<unsigned>::max())
      throw std::overflow_error("Original window route counter exhausted");
    return ++counter;
  }
};
}
