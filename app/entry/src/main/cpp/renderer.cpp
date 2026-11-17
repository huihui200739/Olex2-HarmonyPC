#include "shared_original_probe.h"
#include <fstream>
#include <sstream>
#include "renderer.h"
#include "original_document_viewport.h"
#include <EGL/eglext.h>
#include <hilog/log.h>
#include <native_window/external_window.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <utility>
#include <sstream>
#include <cstring>
#include <cctype>
#include "sha.h"

namespace {
constexpr unsigned DOMAIN = 0x3801;
constexpr const char* TAG = "Olex2Native";
GLuint Compile(GLenum kind, const char* source, std::string& error) {
  GLuint shader = glCreateShader(kind);
  glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
  GLint compiled = GL_FALSE; glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char log[2048]{}; glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    error = log; glDeleteShader(shader); return 0;
  }
  return shader;
}
std::string EglError(const char* where) {
  std::ostringstream s; s << where << "; EGL error=0x" << std::hex << eglGetError(); return s.str();
}
}
Renderer& Renderer::Instance() { static Renderer renderer; return renderer; }
GraphicsStatus Renderer::Status() { std::lock_guard<std::mutex> guard(mutex_); return status_; }
bool Renderer::FailLocked(const std::string& message) {
  status_.error = message; status_.initialized = false;
  OH_LOG_Print(LOG_APP, LOG_ERROR, DOMAIN, TAG, "OLEX2_RENDER_FAIL %{public}s", message.c_str());
  return false;
}
bool Renderer::DimensionsLocked(OH_NativeXComponent* component, void* window) {
  uint64_t width=0, height=0;
  if (!component || !window || OH_NativeXComponent_GetXComponentSize(component, window, &width, &height) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
      width == 0 || height == 0 || width > 16384 || height > 16384) {
    return FailLocked("Native XComponent surface dimensions unavailable");
  }
  status_.width = static_cast<int>(width); status_.height = static_cast<int>(height);
  return true;
}
void Renderer::Attach(OH_NativeXComponent* component, void* window) {
  std::lock_guard<std::mutex> guard(mutex_);
  ReleaseLocked(); status_ = GraphicsStatus{}; UpdateColorStatusLocked();
  status_.originalDocumentFrames=originalFrames_;
  if(originalRuntime_)ClearDemoQualificationLocked();
  if (!DimensionsLocked(component, window)) return;
  if (OH_NativeWindow_NativeObjectReference(window) != 0) { FailLocked("Cannot retain native window"); return; }
  window_ = window; status_.surfaceReady = true;
  if (InitializeLocked()) DrawLocked();
}
void Renderer::Resize(OH_NativeXComponent* component, void* window) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (window_ != window || !DimensionsLocked(component, window)) return;
  OH_NativeWindow_NativeWindowHandleOpt(static_cast<OHNativeWindow*>(window_), SET_BUFFER_GEOMETRY, status_.width, status_.height);
  if (status_.initialized) DrawLocked();
}
void Renderer::Destroy() {
  std::lock_guard<std::mutex> guard(mutex_);
  ReleaseLocked(); status_.surfaceReady = false; status_.initialized = false;
  if(originalRuntime_){
    ClearDemoQualificationLocked();
    const auto waiting=originalView_.Failure("Main surface is unavailable; original document/context retained pending recreation or explicit offscreen Close");
    originalStatus_=waiting.json;status_.error=waiting.error;
  }
  OH_LOG_Print(LOG_APP, LOG_INFO, DOMAIN, TAG, "OLEX2_SURFACE_DESTROYED");
}
void Renderer::SetView(float yaw, float pitch, float distance) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(distance)) return;
  yaw_=std::fmod(yaw,360.f); pitch_=std::clamp(pitch,-85.f,85.f); distance_=std::clamp(distance,2.2f,10.f);
  if (status_.initialized) DrawLocked();
}
void Renderer::UpdateColorStatusLocked() {
  status_.materialCommandCount=material_.CommandCount();
  status_.materialPreset=materialPreset_; status_.materialRevision=materialRevision_;
  status_.materialFlags=material_.ActiveFlags(); status_.materialSource=material_.Source(); status_.materialCanonical=material_.Canonical();
  status_.specularR=material_.Specular()[0];status_.specularG=material_.Specular()[1];status_.specularB=material_.Specular()[2];status_.shininess=material_.Shininess();
  status_.ambientR=material_.Ambient()[0]; status_.ambientG=material_.Ambient()[1]; status_.ambientB=material_.Ambient()[2];
  status_.diffuseR=material_.Diffuse()[0]; status_.diffuseG=material_.Diffuse()[1]; status_.diffuseB=material_.Diffuse()[2];
  status_.emissionR=material_.Emission()[0]; status_.emissionG=material_.Emission()[1]; status_.emissionB=material_.Emission()[2];
  status_.colorPreset=colorPreset_; status_.colorRevision=colorRevision_;
  status_.colorPacked=color_.Packed(); status_.colorSource=color_.Source();
  const auto& rgba=color_.Rgba();
  status_.colorR=rgba[0]; status_.colorG=rgba[1]; status_.colorB=rgba[2]; status_.colorA=rgba[3];
}
bool Renderer::SetColorPreset(unsigned preset) {
  std::lock_guard<std::mutex> guard(mutex_);
  if(originalRuntime_){status_.error="Demo color controls do not apply to original documents";return false;}
  olex2::harmony::OpaqueColor candidate;
  olex2::harmony::OpaqueMaterial nextMaterial;
  if(!candidate.SetPreset(preset) || !nextMaterial.SetLitPreset(materialPreset_,candidate.Rgba())) return false;
  color_=std::move(candidate); material_=std::move(nextMaterial);
  colorPreset_=preset; ++colorRevision_; ++materialRevision_; UpdateColorStatusLocked();
  if(status_.initialized) DrawLocked();
  return true;
}
bool Renderer::SetMaterialPreset(unsigned preset) {
  std::lock_guard<std::mutex> guard(mutex_);
  if(originalRuntime_){status_.error="Demo material controls do not apply to original documents";return false;}
  if(!material_.SetLitPreset(preset,color_.Rgba())) return false;
  materialPreset_=preset; ++materialRevision_; UpdateColorStatusLocked();
  if(status_.initialized) DrawLocked();
  return true;
}
bool Renderer::ApplyDesktopCommandState(const olex2::harmony::DesktopCommandState& state,std::string& error) {
  std::lock_guard<std::mutex> guard(mutex_);
  if(originalRuntime_){error="Close the original document before applying demo-only desktop commands; use the document view controls";return false;}
  if(!status_.initialized) {error="Open the native viewport before applying view commands";return false;}
  if(state.primitive>3 || state.generation>5 || state.colorPreset>2 || state.materialPreset>3 ||
      !std::isfinite(state.yaw) || !std::isfinite(state.pitch) || !std::isfinite(state.distance) || state.pitch<-85 || state.pitch>85 || state.distance<2.2f || state.distance>10) {
    error="Invalid command view state";return false;
  }
  olex2::harmony::OpaqueColor color;olex2::harmony::OpaqueMaterial material;
  if(!color.SetPreset(state.colorPreset) || !material.SetLitPreset(state.materialPreset,color.Rgba())) {error="Command color/material is unsupported";return false;}
  const auto kind=static_cast<olex2::harmony::MeshKind>(state.primitive);
  if(!eglMakeCurrent(display_,surface_,surface_,context_)) {error=EglError("Cannot bind command view context");return false;}
  bool uploaded=true;
  try {
    if(kind!=meshKind_ || state.generation!=generation_ || state.wireframe!=wireframe_)
      uploaded=UploadMeshLocked(olex2::harmony::BuildUpstreamPrimitive(kind,state.generation),state.wireframe);
  } catch(const std::exception& exception) {error=exception.what();uploaded=false;}
  eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  if(!uploaded) {if(error.empty())error=status_.error;return false;}
  // CPU parsing/argument validation completed before any real-window mutation.
  // Commit accepted view fields once and draw once, instead of issuing several
  // independently visible control calls. A later GPU failure is reported as
  // such; this does not claim GPU/device-loss transactional rollback.
  if(colorPreset_!=state.colorPreset)++colorRevision_;
  if(colorPreset_!=state.colorPreset || materialPreset_!=state.materialPreset)++materialRevision_;
  color_=std::move(color);material_=std::move(material);colorPreset_=state.colorPreset;materialPreset_=state.materialPreset;
  yaw_=state.yaw;pitch_=state.pitch;distance_=state.distance;UpdateColorStatusLocked();DrawLocked();
  return true;
}
bool Renderer::UploadMeshLocked(olex2::harmony::MeshData mesh, bool wireframe) {
  using olex2::harmony::MeshVertex;
  // Recheck the stream before uploading; never draw an unverified batch.
  olex2::harmony::ValidateMesh(mesh);
  auto originalTriangles=olex2::harmony::RouteMeshThroughOriginalTriangles(mesh);
  // Upload only the actual output of the same branch used by desktop
  // TGlPrimitive. No parallel/replacement triangle writer is used here.
  mesh.submittedVertices=std::move(originalTriangles.vertices);
  olex2::harmony::OriginalLineUpload originalLines;
  if (wireframe) originalLines=olex2::harmony::RouteMeshThroughOriginalLines(mesh);
  const auto& indices=wireframe?originalLines.indices:mesh.drawTriangles;
  GLuint vao=0,vertex_buffer=0,index_buffer=0;
  // Pending GPU names remain owned on every return/exception path, including
  // allocation failure while hashing a mapped readback. ReadMap unwinds first.
  struct PendingUpload {
    GLuint& vao;GLuint& vertex;GLuint& index;bool committed=false;
    ~PendingUpload() {
      if (!committed) {
        glBindVertexArray(0);
        if (vao) glDeleteVertexArrays(1,&vao);
        if (vertex) glDeleteBuffers(1,&vertex);
        if (index) glDeleteBuffers(1,&index);
      }
    }
  } pending{vao,vertex_buffer,index_buffer};
  glGenVertexArrays(1,&vao); glBindVertexArray(vao);
  glGenBuffers(1,&vertex_buffer); glBindBuffer(GL_ARRAY_BUFFER,vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,mesh.submittedVertices.size()*sizeof(MeshVertex),mesh.submittedVertices.data(),GL_STATIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,position)));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,normal)));
  glEnableVertexAttribArray(1);
  glGenBuffers(1,&index_buffer); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(uint32_t),indices.data(),GL_STATIC_DRAW);
  bool lineBufferVerified=false;std::string lineIndexSha256;
  if (wireframe) {
    // Read the actual EBO, not a digest of the staging vector. GPU pixels and
    // original-source topology checks remain separate from this byte check.
    struct ReadMap {
      void* bytes;
      explicit ReadMap(GLsizeiptr length) : bytes(glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER,0,length,GL_MAP_READ_BIT)) {}
      ~ReadMap() { if (bytes) glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER); }
      bool Finish() { if (!bytes) return false;bytes=nullptr;return glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER)==GL_TRUE; }
    } mapped(static_cast<GLsizeiptr>(indices.size()*sizeof(uint32_t)));
    if (mapped.bytes) {
      const bool equal=std::memcmp(mapped.bytes,indices.data(),indices.size()*sizeof(uint32_t))==0;
      const std::string grouped=SHA256::Digest(static_cast<const char*>(mapped.bytes),indices.size()*sizeof(uint32_t)).c_str();
      for (unsigned char c:grouped) if (c!=' ') lineIndexSha256+=static_cast<char>(std::tolower(c));
      lineBufferVerified=mapped.Finish() && equal;
    }
  }
  glBindVertexArray(0);
  if (!vao || !vertex_buffer || !index_buffer || glGetError()!=GL_NO_ERROR || (wireframe && !lineBufferVerified)) {
    status_.error="Upstream mesh GLES3 upload failed";
    return false;
  }
  if (vao_) glDeleteVertexArrays(1,&vao_);
  if (vertex_buffer_) glDeleteBuffers(1,&vertex_buffer_);
  if (index_buffer_) glDeleteBuffers(1,&index_buffer_);
  vao_=vao; vertex_buffer_=vertex_buffer; index_buffer_=index_buffer;pending.committed=true;
  meshKind_=mesh.kind;
  status_.meshKind=olex2::harmony::MeshKindName(mesh.kind);
  status_.sourceBatches=mesh.sourceBatches; status_.discardedTailVertices=mesh.discardedTailVertices;
  status_.degenerateTrianglesSkipped=mesh.degenerateTrianglesSkipped;
  generation_=mesh.generation; wireframe_=wireframe;
  status_.meshSource=mesh.source; status_.meshGeneration=mesh.generation;
  status_.meshVertices=static_cast<unsigned>(mesh.vertices.size());
  status_.meshTriangles=static_cast<unsigned>(mesh.triangles.size()/3);
  status_.submissionValidated=mesh.submissionValidated;
  status_.submittedVertices=static_cast<unsigned>(mesh.submittedVertices.size());
  status_.submissionSource=mesh.submissionSource;
  status_.originalTriangleVerified=originalTriangles.verified;
  status_.originalTriangleVertices=static_cast<unsigned>(mesh.submittedVertices.size());
  status_.originalTriangleNormalCommands=originalTriangles.normalCommands;
  status_.originalTriangleColorCommands=originalTriangles.colorCommands;
  status_.originalTriangleSource=olex2::harmony::OriginalTriangleSource();
  status_.originalLineVerified=originalLines.verified;status_.originalLineBufferVerified=lineBufferVerified;
  status_.originalLineVertices=originalLines.vertices;status_.originalLineColorCommands=originalLines.colorCommands;
  status_.originalLineWidthQueries=originalLines.widthQueries;status_.originalLineWidthCommands=originalLines.widthCommands;
  status_.originalLineIndexSha256=std::move(lineIndexSha256);
  status_.originalPolygonVerified=mesh.originalPolygonVerified;status_.originalPolygonBatches=mesh.originalPolygonBatches;
  status_.originalPolygonVertices=mesh.originalPolygonVertices;status_.originalPolygonNormalCommands=mesh.originalPolygonNormalCommands;status_.originalPolygonColorCommands=mesh.originalPolygonColorCommands;
  status_.meshIndices=static_cast<unsigned>(indices.size());
  status_.wireframe=wireframe; status_.meshValidated=mesh.validated; ++status_.meshRevision;
  mesh_=std::move(mesh);
  OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,TAG,"OLEX2_UPSTREAM_MESH generation=%{public}u vertices=%{public}u triangles=%{public}u wireframe=%{public}d validated=%{public}d revision=%{public}u",
    status_.meshGeneration,status_.meshVertices,status_.meshTriangles,status_.wireframe,status_.meshValidated,status_.meshRevision);
  OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,TAG,"OLEX2_SHARED_SUBMISSION vertices=%{public}u validated=%{public}d source=%{public}s",
    status_.submittedVertices,status_.submissionValidated,status_.submissionSource.c_str());
  return true;
}
void Renderer::SetMesh(unsigned generation, bool wireframe) {
  std::lock_guard<std::mutex> guard(mutex_);
  SetSelectionLocked(generation,wireframe,meshKind_);
}
void Renderer::SetPrimitive(olex2::harmony::MeshKind kind) {
  std::lock_guard<std::mutex> guard(mutex_);
  SetSelectionLocked(generation_,wireframe_,kind);
}
void Renderer::SetSelectionLocked(unsigned generation,bool wireframe,olex2::harmony::MeshKind kind) {
  if(originalRuntime_){status_.error="Demo mesh controls do not apply to original documents";return;}
  if (generation>5) { status_.error="Unsupported mesh generation"; return; }
  if (!status_.initialized) { generation_=generation; wireframe_=wireframe; meshKind_=kind; return; }
  if (!eglMakeCurrent(display_,surface_,surface_,context_)) { FailLocked(EglError("Cannot bind mesh context")); return; }
  bool uploaded=false;
  try { uploaded=UploadMeshLocked(olex2::harmony::BuildUpstreamPrimitive(kind,generation),wireframe); }
  catch (const std::exception& error) { status_.error=error.what(); }
  eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  if (uploaded) DrawLocked();
}
bool Renderer::InitializeLocked() {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display_ == EGL_NO_DISPLAY) return FailLocked(EglError("eglGetDisplay"));
  EGLint major=0,minor=0;
  if (!eglInitialize(display_, &major, &minor)) return FailLocked(EglError("eglInitialize"));
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return FailLocked(EglError("eglBindAPI"));
  EGLConfig config = nullptr; EGLint count=0;
  const EGLint attributes[] = {EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,
    EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
  if(originalView_.HasContext()) {
    std::string error;
    if(!originalView_.CompatibleConfig(display_,config,error))return FailLocked(error);
  }else if (!eglChooseConfig(display_, attributes, &config, 1, &count) || count == 0)
    return FailLocked(EglError("No GLES3 window configuration"));
  OH_NativeWindow_NativeWindowHandleOpt(static_cast<OHNativeWindow*>(window_), SET_BUFFER_GEOMETRY, status_.width, status_.height);
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attributes);
  surface_ = eglCreateWindowSurface(display_, config, reinterpret_cast<EGLNativeWindowType>(window_), nullptr);
  if (context_ == EGL_NO_CONTEXT || surface_ == EGL_NO_SURFACE || !eglMakeCurrent(display_, surface_, surface_, context_))
    return FailLocked(EglError("Cannot create/bind XComponent EGL surface"));
  const auto* version=glGetString(GL_VERSION); const auto* renderer=glGetString(GL_RENDERER);
  status_.version=version?reinterpret_cast<const char*>(version):"unknown";
  status_.renderer=renderer?reinterpret_cast<const char*>(renderer):"unknown";
  const char* vertex_source=R"glsl(#version 300 es
