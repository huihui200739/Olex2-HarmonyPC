#pragma once
#include <string>
struct Olex2LegacyHost;
namespace olex2::harmony {
class PngTextureGXApp;
class OriginalLogCapture;
// Caller holds the unique runtime/interpreter locks and a live scene/context lease.
std::string RunOriginalTextureLoadProbe(const Olex2LegacyHost&, PngTextureGXApp&,
                                      OriginalLogCapture&, int width, int height);
bool OriginalLoadedTexturesDetached(PngTextureGXApp&);
}
