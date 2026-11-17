#pragma once
#include <string>
#include "gltexture.h"
struct Olex2LegacyHost;
// Real original renderer-owned textures, retained as CPU data between leases.
// Caller restores the renderer manager before entry and detaches before GL closes.
std::string RunPersistentTextureWindowProbe(const Olex2LegacyHost&,TTextureManager&,int,int);
bool PersistentWindowTexturesDetached(TTextureManager&);