precision highp float;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
uniform mat4 mvp;
uniform mat4 normalTransform;
uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform vec3 emissionColor;
uniform float shininess;
out vec3 tint;
void main() {
  gl_Position=mvp*vec4(position,1.);
  vec3 n=normalize(mat3(normalTransform)*normal);
  vec3 lightDirection=normalize(vec3(.35,.5,1.));
  float light=max(dot(n,lightDirection),0.);
  vec3 halfDirection=normalize(lightDirection+vec3(0.,0.,1.));
  float highlight=light>0.?(shininess==0.?1.:pow(max(dot(n,halfDirection),0.),shininess)):0.;
  tint=ambientColor+diffuseColor*light+specularColor*highlight+emissionColor;
}
)glsl";
  const char* fragment_source="#version 300 es\nprecision mediump float;\nin vec3 tint;\nout vec4 result;\nvoid main(){result=vec4(tint,1.);}\n";
  std::string error;
  GLuint vertex=Compile(GL_VERTEX_SHADER,vertex_source,error);
  GLuint fragment=Compile(GL_FRAGMENT_SHADER,fragment_source,error);
  if (!vertex || !fragment) {
    if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment);
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return FailLocked("Shader compile: "+error);
  }
  program_=glCreateProgram(); glAttachShader(program_,vertex); glAttachShader(program_,fragment); glLinkProgram(program_);
  glDeleteShader(vertex); glDeleteShader(fragment);
  GLint linked=0; glGetProgramiv(program_,GL_LINK_STATUS,&linked);
  if (!linked) {
    char log[2048]{}; glGetProgramInfoLog(program_,sizeof(log),nullptr,log);
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return FailLocked(std::string("Shader link: ")+log);
  }
  glGenQueries(1,&fragment_query_);
  if (!fragment_query_ || glGetError()!=GL_NO_ERROR) {
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return FailLocked("Cannot create GLES3 geometry coverage query");
  }
  // Verify one synthetic concave contour in an isolated RGBA8 FBO using
  // the actual window context and shader, without switching EGL routes.
  try {status_.polygonProbe=RunOriginalPolygonGpuProbe(program_);}
  catch(const std::exception& error) {status_.polygonProbe.error=error.what();}
  status_.polygonProbe.contextRestored=eglGetCurrentContext()==context_ &&
    eglGetCurrentDisplay()==display_ && eglGetCurrentSurface(EGL_DRAW)==surface_ &&
    eglGetCurrentSurface(EGL_READ)==surface_;
  status_.polygonProbe.verified=status_.polygonProbe.verified && status_.polygonProbe.contextRestored;
  if(!status_.polygonProbe.verified) {
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return FailLocked(status_.polygonProbe.error);
  }
  // Independent synthetic path contours, not the primary mesh line source.
  try {status_.linePathProbe=olex2::harmony::RunOriginalLinePathGpuProbe();}
  catch(const std::exception& error) {status_.linePathProbe.error=error.what();}
  status_.linePathProbe.contextRestored=status_.linePathProbe.contextRestored && eglGetCurrentContext()==context_ &&
    eglGetCurrentDisplay()==display_ && eglGetCurrentSurface(EGL_DRAW)==surface_ && eglGetCurrentSurface(EGL_READ)==surface_;
  status_.linePathProbe.verified=status_.linePathProbe.verified && status_.linePathProbe.contextRestored;
  if(!status_.linePathProbe.verified) {
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return FailLocked(status_.linePathProbe.error);
  }
  bool uploaded=false;
  try { uploaded=UploadMeshLocked(olex2::harmony::BuildUpstreamPrimitive(meshKind_,generation_),wireframe_); }
  catch (const std::exception& error) { status_.error=error.what(); }
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (!uploaded) return FailLocked(status_.error);
  status_.initialized=true; status_.error.clear();
  OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,TAG,"OLEX2_CONTEXT_READY %{public}s",status_.version.c_str());
  return true;
}
void Renderer::DrawLocked() {
  if(originalRuntime_) {
    Olex2DocumentRequest request;request.operation=Olex2DocumentOperation::Render;
    RunOriginalDocumentLocked(std::move(request));return; // never draw the demo over a document
  }
  status_.originalDocumentView=false;
  if (!status_.initialized || !eglMakeCurrent(display_,surface_,surface_,context_)) { FailLocked(EglError("Cannot bind frame context")); return; }
  UpdateColorStatusLocked();
  status_.yaw=yaw_; status_.pitch=pitch_; status_.distance=distance_;
  status_.basisMatrixVerified=status_.matrixUniformVerified=false;
  olex2::harmony::OriginalCameraMatrices camera;
  try { camera=olex2::harmony::BuildOriginalCamera(yaw_,pitch_,distance_,status_.width,status_.height); }
  catch(const std::exception& error) {
    FailLocked(error.what());
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return;
  }
  status_.basisMatrixVerified=camera.originalBasisVerified;
  status_.modelViewMatrix=camera.modelView;
  status_.modelViewDepth=camera.modelViewDepth;status_.projectionDepth=camera.projectionDepth;
  const auto& mvp=camera.mvp;
  const auto& normalMatrix=camera.normal;
  glViewport(0,0,status_.width,status_.height);
  glDisable(GL_SCISSOR_TEST); glClearColor(.055f,.078f,.10f,1.f); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST); glUseProgram(program_);
  const GLint mvpLocation=glGetUniformLocation(program_,"mvp");
  const GLint normalLocation=glGetUniformLocation(program_,"normalTransform");
  if(mvpLocation<0 || normalLocation<0) {
    FailLocked("Camera matrix uniform missing");
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return;
  }
  glUniformMatrix4fv(mvpLocation,1,GL_FALSE,mvp.data());
  glUniformMatrix4fv(normalLocation,1,GL_FALSE,normalMatrix.data());
  // Retain the actual GPU state, not a copy of the intended upload.
  glGetUniformfv(program_,mvpLocation,status_.mvpMatrix.data());
  glGetUniformfv(program_,normalLocation,status_.normalMatrix.data());
  status_.matrixUniformMaxError=0;
  bool finite=true;
  for(size_t i=0;i<16;++i) {
    finite=finite && std::isfinite(status_.mvpMatrix[i]) && std::isfinite(status_.normalMatrix[i]);
    status_.matrixUniformMaxError=std::max(status_.matrixUniformMaxError,
      std::max(std::abs(status_.mvpMatrix[i]-mvp[i]),std::abs(status_.normalMatrix[i]-normalMatrix[i])));
  }
  status_.matrixUniformVerified=finite && status_.matrixUniformMaxError<=2.e-5f && glGetError()==GL_NO_ERROR;
  if(!status_.matrixUniformVerified) {
    FailLocked("Camera matrix uniform readback differs from original basis state");
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    return;
  }
  glUniform3fv(glGetUniformLocation(program_,"ambientColor"),1,material_.Ambient().data());
  glUniform3fv(glGetUniformLocation(program_,"diffuseColor"),1,material_.Diffuse().data());
  glUniform3fv(glGetUniformLocation(program_,"emissionColor"),1,material_.Emission().data());
  glUniform1f(glGetUniformLocation(program_,"shininess"),material_.Shininess());
  status_.specularComparedPixels=status_.specularChangedPixels=0;
  status_.specularMaxError=status_.specularMaxDelta=0;status_.specularReferenceVerified=false;status_.specularSamples.clear();
  std::vector<GLubyte> baseline;
  if(materialPreset_==3 && !wireframe_) {
    // Same actual mesh/camera/framebuffer, with only the specular uniform zeroed.
    // Draw the real material afterwards, so the displayed frame is not diagnostic data.
    baseline.resize(static_cast<size_t>(status_.width)*4);
    glUniform3f(glGetUniformLocation(program_,"specularColor"),0,0,0);
    glBindVertexArray(vao_);glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(status_.meshIndices),GL_UNSIGNED_INT,nullptr);
    glReadPixels(0,status_.height/2,status_.width,1,GL_RGBA,GL_UNSIGNED_BYTE,baseline.data());
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  }
  glUniform3fv(glGetUniformLocation(program_,"specularColor"),1,material_.Specular().data());
  glBindVertexArray(vao_);
  glBeginQuery(GL_ANY_SAMPLES_PASSED,fragment_query_);
  glDrawElements(wireframe_?GL_LINES:GL_TRIANGLES,static_cast<GLsizei>(status_.meshIndices),GL_UNSIGNED_INT,nullptr);
  glEndQuery(GL_ANY_SAMPLES_PASSED);
  glBindVertexArray(0);
  GLuint samples=0; glGetQueryObjectuiv(fragment_query_,GL_QUERY_RESULT,&samples);
  const bool covered=samples!=0 && glGetError()==GL_NO_ERROR;
  GLubyte center[4]{}; glReadPixels(status_.width/2,status_.height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
  const bool center_verified=olex2::harmony::MatchesLitColor(center,color_.Rgba()) && glGetError()==GL_NO_ERROR;
  // A torus legitimately has a hole at its centre. Sample the real central
  // framebuffer row, excluding the separate marker, rather than inventing a
  // centre-pixel requirement for all shapes. Bounded to at most 64 KiB.
  std::vector<GLubyte> row(static_cast<size_t>(status_.width)*4);
  glReadPixels(0,status_.height/2,status_.width,1,GL_RGBA,GL_UNSIGNED_BYTE,row.data());
  if(!baseline.empty()) {
    try {
      const auto reference=olex2::harmony::ReferenceLightingRow(mesh_,mvp,normalMatrix,status_.width,status_.height,material_.Shininess());
      bool valid=true;
      for(const auto& sample:reference) {
        const size_t offset=sample.x*4;float error=0,delta=0;
        for(unsigned c=0;c<3;++c) {
          const float expectedBase=255.f*(material_.Ambient()[c]+material_.Diffuse()[c]*sample.diffuse+material_.Emission()[c]);
          const float expected=expectedBase+255.f*material_.Specular()[c]*sample.specular;
          error=std::max({error,std::abs(row[offset+c]-expected),std::abs(baseline[offset+c]-expectedBase)});
          delta=std::max(delta,float(row[offset+c])-baseline[offset+c]);
          if(int(row[offset+c])+1<int(baseline[offset+c]))valid=false;
        }
        ++status_.specularComparedPixels;if(delta>1)++status_.specularChangedPixels;
        status_.specularMaxError=std::max(status_.specularMaxError,error);status_.specularMaxDelta=std::max(status_.specularMaxDelta,delta);
      }
      // Retain evenly distributed raw samples for independent report-side math.
      for(unsigned i=0;i<4 && !reference.empty();++i) {
        const auto& sample=reference[(reference.size()-1)*i/3];SpecularPixelSample pixel;
        pixel.x=sample.x;pixel.diffuse=sample.diffuse;pixel.specular=sample.specular;
        for(unsigned c=0;c<3;++c){pixel.actual[c]=row[sample.x*4+c];pixel.baseline[c]=baseline[sample.x*4+c];}
        status_.specularSamples.push_back(pixel);
      }
      status_.specularReferenceVerified=valid&&reference.size()>=4&&status_.specularMaxError<=3.f&&glGetError()==GL_NO_ERROR;
    }catch(const std::exception&){status_.specularReferenceVerified=false;}
  }
  unsigned colored=0;
  double sumScale=0; float minScale=2,maxScale=0;
  status_.readbackR=status_.readbackG=status_.readbackB=status_.readbackA=0;
  for(size_t i=0;i<row.size();i+=4) if(olex2::harmony::MatchesLitColor(&row[i],color_.Rgba())) {
    ++colored;
    const float scale=olex2::harmony::LitColorScale(&row[i],color_.Rgba());
    sumScale+=scale; minScale=std::min(minScale,scale); maxScale=std::max(maxScale,scale);
    status_.readbackR=row[i]; status_.readbackG=row[i+1]; status_.readbackB=row[i+2]; status_.readbackA=row[i+3];
  }
  const float minimumScale[]={.29f,.59f,.99f,.29f};
  const bool rowVerified=glGetError()==GL_NO_ERROR && colored>0;
  const bool materialVerified=rowVerified && minScale>=minimumScale[materialPreset_] && maxScale<=1.02f;
  status_.rowMeanScale=colored?static_cast<float>(sumScale/colored):0;
  status_.rowMinScale=colored?minScale:0; status_.rowMaxScale=colored?maxScale:0;
  status_.materialVerified=materialVerified;
  const bool geometry=covered && status_.meshValidated && status_.submissionValidated && (wireframe_ || (materialVerified && (meshKind_!=olex2::harmony::MeshKind::Sphere || center_verified)));
  // Real framebuffer readback of a known synthetic marker, not a scientific model.
  glEnable(GL_SCISSOR_TEST); glScissor(0,0,2,2); glClearColor(0.f,1.f,0.f,1.f); glClear(GL_COLOR_BUFFER_BIT);
  GLubyte pixel[4]{}; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  const GLenum markerError=glGetError();
  bool verified=pixel[0]==0 && pixel[1]==255 && pixel[2]==0 && pixel[3]==255 && markerError==GL_NO_ERROR;
  if(!verified) {
    GLint draw=0,read=0,readBuffer=0,drawBuffer=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_READ_BUFFER,&readBuffer);glGetIntegerv(GL_DRAW_BUFFER0,&drawBuffer);
    OH_LOG_Print(LOG_APP,LOG_ERROR,DOMAIN,TAG,"OLEX2_WINDOW_READBACK rgba=%{public}u,%{public}u,%{public}u,%{public}u error=%{public}u draw=%{public}d read=%{public}d drawBuffer=%{public}d readBuffer=%{public}d",pixel[0],pixel[1],pixel[2],pixel[3],unsigned(markerError),draw,read,drawBuffer,readBuffer);
  }
  glDisable(GL_SCISSOR_TEST);
  bool swapped=eglSwapBuffers(display_,surface_)==EGL_TRUE;
  if (swapped) {
    ++status_.frames; status_.colorVerified=rowVerified; status_.geometryColorSamples=colored; status_.pixelVerified=verified; status_.geometryVerified=geometry; status_.coverageVerified=covered;
    status_.error = verified && geometry ? "" : "Framebuffer diagnostic did not pass";
  } else {
    status_.materialVerified=false; status_.colorVerified=false; status_.pixelVerified=false; status_.geometryVerified=false; status_.coverageVerified=false; status_.error=EglError("eglSwapBuffers");
  }
  eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,TAG,"OLEX2_FRAME frames=%{public}u size=%{public}dx%{public}d geometry=%{public}d pixel=%{public}d swap=%{public}d",
    status_.frames,status_.width,status_.height,geometry,verified,swapped);
}
void Renderer::ReleaseLocked() {
  bool current=display_!=EGL_NO_DISPLAY && surface_!=EGL_NO_SURFACE && context_!=EGL_NO_CONTEXT && eglMakeCurrent(display_,surface_,surface_,context_);
  if (current) {
    if (fragment_query_) glDeleteQueries(1,&fragment_query_);
    if (program_) glDeleteProgram(program_); if (vertex_buffer_) glDeleteBuffers(1,&vertex_buffer_);
    if (index_buffer_) glDeleteBuffers(1,&index_buffer_); if (vao_) glDeleteVertexArrays(1,&vao_);
  }
  status_.basisMatrixVerified=status_.matrixUniformVerified=false;status_.polygonProbe.verified=false;status_.linePathProbe=olex2::harmony::LinePathGpuProbe{};
  status_.originalLineVerified=status_.originalLineBufferVerified=false;
  status_.originalLineIndexSha256.clear();
  status_.originalPolygonVerified=false;
  fragment_query_=program_=vao_=vertex_buffer_=index_buffer_=0;
  status_.materialVerified=false; status_.colorVerified=false; status_.coverageVerified=false; status_.meshValidated=false; status_.submissionValidated=false;
  if (display_!=EGL_NO_DISPLAY) {
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if (surface_!=EGL_NO_SURFACE) eglDestroySurface(display_,surface_);
    if (context_!=EGL_NO_CONTEXT) eglDestroyContext(display_,context_);
    if(!originalView_.HasContext())eglTerminate(display_);
  }
  surface_=EGL_NO_SURFACE; context_=EGL_NO_CONTEXT;
  display_=originalView_.HasContext()?originalView_.Display():EGL_NO_DISPLAY;
  status_.originalDocumentView=false;
  if(originalRuntime_)originalStatus_=originalView_.Failure("Main surface detached; original document/context retained").json;
  if (window_) OH_NativeWindow_NativeObjectUnreference(window_);
  window_=nullptr;
}

