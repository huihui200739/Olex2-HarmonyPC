#include "upstream_mesh.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
unsigned checks=0;
void Check(bool condition,const std::string& name) {
  if (!condition) throw std::runtime_error(name);
  ++checks; std::cout << "PASS " << name << '\n';
}
template <class F> void Reject(F action,const char* name) {
  bool rejected=false;
  try { action(); } catch (const std::exception&) { rejected=true; }
  Check(rejected,name);
}
}
int main() {
  using namespace olex2::harmony;
  try {
    for (unsigned gen=0;gen<=5;++gen) {
      const auto mesh=BuildUpstreamSphere(gen);
      const auto faces=size_t(8)<<(2*gen);
      Check(mesh.validated,"generation "+std::to_string(gen)+" radius/normals/winding/indices validated");
      Check(mesh.vertices.size()==faces-2 && mesh.triangles.size()==faces*3,"generation "+std::to_string(gen)+" actual upstream mesh counts");
      Check(mesh.submissionValidated && mesh.submittedVertices.size()==faces*3 && mesh.drawTriangles.size()==faces*3 && mesh.drawLines.size()==faces*6,
        "generation "+std::to_string(gen)+" shared GlSphereEx stream validated and drawable");
      Check(mesh.lines.size()==faces*6,"generation "+std::to_string(gen)+" triangle-edge line indices");
    }
    Check(BuildUpstreamSphere(2,2.f).radius==2.f,"alternate radius supported");
    Check(BuildUpstreamSphere(5,.01f).validated,"minimum radius supported at detail five");
    Check(BuildUpstreamSphere(5,10.f).validated,"maximum radius supported at detail five");
    Reject([]{ BuildUpstreamSphere(6); },"unsupported detail level rejected before generation");
    Reject([]{ BuildUpstreamSphere(1,0); },"zero radius rejected");
    Reject([]{ BuildUpstreamSphere(1,std::numeric_limits<float>::quiet_NaN()); },"non-finite radius rejected");
    auto mesh=BuildUpstreamSphere(1); mesh.triangles[0]=static_cast<uint32_t>(mesh.vertices.size());
    Reject([&]{ ValidateMesh(mesh); },"inconsistent adapter triangle index rejected");
    mesh=BuildUpstreamSphere(1); mesh.vertices[0].normal[0]=1.f;
    Reject([&]{ ValidateMesh(mesh); },"inconsistent adapter normal rejected");
    mesh=BuildUpstreamSphere(1); std::swap(mesh.triangles[0],mesh.triangles[1]);
    Reject([&]{ ValidateMesh(mesh); },"reversed adapter triangle rejected");
    mesh=BuildUpstreamSphere(1); mesh.lines[0]=(mesh.lines[0]+1)%mesh.vertices.size();
    Reject([&]{ ValidateMesh(mesh); },"inconsistent adapter edge rejected");
    mesh=BuildUpstreamSphere(1); mesh.submittedVertices[0].position[0]+=.1f;
    Reject([&]{ ValidateMesh(mesh); },"isolated synthetic test batch with inconsistent submitted position rejected");
    mesh=BuildUpstreamSphere(1); mesh.submittedVertices[0].normal[0]+=.1f;
    Reject([&]{ ValidateMesh(mesh); },"isolated synthetic test batch with inconsistent submitted normal rejected");
    mesh=BuildUpstreamSphere(1); mesh.drawTriangles.pop_back();
    Reject([&]{ ValidateMesh(mesh); },"isolated synthetic incomplete triangle batch rejected");
    mesh=BuildUpstreamSphere(1); mesh.drawLines[0]=9;
    Reject([&]{ ValidateMesh(mesh); },"isolated synthetic inconsistent submitted edge rejected");
    std::cout << "PASS sphere adapter and shared triangle submission only; " << checks << " assertions; not the original Olex2 renderer\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
