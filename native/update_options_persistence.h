#pragma once
#include <functional>
#include <string>
namespace olex2::harmony::ui {
// The original serializer writes a same-directory private temporary file.
// A false/throw leaves the existing destination intact. This is not a cross-
// process transaction or a promise of crash-durable directory metadata.
void SaveUpdateOptionsAtomically(const std::string& destination,
  const std::function<bool(const std::string& temporary)>& serialize);
}
