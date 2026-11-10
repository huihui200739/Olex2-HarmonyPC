#pragma once
#include <string>
struct Olex2LegacyHost;
// Uses only the caller's current real window context, never a mock/offscreen one.
std::string RunOriginalTexture1DWindowProbe(const Olex2LegacyHost&,int,int);