olex2::harmony::FontDisplayMetrics Renderer::SetFontDisplayMetrics(double id,double x,double y,double density){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Set(id,x,y,density);
}
olex2::harmony::FontDisplayMetrics Renderer::ClearFontDisplayMetrics(){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Clear();
}
olex2::harmony::FontDisplayMetrics Renderer::GetFontDisplayMetrics(){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Get();
}
std::string Renderer::ProbeOriginalRenderer(void* runtime,const std::string& directory,bool loadTextures,bool fontProbe){
 std::lock_guard<std::mutex> lock(mutex_);
 if(originalRuntime_||originalView_.HasContext())return "{\"limited_path_verified\":false,\"error\":\"Close the original document before running isolated diagnostics\"}";
 if(!status_.initialized || !runtime)return "{\"limited_path_verified\":false,\"error\":\"Application runtime/window is not ready\"}";
 if(fontProbe&&!fontDisplay_.Get().ready)return "{\"limited_path_verified\":false,\"error\":\"Actual main-window display DPI is unavailable; no fixed96 fallback\"}";
 const EGLDisplay oldDisplay=eglGetCurrentDisplay();const EGLSurface oldDraw=eglGetCurrentSurface(EGL_DRAW),oldRead=eglGetCurrentSurface(EGL_READ);const EGLContext oldContext=eglGetCurrentContext();
 const auto before=status_;SharedOriginalWindowResult probe;
 if(eglMakeCurrent(display_,surface_,surface_,context_))probe=RunSharedOriginalWindow(display_,surface_,context_,status_.width,status_.height,runtime,directory,loadTextures,fontProbe,fontDisplay_.Get());
 DrawLocked();
 const bool graphics=status_.initialized&&status_.surfaceReady&&status_.meshValidated&&status_.submissionValidated&&status_.coverageVerified&&status_.geometryVerified&&status_.pixelVerified&&status_.matrixUniformVerified&&status_.error.empty()&&status_.frames==before.frames+1;
 const bool selection=before.meshRevision==status_.meshRevision&&before.colorRevision==status_.colorRevision&&before.materialRevision==status_.materialRevision&&before.meshKind==status_.meshKind&&before.meshGeneration==status_.meshGeneration&&before.wireframe==status_.wireframe&&before.yaw==status_.yaw&&before.pitch==status_.pitch&&before.distance==status_.distance;
 const bool external=eglMakeCurrent(oldDisplay==EGL_NO_DISPLAY?display_:oldDisplay,oldDraw,oldRead,oldContext)&&eglGetCurrentDisplay()==oldDisplay&&eglGetCurrentContext()==oldContext&&eglGetCurrentSurface(EGL_DRAW)==oldDraw&&eglGetCurrentSurface(EGL_READ)==oldRead;
 std::ostringstream out;out<<"{\"run\":"<<probe.run<<",\"limited_path_verified\":"<<(probe.completed&&graphics&&selection&&external?"true":"false")<<",\"main_graphics_revalidated\":"<<(graphics?"true":"false")<<",\"main_selection_unchanged\":"<<(selection?"true":"false")<<",\"external_egl_restored\":"<<(external?"true":"false")<<",\"main_frames_before\":"<<before.frames<<",\"main_frames_after\":"<<status_.frames<<",\"original\":"<<(probe.json.empty()?"null":probe.json)<<'}';
 std::ofstream file(directory+(fontProbe?"/shared-original-font-run-":loadTextures?"/shared-original-texture-load-run-":"/shared-original-integrated-run-")+std::to_string(probe.run)+".json");file<<out.str()<<'\n';return out.str();
}


