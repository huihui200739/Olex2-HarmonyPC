// Must remain the first/only project include. No forced or transitive GL header.
#include "gltexture.h"
static_assert(sizeof(glObj::TGlTexture) > 0, "Original texture header is self-contained");
