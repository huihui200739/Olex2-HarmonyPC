#include "original_texture_load_probe.h"
#include "original_texture_load_fixtures.h"
#include "original_renderer_host.h"
#include "png_gx_texture_app.h"
#include "original_log.h"
#include "gltexture.h"
#include "glpixelstorescope.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>

namespace olex2::harmony {
namespace {
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void Check(const char* stage) {
  const GLenum error = glGetError();
  if (error) throw std::runtime_error(std::string(stage) + " GL " + std::to_string(error));
}
std::string Quote(const std::string& value) {
  std::ostringstream out; out << '"';
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << c;
    else if (c >= 32) out << c;
    else out << "\\u00" << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
  }
  out << '"'; return out.str();
}
void Save(const std::string& path, const unsigned char* data, size_t size) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(data), size); file.close();
  Require(bool(file), "Could not retain texture-load evidence");
}
void Log(std::ostringstream& out, const OriginalLogSnapshot& value) {
  out << "{\"attached\":" << (value.attached ? "true" : "false")
      << ",\"posted\":" << value.posted << ",\"dropped\":" << value.dropped
      << ",\"truncated\":" << value.truncated << ",\"captureFailures\":" << value.captureFailures
      << ",\"clearCount\":" << value.clearCount << ",\"clearedEntries\":" << value.clearedEntries
      << ",\"textSha256\":" << Quote(value.textSha256) << ",\"entries\":[";
  for (size_t i = 0; i < value.entries.size(); ++i) {
    if (i) out << ',';
    const auto& entry = value.entries[i];
    out << "{\"sequence\":" << entry.sequence << ",\"text\":" << Quote(entry.text)
        << ",\"truncated\":" << (entry.truncated ? "true" : "false") << '}';
  }
  out << "]}";
}
std::string Stem(const Olex2LegacyHost& host, const std::string& phase) {
  return std::string(host.files_dir) + "/load-textures-run-" + std::to_string(host.run) + '-' + phase;
}
// This deliberately retains LoadTextures' historical x/y transpose, rather than
// replacing it with a new texture convention.
std::vector<unsigned char> Expected(const OriginalTextureLoadFixture& fixture) {
  std::vector<unsigned char> result;
  for (unsigned y = 0; y < fixture.width; ++y) {
    for (unsigned x = 0; x < fixture.width; ++x) {
      const size_t source = (size_t(x) * fixture.width + y) * 3;
      for (unsigned c = 0; c < 3; ++c) result.push_back(fixture.rgb[source + c]);
      result.push_back(255);
    }
  }
  return result;
}
void Draw(TTextureManager& manager, int width, int height) {
  glViewport(0, 0, width, height);
  glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_SCISSOR_TEST);
  glDisable(GL_TEXTURE_1D); glDisable(GL_TEXTURE_GEN_S); glDisable(GL_TEXTURE_GEN_T);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glColor4f(1, 1, 1, 1);
  for (unsigned index = 0; index < 2; ++index) {
    auto* texture = manager.FindByName(OriginalTextureLoadFixtures[index].name);
    Require(texture != nullptr, "Original loader did not create both named textures");
    texture->SetCurrent();
    // Do not override min/mag filters or S/T wrapping supplied by LoadTextures.
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    const float left = float(index) - 1.0f, right = left + 1.0f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(left, -1, 0);
    glTexCoord2f(1, 0); glVertex3f(right, -1, 0);
    glTexCoord2f(1, 1); glVertex3f(right, 1, 0);
    glTexCoord2f(0, 1); glVertex3f(left, 1, 0);
    glEnd();
  }
  glFinish(); Check("Original loaded 2D texture actual window draw");
}
void Observe(std::ostringstream& out, const Olex2LegacyHost& host,
             PngTextureGXApp& app, const char* phase, unsigned fixtureOffset,
             int width, int height) {
  auto& manager = app.GetRenderer().GetTextureManager();
  out << "{\"phase\":" << Quote(phase) << ",\"textures\":[";
  for (unsigned index = 0; index < 2; ++index) {
    if (index) out << ',';
    const auto& fixture = OriginalTextureLoadFixtures[fixtureOffset + index];
    auto* texture = manager.FindByName(fixture.name);
    Require(texture && texture->GetType() == GL_TEXTURE_2D && texture->IsEnabled(), "Loaded texture identity/type differs");
    Require(texture->GetId() != ~GLuint(0) && manager.FindTexture(texture->GetId()) == texture, "Resident texture manager index differs");
    const auto read = texture->ReadData(); Check("Read original loaded texture");
    Require(read.width == int(fixture.width) && read.height == int(fixture.width) && read.border == 0, "Loaded texture dimensions differ");
    const auto expected = Expected(fixture);
    Require(std::equal(expected.begin(), expected.end(), &read.data[0]), "Original loaded texture RGB transpose differs");
    Save(Stem(host, phase) + '-' + fixture.name + "-cpu.rgba", &read.data[0], expected.size());
    texture->SetCurrent();
    GLint minimum=0, maximum=0, wrapS=0, wrapT=0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minimum);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &maximum);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrapS);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrapT);
    Check("Query actual original loader sampler");
    Require(minimum == GL_NEAREST && maximum == GL_LINEAR && wrapS == GL_REPEAT && wrapT == GL_REPEAT,
            "Original loader sampler differs");
    out << "{\"name\":" << Quote(fixture.name) << ",\"width\":" << read.width << ",\"height\":" << read.height
        << ",\"texture_address\":" << reinterpret_cast<uintptr_t>(texture) << ",\"resident_id\":" << texture->GetId()
        << ",\"min_filter\":" << minimum << ",\"mag_filter\":" << maximum
        << ",\"wrap_s\":" << wrapS << ",\"wrap_t\":" << wrapT << '}';
  }
  Draw(manager, width, height);
  std::vector<unsigned char> pixels(size_t(width) * height * 4);
  {
    olex2_harmony::ScopedPackedPixels<olx_gl> pack(GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  }
  Check("Read actual loaded texture window pixels");
  Save(Stem(host, phase) + ".rgba", pixels.data(), pixels.size());
  if (host.gpu_snapshot) host.gpu_snapshot(host.run, (std::string("load-textures-") + phase).c_str());
  out << "]}";
}
}
std::string RunOriginalTextureLoadProbe(const Olex2LegacyHost& host, PngTextureGXApp& app,
                                      OriginalLogCapture& log, int width, int height) {
  auto& manager = app.GetRenderer().GetTextureManager();
  auto* before0 = manager.FindByName("LockedAtoms");
  auto* before1 = manager.FindByName("ConstrainedAtoms");
  Require((before0 != nullptr) == (before1 != nullptr), "Only one original UI texture was retained");
  const bool restored = before0 != nullptr;
  // Snapshot is the observer's non-logging API. Do not call ApplicationRuntime's
  // lock-taking OriginalLogs() while already holding the interpreter guard.
  const auto logBefore = log.Snapshot();
  std::ostringstream out;
  out << "{\"schema\":1,\"route\":\"original-TGXApp-LoadTextures-real-window\",\"offscreen\":false"
      << ",\"restored_from_prior_context\":" << (restored ? "true" : "false")
      << ",\"manager_address\":" << reinterpret_cast<uintptr_t>(&manager) << ",\"phases\":[";
  if (restored) { Observe(out, host, app, "restored-b", 2, width, height); out << ','; }
  TGlTexture* identities[2] = {before0, before1};
  for (unsigned generation = 0; generation < 2; ++generation) {
    if (generation) out << ',';
    const std::string folder = std::string(host.files_dir) + "/texture-input-" + char('a' + generation);
    Require(mkdir(folder.c_str(), 0755) == 0, "Could not create exclusive original loader input directory");
    for (unsigned index = 0; index < 2; ++index) {
      const auto& fixture = OriginalTextureLoadFixtures[generation * 2 + index];
      Save(folder + '/' + fixture.name + ".png", fixture.png, fixture.pngSize);
    }
    // This is the genuine original loader, including Add/Replace2DTexture,
    // original cache invalidation and actual TBasicApp log notifications.
    app.LoadTextures(TUtf8::Decode(folder.c_str()));
    Check("Original TGXApp LoadTextures");
    Require(app.LastPngTexture().DecoderAllocationsBalanced(), "PNG decoder allocations did not balance");
    for (unsigned index = 0; index < 2; ++index) {
      auto* texture = manager.FindByName(OriginalTextureLoadFixtures[index].name);
      Require(texture && (!identities[index] || identities[index] == texture), "Original replacement changed texture object identity");
      identities[index] = texture;
    }
    Observe(out, host, app, generation ? "loaded-b" : "loaded-a", generation * 2, width, height);
  }
  const auto logAfter = log.Snapshot();
  Require(logAfter.posted == logBefore.posted + 4 && logAfter.dropped == logBefore.dropped &&
          logAfter.truncated == logBefore.truncated && logAfter.captureFailures == logBefore.captureFailures &&
          logAfter.clearCount == logBefore.clearCount && logAfter.clearedEntries == logBefore.clearedEntries,
          "Original loader log delta was changed or suppressed");
  out << "],\"decoder_allocations_balanced\":true,\"log_before\":"; Log(out, logBefore);
  out << ",\"log_after\":"; Log(out, logAfter); out << '}';
  return out.str();
}
bool OriginalLoadedTexturesDetached(PngTextureGXApp& app) {
  auto& manager = app.GetRenderer().GetTextureManager();
  for (const char* name : {"LockedAtoms", "ConstrainedAtoms"}) {
    auto* texture = manager.FindByName(name);
    if (!texture || texture->GetId() != ~GLuint(0)) return false;
  }
  return true;
}
}
