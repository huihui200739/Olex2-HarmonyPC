#pragma once
#include "ememstream.h"
#include "sha.h"
#include <cctype>
#include <string>

namespace olex2::harmony {
// Use the original stream overload for byte-exact platform file identities.
// It retains the padding marker when the final block has 56 input bytes;
// the legacy pointer overload does not. No upstream source is modified.
inline std::string PlatformSha256(const std::string& bytes) {
  esdl::TEMemoryInputStream stream(bytes.data(), bytes.size());
  const std::string grouped = esdl::SHA256::Digest(stream).c_str();
  std::string result;
  result.reserve(64);
  for (unsigned char c : grouped) {
    if (c != ' ') result += static_cast<char>(std::tolower(c));
  }
  return result;
}
}
