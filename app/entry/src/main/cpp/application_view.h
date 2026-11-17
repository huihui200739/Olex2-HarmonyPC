#pragma once
#include <EGL/egl.h>
#include <cstdint>
#include <string>
#include "original_renderer_host.h"

constexpr size_t MaximumOriginalDocumentBytes = 8u * 1024u * 1024u;
bool ValidateOriginalDocumentInput(const std::string& name, const std::string& bytes,
                                  std::string& error);
struct ApplicationViewResult {
  bool passed=false, surfacePresented=false, parentContextRestored=false;
  bool coreInvoked=false, coreStateKnown=false, corePassed=false, loaded=false;
  bool modelMayHaveChanged=false, contextRetired=false, sessionRetained=false;
  uint64_t contextGeneration=0;
  std::string error, coreJson, json, exportedBytes, viewportJson;
};

// All calls are serialized by Renderer::mutex_. No ownership of ApplicationRuntime.
// The GL4ES session and this unshared EGLContext live until successful core Close.
// Surface loss detaches a surface, never destroys the document context/model.
class ApplicationView {
public:
  ApplicationView();
  ApplicationView(const ApplicationView&)=delete;
  ApplicationView& operator=(const ApplicationView&)=delete;
  bool HasContext() const { return context_ != EGL_NO_CONTEXT; }
  EGLDisplay Display() const { return display_; }
  bool CompatibleConfig(EGLDisplay display, EGLConfig& config, std::string& error) const;
  ApplicationViewResult Failure(const std::string& error) const;
  ApplicationViewResult Run(EGLDisplay mainDisplay, EGLSurface mainSurface,
      EGLContext mainContext, int width, int height, void* runtime,
      const std::string& privateDirectory, Olex2DocumentRequest request,
      const olex2::harmony::FontDisplayMetrics& fontDisplay);
private:
  EGLDisplay display_=EGL_NO_DISPLAY;
  EGLContext context_=EGL_NO_CONTEXT;
  EGLConfig config_=nullptr;
  EGLSurface cleanupSurface_=EGL_NO_SURFACE; // retained only if cleanup fails; retry, never lose ownership
  EGLint configId_=0;
  uint64_t generation_=0;
  unsigned sequence_=0;
  void* driver_=nullptr;
  bool loaded_=false;
  std::string coreJson_;
};
