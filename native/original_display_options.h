#pragma once
#include "gradient_options.h"
#include "primitive_options.h"
namespace glObj {class TGlBackground;class AGDrawObject;}
namespace olex2::harmony::ui {
// The caller holds the original renderer's lifetime and serialization lock.
// Runtime background stores resolve the current background on every operation.
GradientColors ReadOriginalGradientBackground(const glObj::TGlBackground&);
void ApplyOriginalGradientBackground(glObj::TGlBackground&,const GradientColors&);
PrimitiveDisplaySnapshot ReadOriginalPrimitiveDisplay(glObj::AGDrawObject&);
}
