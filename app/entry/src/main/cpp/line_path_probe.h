#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace olex2::harmony {
struct LinePathGpuSample {std::string name;int x=0,y=0;bool expectedLit=false,passed=false;std::array<uint8_t,36> rgba{};};
struct LinePathGpuCase {std::string kind,indexSha256;unsigned inputVertices=0,indexCount=0;bool actualBufferVerified=false,pixelsVerified=false;float actualLineWidth=0;std::vector<uint32_t> actualIndices;std::vector<LinePathGpuSample> samples;};
struct LinePathGpuProbe {bool verified=false,stateRestored=false,contextRestored=false;std::string error;std::vector<LinePathGpuCase> cases;};
// Isolated uncolored width-one check in the actual current app context.
// Two uncolored multiplier-one synthetic contours, not original normal/color/
// selecting/wide-line equivalence, primary mesh integration or full renderer.
constexpr const char* OriginalLinePathGpuProbeSource(){return "original sgloLineStrip/sgloLineLoop -> bounded source-edge indices -> actual main-context GLES3 mapped EBO and isolated RGBA8 FBO readback; uncolored width-one only";}
LinePathGpuProbe RunOriginalLinePathGpuProbe();
}
