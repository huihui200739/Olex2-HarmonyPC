// Synthetic geometry only: verifies the extracted GlSphereEx loop, not a GL context.
#include "spheresubmit.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
unsigned checks=0;
void Check(bool ok,const char* name) {
  if (!ok) throw std::runtime_error(name);
  ++checks; std::cout << "PASS " << name << '\n';
}
struct Sink {
  unsigned begins=0,ends=0;
  std::vector<std::array<float,3>> normals,vertices;
  void BeginTriangles() { ++begins; }
  void Normal(const vec3f& v) { normals.push_back({v[0],v[1],v[2]}); }
  void Vertex(const vec3f& v) { vertices.push_back({v[0],v[1],v[2]}); }
  void End() { ++ends; }
};
void VerifyOrder(const Sink& sink,const TTypeList<vec3f>& vertices,
    const TTypeList<IndexTriangle>& faces,const TArrayList<vec3f>& normals,
    const std::vector<size_t>& selected) {
  size_t emitted=0;
  Check(sink.begins==1 && sink.ends==1,"one complete triangle batch");
  Check(sink.vertices.size()==selected.size()*3 && sink.normals.size()==sink.vertices.size(),"expected emitted triangle count");
  bool same=true;
  for (auto face:selected) for (size_t j=0;j<3;++j) {
    const auto index=faces[face][j];
    for (size_t k=0;k<3;++k)
      same=same && sink.vertices[emitted][k]==vertices[index][k] && sink.normals[emitted][k]==normals[index][k];
    ++emitted;
  }
  Check(same,"original face order, vertex order and normals preserved");
}
}
int main() {
  try {
    TTypeList<vec3f> vertices; TTypeList<IndexTriangle> faces; TArrayList<vec3f> normals;
    OlxSphere<float,OctahedronFP<vec3f>>::Generate(.9f,2,vertices,faces,normals);
    std::vector<size_t> all,lower;
    for (size_t i=0;i<faces.Count();++i) {
      all.push_back(i);
      // Independent selection for the mask that includes the upper hemisphere.
      if (!(vertices[faces[i][0]][2]>=0 && vertices[faces[i][1]][2]>=0 && vertices[faces[i][2]][2]>=0))
        lower.push_back(i);
    }
    Sink full; glObj::SphereTriangleSubmission<float>::Render(vertices,faces,normals,full);
    VerifyOrder(full,vertices,faces,normals,all);
    Sink outside; glObj::SphereTriangleSubmission<float>::RenderEx(vertices,faces,normals,vec3f(2,2,2),vec3f(3,3,3),outside);
    VerifyOrder(outside,vertices,faces,normals,all);
    Sink whole; glObj::SphereTriangleSubmission<float>::RenderEx(vertices,faces,normals,vec3f(-1,-1,-1),vec3f(1,1,1),whole);
    VerifyOrder(whole,vertices,faces,normals,{});
    Sink upper; glObj::SphereTriangleSubmission<float>::RenderEx(vertices,faces,normals,vec3f(-1,-1,0),vec3f(1,1,1),upper);
    Check(lower.size()>0 && lower.size()<all.size(),"synthetic inclusive mask removes only the complete upper faces");
    VerifyOrder(upper,vertices,faces,normals,lower);
    std::cout << "PASS shared GlSphereEx Render/RenderEx; " << checks << " assertions; not the complete renderer\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
