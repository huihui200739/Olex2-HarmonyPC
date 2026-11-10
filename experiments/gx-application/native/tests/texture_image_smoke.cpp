#include "texture_image.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace olex2::harmony;
int main() {
  try {
    const std::vector<uint8_t> rgba={17,31,89,255,241,12,63,255, 250,251,252,253,
                                    5,173,231,255,211,97,43,255, 254,251,252,253};
    const std::vector<uint8_t> expected={17,31,89,241,12,63,5,173,231,211,97,43};
    auto image=PackOpaqueRgbaTexture(2,2,12,rgba.data(),rgba.size());
    if(!image.Decoded()||image.rgb!=expected||image.width!=2||image.height!=2||image.rowStride!=12)throw std::runtime_error("Opaque row-major color/padding conversion differs");
    auto transparent=rgba;transparent[3]=0;
    auto unsupported=PackOpaqueRgbaTexture(2,2,12,transparent.data(),transparent.size());
    if(unsupported.status!=TextureImageStatus::UnsupportedTransparency||!unsupported.rgb.empty())throw std::runtime_error("Transparent RGB was silently changed");
    const std::vector<uint8_t> single={19,149,229,255};
    auto one=PackOpaqueRgbaTexture(1,1,4,single.data(),single.size());
    if(!one.Decoded()||one.rgb!=std::vector<uint8_t>({19,149,229}))throw std::runtime_error("Single texel color differs");
    std::vector<uint8_t> normal3x3(36,255);
    auto dimensions=PackOpaqueRgbaTexture(3,3,12,normal3x3.data(),normal3x3.size());
    if(dimensions.status!=TextureImageStatus::UnsupportedDimensions||!dimensions.rgb.empty())throw std::runtime_error("Original power-of-two texture constraint differs");
    std::cout<<"PASS ordinary synthetic opaque RGB, padded rows, single texel; explicit transparency and non-power-of-two limits\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
