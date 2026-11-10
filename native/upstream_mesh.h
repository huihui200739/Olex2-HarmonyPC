#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace olex2::harmony {
enum class MeshKind { Sphere=0, Torus=1, Disks=2, Rims=3 };
struct MeshVertex {
  std::array<float, 3> position;
  std::array<float, 3> normal;
};
struct MeshData {
  MeshKind kind = MeshKind::Sphere;
  unsigned sourceBatches = 0, discardedTailVertices = 0, degenerateTrianglesSkipped = 0;
  bool originalPolygonVerified=false;
  unsigned originalPolygonBatches=0,originalPolygonVertices=0,originalPolygonNormalCommands=0,originalPolygonColorCommands=0;
  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> triangles;
  std::vector<uint32_t> lines;
  // Actual normal/vertex stream emitted by the shared GlSphereEx loop.
  std::vector<MeshVertex> submittedVertices;
  std::vector<uint32_t> drawTriangles;
  std::vector<uint32_t> drawLines;
  std::string submissionSource;
  bool submissionValidated = false;
  unsigned generation = 0;
  float radius = 0;
  std::string source;
  bool validated = false;
};
// Uses the pinned upstream OlxSphere algorithm unchanged. No GL/GLU dependency.
MeshData BuildUpstreamSphere(unsigned generation, float radius = .9f);
// Independent checks on adapter output; throws on inconsistent geometry.
void ValidateMesh(const MeshData& mesh);
}