void Renderer::ClearDemoQualificationLocked() {
  status_.originalDocumentView=false;status_.originalDocumentFrames=originalFrames_;
  status_.pixelVerified=status_.geometryVerified=status_.coverageVerified=false;
  status_.meshValidated=status_.submissionValidated=false;status_.submittedVertices=0;
  status_.submissionSource.clear();status_.meshSource.clear();status_.meshKind="original-document";
  status_.meshVertices=status_.meshTriangles=status_.meshIndices=0;
  status_.sourceBatches=status_.discardedTailVertices=status_.degenerateTrianglesSkipped=0;
  status_.geometryColorSamples=0;status_.readbackR=status_.readbackG=status_.readbackB=status_.readbackA=0;
  status_.originalTriangleVerified=status_.originalLineVerified=status_.originalLineBufferVerified=status_.originalPolygonVerified=false;
  status_.originalTriangleVertices=status_.originalTriangleNormalCommands=status_.originalTriangleColorCommands=0;
  status_.originalLineVertices=status_.originalLineColorCommands=status_.originalLineWidthQueries=status_.originalLineWidthCommands=0;
  status_.originalPolygonBatches=status_.originalPolygonVertices=status_.originalPolygonNormalCommands=status_.originalPolygonColorCommands=0;
  status_.originalLineIndexSha256.clear();status_.polygonProbe=PolygonGpuProbe{};
  status_.linePathProbe=olex2::harmony::LinePathGpuProbe{};
  status_.basisMatrixVerified=status_.matrixUniformVerified=false;
  status_.modelViewMatrix={};status_.mvpMatrix={};status_.normalMatrix={};
  status_.modelViewDepth=status_.projectionDepth=0;status_.matrixUniformMaxError=0;
  status_.colorVerified=status_.materialVerified=status_.specularReferenceVerified=false;
  status_.rowMeanScale=status_.rowMinScale=status_.rowMaxScale=0;
  status_.specularComparedPixels=status_.specularChangedPixels=0;
  status_.specularMaxError=status_.specularMaxDelta=0;status_.specularSamples.clear();
}
ApplicationViewResult Renderer::RunOriginalDocumentLocked(Olex2DocumentRequest request) {
  ClearDemoQualificationLocked();
  status_.yaw=yaw_;status_.pitch=pitch_;status_.distance=distance_;
  // ResetView deliberately keeps the request defaults for its new fitted basis.
  // Every ordinary render/capture/restore continues to use the retained camera.
  const bool resetView=request.operation==Olex2DocumentOperation::ResetView;
  if(!resetView){request.yaw=yaw_;request.pitch=pitch_;request.distance=distance_;}
  ApplicationViewResult result=originalView_.Run(display_,surface_,context_,status_.width,status_.height,
      originalRuntime_,originalDirectory_,std::move(request),fontDisplay_.Get());
  if(resetView&&result.passed&&result.corePassed&&result.loaded&&result.surfacePresented&&result.parentContextRestored){
    const Olex2DocumentRequest defaults;
    yaw_=defaults.yaw;pitch_=defaults.pitch;distance_=defaults.distance;
    status_.yaw=yaw_;status_.pitch=pitch_;status_.distance=distance_;
  }
  // Only a real core camera observation can synchronize a viewport operation.
  // This also handles core restore followed by failed swap: no false rollback.
  if(!result.viewportJson.empty()){
    const auto observed=olex2::harmony::ParseOriginalDocumentViewport(result.viewportJson);
    yaw_=observed.yaw;pitch_=observed.pitch;distance_=observed.distance;
    status_.yaw=yaw_;status_.pitch=pitch_;status_.distance=distance_;
  }
  originalStatus_=result.json;status_.error=result.error;
  if(result.passed&&result.surfacePresented&&result.loaded){
    ++originalFrames_;++status_.frames;status_.originalDocumentView=true;
  }
  status_.originalDocumentFrames=originalFrames_;
  return result;
}
ApplicationViewResult Renderer::RunOriginalViewportLocked(void* runtime,Olex2DocumentOperation operation,const std::string& snapshot){
  if(!runtime||!originalRuntime_||runtime!=originalRuntime_||originalDirectory_.empty()||!originalView_.HasContext())
    throw std::runtime_error("No current owned original viewport session is available");
  if(!status_.initialized||!status_.surfaceReady||surface_==EGL_NO_SURFACE||!status_.originalDocumentView)
    throw std::runtime_error("Original viewport has no current successful main-surface presentation");
  if(operation==Olex2DocumentOperation::RestoreViewport)olex2::harmony::ParseOriginalDocumentViewport(snapshot);
  Olex2DocumentRequest request;request.operation=operation;request.viewport_snapshot=snapshot;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(!result.passed||!result.surfacePresented||!result.parentContextRestored||!result.corePassed||!result.loaded||result.viewportJson.empty())
    throw std::runtime_error(result.error.empty()?"Original viewport operation/presentation did not complete":result.error);
  return result;
}
std::string Renderer::ResetOriginalDocumentView(void* runtime){
  // Lock order stays RuntimeOwner -> Renderer -> existing core; no runtime,
  // source or session replacement, and no fallback to a demo/default Render.
  std::lock_guard<std::mutex> lock(mutex_);
  if(!runtime||!originalRuntime_||runtime!=originalRuntime_||originalDirectory_.empty()||!originalView_.HasContext())
    throw std::runtime_error("No current owned original document session is available for reset");
  if(!status_.initialized||!status_.surfaceReady||!status_.originalDocumentView||
      display_==EGL_NO_DISPLAY||surface_==EGL_NO_SURFACE||context_==EGL_NO_CONTEXT||status_.width<=0||status_.height<=0)
    throw std::runtime_error("Original document reset requires the actual successfully presented main viewport");
  Olex2DocumentRequest request;request.operation=Olex2DocumentOperation::ResetView;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(!result.passed||!result.corePassed||!result.loaded||!result.surfacePresented||!result.parentContextRestored)
    throw std::runtime_error(result.error.empty()?"Original document reset/presentation did not complete":result.error);
  return result.json;
}
std::string Renderer::CaptureOriginalDocumentViewport(void* runtime){
  std::lock_guard<std::mutex> lock(mutex_);
  return RunOriginalViewportLocked(runtime,Olex2DocumentOperation::CaptureViewport,"").viewportJson;
}
std::string Renderer::RestoreOriginalDocumentViewport(void* runtime,const std::string& snapshot){
  std::lock_guard<std::mutex> lock(mutex_);
  return RunOriginalViewportLocked(runtime,Olex2DocumentOperation::RestoreViewport,snapshot).json;
}
std::string Renderer::RejectOriginalDocumentOperation(const std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto result=originalView_.Failure(error);originalStatus_=result.json;status_.error=error;return originalStatus_;
}
std::string Renderer::ReportOriginalDocumentError(const std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_.error=error;
  return originalStatus_;
}
std::string Renderer::GetOriginalDocumentStatus() {
  std::lock_guard<std::mutex> lock(mutex_);return originalStatus_;
}
std::string Renderer::RedrawOriginalDocument(void* runtime) {
  std::lock_guard<std::mutex> lock(mutex_);
  // A best-effort redraw must never replace the retained document status with a
  // failure envelope: the document itself is still loaded and the page would
  // otherwise report it as unloaded. Report the error through the graphics
  // status only.
  auto reject=[&](const std::string& error){
    status_.error=error;return originalStatus_;
  };
  if(!runtime)return reject("Original application runtime is not initialized");
  if(!originalRuntime_||!originalView_.HasContext()||originalDirectory_.empty())
    return reject("No retained original document session is available");
  if(runtime!=originalRuntime_)return reject("Retained original document belongs to another runtime");
  if(!status_.initialized||!status_.surfaceReady||display_==EGL_NO_DISPLAY||
      surface_==EGL_NO_SURFACE||context_==EGL_NO_CONTEXT)
    return reject("Original document redraw requires the actual main viewport surface");
  if(!status_.originalDocumentView)
    return reject("No presented original document to redraw");
  DrawLocked();
  return originalStatus_;
}
std::string Renderer::OpenOriginalDocument(void* runtime,const std::string& directory,
    const std::string& name,const std::string& bytes) {
  std::lock_guard<std::mutex> lock(mutex_);std::string error;
  if(!runtime||directory.empty())error="Original application runtime is not initialized";
  else if(originalRuntime_&&runtime!=originalRuntime_)error="Cannot replace the owner of a retained original document";
  else if(!status_.initialized||surface_==EGL_NO_SURFACE)error="Open the actual main viewport before opening an original document";
  else ValidateOriginalDocumentInput(name,bytes,error);
  if(!error.empty()){originalStatus_=originalView_.Failure(error).json;status_.error=error;return originalStatus_;}
  const bool existingSession=originalRuntime_!=nullptr;
  if(!existingSession){
    originalDemoYaw_=yaw_;originalDemoPitch_=pitch_;originalDemoDistance_=distance_;
    originalDemoCameraRetained_=true;
  }
  originalRuntime_=runtime;originalDirectory_=directory;
  Olex2DocumentRequest request;request.operation=Olex2DocumentOperation::Open;request.name=name;request.bytes=bytes;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(!existingSession&&!result.coreInvoked&&!originalView_.HasContext()){
    // No original GL session was ever entered, so there is no borrowed runtime
    // to keep alive or clean up. Preserve the failure envelope, not fake success.
    originalRuntime_=nullptr;originalDirectory_.clear();originalDemoCameraRetained_=false;
  }
  return originalStatus_;
}
std::string Renderer::InteractOriginalDocument(void* runtime,const std::string& action,
    double x,double y,unsigned mode) {
  // RuntimeOwner -> Renderer -> existing application/core guard. Never acquire
  // the owner here, replace its borrowed pointer, or create a document session.
  std::lock_guard<std::mutex> lock(mutex_);
  auto reject=[&](const std::string& error){
    originalStatus_=originalView_.Failure(error).json;status_.error=error;return originalStatus_;
  };
  Olex2DocumentRequest request;
  if(action=="pick")request.operation=Olex2DocumentOperation::Pick;
  else if(action=="clear-selection")request.operation=Olex2DocumentOperation::ClearSelection;
  else if(action=="select-all")request.operation=Olex2DocumentOperation::SelectAll;
  else if(action=="invert-selection")request.operation=Olex2DocumentOperation::InvertSelection;
  else if(action=="toggle-cell")request.operation=Olex2DocumentOperation::ToggleCellVisibility;
  else if(action=="toggle-basis")request.operation=Olex2DocumentOperation::ToggleBasisVisibility;
  else if(action=="toggle-frame")request.operation=Olex2DocumentOperation::ToggleFrameVisibility;
  else if(action=="toggle-grid")request.operation=Olex2DocumentOperation::ToggleGridVisibility;
  else if(action=="show-all")request.operation=Olex2DocumentOperation::ShowAllVisible;
  else if(action=="center-model")request.operation=Olex2DocumentOperation::CenterModel;
  else if(action=="view-along-100")request.operation=Olex2DocumentOperation::ViewAlong100;
  else if(action=="view-along-010")request.operation=Olex2DocumentOperation::ViewAlong010;
  else if(action=="view-along-001")request.operation=Olex2DocumentOperation::ViewAlong001;
  else if(action=="view-along-110")request.operation=Olex2DocumentOperation::ViewAlong110;
  else if(action=="view-along-101")request.operation=Olex2DocumentOperation::ViewAlong101;
  else if(action=="view-along-011")request.operation=Olex2DocumentOperation::ViewAlong011;
  else if(action=="view-along-111")request.operation=Olex2DocumentOperation::ViewAlong111;
  else if(action=="view-along-selection")request.operation=Olex2DocumentOperation::ViewAlongSelection;
  else if(action=="hide-selection")request.operation=Olex2DocumentOperation::HideSelection;
  else if(action=="quality-1")request.operation=Olex2DocumentOperation::SetDrawingQualityLow;
  else if(action=="quality-2")request.operation=Olex2DocumentOperation::SetDrawingQualityMedium;
  else if(action=="quality-3")request.operation=Olex2DocumentOperation::SetDrawingQualityHigh;
  else if(action=="toggle-hydrogens")request.operation=Olex2DocumentOperation::ToggleHydrogensVisibility;
  else if(action=="toggle-hydrogen-bonds")request.operation=Olex2DocumentOperation::ToggleHydrogenBondsVisibility;
  else if(action=="toggle-qpeaks")request.operation=Olex2DocumentOperation::ToggleQPeaksVisibility;
  else if(action=="toggle-qpeak-bonds")request.operation=Olex2DocumentOperation::ToggleQPeakBondsVisibility;
  else if(action=="toggle-labels")request.operation=Olex2DocumentOperation::ToggleLabelsVisibility;
  else if(action=="toggle-structure")request.operation=Olex2DocumentOperation::ToggleStructureVisibility;
  else if(action=="fragment-select")request.operation=Olex2DocumentOperation::FragmentSelectAll;
  else if(action=="fragment-select-atoms")request.operation=Olex2DocumentOperation::FragmentSelectAtoms;
  else if(action=="fragment-select-bonds")request.operation=Olex2DocumentOperation::FragmentSelectBonds;
  else if(action=="fragment-hide")request.operation=Olex2DocumentOperation::FragmentHide;
  else if(action=="fragment-show-only")request.operation=Olex2DocumentOperation::FragmentShowOnly;
  else return reject("Unknown original document interaction action");
  if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1||mode>2)
    return reject("Expected finite normalized viewport coordinates in [0,1] and selection mode 0, 1 or 2");
  if(!runtime)return reject("Original application runtime is not initialized");
  if(!originalRuntime_||!originalView_.HasContext()||originalDirectory_.empty())
    return reject("No retained original document session is available for selection");
  if(runtime!=originalRuntime_)return reject("Retained original document belongs to another runtime");
  if(!status_.initialized||!status_.surfaceReady||display_==EGL_NO_DISPLAY||
      surface_==EGL_NO_SURFACE||context_==EGL_NO_CONTEXT||status_.width<=0||status_.height<=0)
    return reject("Original document selection requires the actual main viewport surface");
  request.pointer_x=x;request.pointer_y=y;request.selection_mode=mode;
  const bool viewAlong=request.operation==Olex2DocumentOperation::ViewAlong100||
      request.operation==Olex2DocumentOperation::ViewAlong010||request.operation==Olex2DocumentOperation::ViewAlong001||
      request.operation==Olex2DocumentOperation::ViewAlong110||request.operation==Olex2DocumentOperation::ViewAlong101||
      request.operation==Olex2DocumentOperation::ViewAlong011||request.operation==Olex2DocumentOperation::ViewAlong111||
      request.operation==Olex2DocumentOperation::ViewAlongSelection;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(viewAlong&&result.passed&&result.corePassed&&result.loaded&&result.surfacePresented&&result.parentContextRestored){
    // The original direction command replaces the basis, so its relative
    // mouse-camera angles start at zero while the molecular document is retained.
    yaw_=0;pitch_=0;distance_=4.4;
    status_.yaw=yaw_;status_.pitch=pitch_;status_.distance=distance_;
  }
  return originalStatus_; // actual core/presentation envelope, including failures
}
bool Renderer::ExportOriginalDocument(std::string& bytes,std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);bytes.clear();
  if(!originalRuntime_){error="No original document is open";originalStatus_=originalView_.Failure(error).json;return false;}
  Olex2DocumentRequest request;request.operation=Olex2DocumentOperation::Export;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(!result.passed){error=result.error;return false;}
  bytes=result.exportedBytes;error.clear();return true;
}
bool Renderer::CloseOriginalDocumentLocked(void* runtime,const std::string& directory,std::string& error) {
  if(originalRuntime_&&originalRuntime_!=runtime){error="Retained original document belongs to another runtime";originalStatus_=originalView_.Failure(error).json;return false;}
  if(!runtime){error="Original application runtime is not initialized";originalStatus_=originalView_.Failure(error).json;return false;}
  originalRuntime_=runtime;originalDirectory_=directory;
  Olex2DocumentRequest request;request.operation=Olex2DocumentOperation::Close;
  const auto result=RunOriginalDocumentLocked(std::move(request));
  if(!result.passed||result.loaded||originalView_.HasContext()){
    error=result.error.empty()?"Original context cleanup has not completed; runtime retained":result.error;return false;
  }
  originalRuntime_=nullptr;originalDirectory_.clear();status_.originalDocumentView=false;
  // Only successful original cleanup releases this snapshot. Failed Close or
  // surface loss retains it alongside the same owner, for deferred cleanup.
  if(originalDemoCameraRetained_){
    yaw_=originalDemoYaw_;pitch_=originalDemoPitch_;distance_=originalDemoDistance_;
    status_.yaw=yaw_;status_.pitch=pitch_;status_.distance=distance_;
    originalDemoCameraRetained_=false;
  }
  // If the window disappeared, release only our idle display. Never terminate
  // a display carrying an unrelated caller's current context.
  if(context_==EGL_NO_CONTEXT&&surface_==EGL_NO_SURFACE&&display_!=EGL_NO_DISPLAY&&
      (eglGetCurrentContext()==EGL_NO_CONTEXT||eglGetCurrentDisplay()!=display_)){
    eglTerminate(display_);display_=EGL_NO_DISPLAY;
  }
  error.clear();return true;
}
std::string Renderer::CloseOriginalDocument(void* runtime,const std::string& directory) {
  std::lock_guard<std::mutex> lock(mutex_);std::string error;
  if(CloseOriginalDocumentLocked(runtime,directory,error)&&status_.initialized)SetSelectionLocked(generation_,wireframe_,meshKind_);
  return originalStatus_; // document Close result, not the demo verification
}
bool Renderer::ForgetOriginalDocumentRuntime(void* runtime,std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if(!originalRuntime_&&!originalView_.HasContext())return true;
  // No reset of the core owner until Close has run on its retained context,
  // either on the actual surface or the same-config cleanup pbuffer.
  return CloseOriginalDocumentLocked(runtime,originalDirectory_,error);
}
