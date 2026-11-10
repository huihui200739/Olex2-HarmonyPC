#pragma once
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <mutex>
#include <string>
#include "upstream_mesh.h"
#include "primitive_batch.h"
#include "primitive_triangles.h"
#include "upstream_color.h"
#include "upstream_material.h"
#include "material_commands.h"
#include "lit_row_reference.h"
#include "original_matrices.h"
#include "primitive_polygons.h"
#include "primitive_lines.h"
#include "polygon_probe.h"
#include "line_path_probe.h"
#include "desktop_commands.h"

struct SpecularPixelSample {
  unsigned x=0; float diffuse=0,specular=0;
  std::array<unsigned,3> actual{},baseline{};
};
struct GraphicsStatus {
  bool surfaceReady = false;
  bool initialized = false;
  bool pixelVerified = false;
  bool geometryVerified = false;
  bool coverageVerified = false;
  bool meshValidated = false;
  bool submissionValidated = false;
  unsigned submittedVertices = 0;
  std::string submissionSource;
  bool originalTriangleVerified=false;
  unsigned originalTriangleVertices=0, originalTriangleNormalCommands=0, originalTriangleColorCommands=0;
  std::string originalTriangleSource;
  bool originalLineVerified=false, originalLineBufferVerified=false;
  unsigned originalLineVertices=0,originalLineColorCommands=0,originalLineWidthQueries=0,originalLineWidthCommands=0;
  std::string originalLineSource=olex2::harmony::OriginalLineSource(),originalLineIndexSha256;
  bool originalPolygonVerified=false;
  unsigned originalPolygonBatches=0,originalPolygonVertices=0,originalPolygonNormalCommands=0,originalPolygonColorCommands=0;
  std::string originalPolygonSource=olex2::harmony::OriginalPolygonSource();
  PolygonGpuProbe polygonProbe;
  olex2::harmony::LinePathGpuProbe linePathProbe;
  bool basisMatrixVerified=false,matrixUniformVerified=false;
  std::string basisMatrixSource=olex2::harmony::OriginalMatrixSource();
  unsigned modelViewDepth=0,projectionDepth=0;
  float matrixUniformMaxError=0;
  // modelView is logical CPU state; MVP and normal are actual uniform readback.
  olex2::harmony::Matrix4 modelViewMatrix{},mvpMatrix{},normalMatrix{};
  std::string meshKind = "sphere";
  unsigned sourceBatches = 0, discardedTailVertices = 0, degenerateTrianglesSkipped = 0, geometryColorSamples = 0;
  unsigned colorPreset = 0, colorRevision = 0;
  std::string colorSource = olex2::harmony::OpaqueColor::Source();
  uint32_t colorPacked = 4288072755u;
  float colorR=.20f, colorG=.80f, colorB=.59f, colorA=1.f;
  unsigned readbackR=0, readbackG=0, readbackB=0, readbackA=0;
  bool colorVerified=false;
  unsigned materialPreset=0, materialRevision=0, materialFlags=783;
  std::string materialSource=olex2::harmony::OpaqueMaterial::Source(), materialCanonical;
  float ambientR=0,ambientG=0,ambientB=0,diffuseR=0,diffuseG=0,diffuseB=0,emissionR=0,emissionG=0,emissionB=0;
  float rowMeanScale=0,rowMinScale=0,rowMaxScale=0;
  bool materialVerified=false;
  float specularR=0,specularG=0,specularB=0,shininess=0;
  unsigned specularComparedPixels=0,specularChangedPixels=0;
  float specularMaxError=0,specularMaxDelta=0;
  bool specularReferenceVerified=false;
  std::vector<SpecularPixelSample> specularSamples;
  unsigned materialCommandCount=0;
  std::string materialCommandsSource=olex2::harmony::MaterialCommands::Source();
  bool wireframe = false;
  unsigned meshGeneration = 3, meshRevision = 0;
  unsigned meshVertices = 0, meshTriangles = 0, meshIndices = 0;
  std::string meshSource;
  int width = 0;
  int height = 0;
  unsigned frames = 0;
  float yaw = 32.f, pitch = -22.f, distance = 4.4f;
  std::string version;
  std::string renderer;
  std::string error;
};
class Renderer {
public:
  static Renderer& Instance();
  void Attach(OH_NativeXComponent* component, void* window);
  void Resize(OH_NativeXComponent* component, void* window);
  void Destroy();
  GraphicsStatus Status();
  std::string ProbeOriginalRenderer(void* runtime,const std::string& privateDirectory);
  void SetView(float yaw, float pitch, float distance);
  void SetMesh(unsigned generation, bool wireframe);
  bool ApplyDesktopCommandState(const olex2::harmony::DesktopCommandState& state,std::string& error);
  bool SetMaterialPreset(unsigned preset);
  bool SetColorPreset(unsigned preset);
  void SetPrimitive(olex2::harmony::MeshKind kind);
private:
  std::mutex mutex_;
  GraphicsStatus status_;
  void* window_ = nullptr;
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;
  GLuint program_ = 0, vao_ = 0, vertex_buffer_ = 0, index_buffer_ = 0;
  GLuint fragment_query_ = 0;
  olex2::harmony::MeshData mesh_;
  olex2::harmony::MeshKind meshKind_ = olex2::harmony::MeshKind::Sphere;
  unsigned generation_ = 3;
  bool wireframe_ = false;
  olex2::harmony::OpaqueColor color_;
  unsigned colorPreset_=0, colorRevision_=0, materialPreset_=0, materialRevision_=0;
  olex2::harmony::OpaqueMaterial material_;
  void UpdateColorStatusLocked();
  bool UploadMeshLocked(olex2::harmony::MeshData mesh, bool wireframe);
  float yaw_ = 32.f, pitch_ = -22.f, distance_ = 4.4f;
  void SetSelectionLocked(unsigned generation, bool wireframe, olex2::harmony::MeshKind kind);
  bool InitializeLocked();
  void DrawLocked();
  void ReleaseLocked();
  bool DimensionsLocked(OH_NativeXComponent* component, void* window);
  bool FailLocked(const std::string& message);
};
