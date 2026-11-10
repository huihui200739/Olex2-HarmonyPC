#pragma once
#include <array>
#include <string>
namespace olex2::harmony {
// Symmetric opaque adapters: strict legacy Parse/SetPreset and bounded
// SetLitPreset specular/shininess. Not full fixed-function lighting or blending.
class OpaqueMaterial {
public:
  OpaqueMaterial();
  bool Parse(const std::string& representation);
  bool SetPreset(unsigned preset, const std::array<float,4>& base);
  bool SetLitPreset(unsigned preset, const std::array<float,4>& base);
  const std::array<float,3>& Specular() const { return specular_; }
  float Shininess() const { return shininess_; }
  unsigned ActiveFlags() const { return flags_; }
  const std::array<float,3>& Ambient() const { return ambient_; }
  const std::array<float,3>& Diffuse() const { return diffuse_; }
  const std::array<float,3>& Emission() const { return emission_; }
  unsigned CommandCount() const { return commandCount_; }
  const std::string& Canonical() const { return canonical_; }
  static const char* Source();
  static unsigned Flags();
private:
  std::array<float,3> ambient_{}, diffuse_{}, emission_{};
  std::string canonical_;
  unsigned commandCount_=0, flags_=783;
  std::array<float,3> specular_{};
  float shininess_=0;
};
float LitColorScale(const unsigned char* pixel, const std::array<float,4>& base);
}
