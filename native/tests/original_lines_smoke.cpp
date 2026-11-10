// Synthetic geometry/state only; no original scene/business data or GPU-width
// equivalence claim. The actual window's uncolored path is tested separately.
#include "primitive_lines.h"
#include "primitive_batch.h"
#include "linesubmit.h"
#include "sha.h"
#include <cctype>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned checks=0,cases=0,meshes=0;
void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message);++checks; }
template<class F> void Reject(F action,const char* message) {
  bool failed=false;try { action(); } catch(const std::exception&) { failed=true; }Check(failed,message);
}
TArrayList<vec3f> Positions(size_t count) {
  TArrayList<vec3f> values(count);
  for(size_t i=0;i<count;++i)values[i]=vec3f(float(i/2),float(i%2),float(i%3));
  return values;
}
void OriginalBranches() {
  for(size_t count:{size_t(0),size_t(2),size_t(4),size_t(8)}) {
    auto values=Positions(count);
    for(size_t colorsCount:std::set<size_t>{0,count/2,count}) {
      TArrayList<uint32_t> colors(colorsCount);
      for(size_t i=0;i<colorsCount;++i)colors[i]=0xff000001u+uint32_t(i);
      for(bool selecting:{false,true})for(bool forcePlain:{false,true})
      for(uint32_t initialColor:{0xffffffffu,0xff123456u})
      for(float initialWidth:{1.f,3.f})for(double multiplier:{1.,.5,1.5,2.}) {
        LineCommandState initial{initialColor,initialWidth,forcePlain,selecting};
        const auto batch=CapturePrimitiveLines(values,colors,multiplier,initial);++cases;
        // Independent expected event/attribute calculation, not a second call
        // to the shared header or adapter helper.
        const bool uncolored=colorsCount==0 || selecting;
        const bool perVertex=!uncolored && colorsCount==count;
        const bool perPair=!uncolored && !perVertex && colorsCount*2==count;
        const unsigned requests=perVertex?unsigned(count):perPair?unsigned(count/2):0;
        Check(batch.vertices.size()==count,"original line vertex count");
        Check(batch.colorRequests==requests && batch.colorCommands==(forcePlain?0:requests),"original color requests and ForcePlain suppression");
        Check(batch.selectionQueries==(colorsCount==0?0:1),"original selection short circuit");
        Check(batch.widthQueries==(multiplier==1?0:1),"original width query count");
        const std::vector<float> expectedWidths=multiplier==1?std::vector<float>{}:std::vector<float>{float(multiplier*initialWidth),initialWidth};
        Check(batch.widthCommands==expectedWidths && batch.finalState.lineWidth==initialWidth,"original scaled width and restore order");
        uint32_t finalColor=initialColor;
        for(size_t i=0;i<count;++i) {
          const uint32_t expectedColor=forcePlain || uncolored?initialColor:colors[perVertex?i:i/2];
          Check(batch.vertices[i].position==std::array<float,3>{values[i][0],values[i][1],values[i][2]} && batch.vertices[i].inputVertex==i,"positions and input identity are preserved");
          Check(batch.vertices[i].color==expectedColor,"per-vertex/per-pair/inherited color");
          finalColor=expectedColor;
        }
        Check(batch.finalState.color==finalColor && batch.finalState.selecting==selecting && batch.finalState.forcePlain==forcePlain,"explicit final state");
      }
    }
  }
}
struct RawSink {
  size_t vertices=0,queries=0;float width=2;std::vector<float> widths;
  bool IsSelecting() { return false; }
  void GetLineWidth(float* value) { ++queries;*value=width; }
  void SetLineWidth(float value) { width=value;widths.push_back(value); }
  void SetColor(const uint32_t&) {}
  void DrawVertex(const vec3f&) { ++vertices; }
  void DrawVertex(const vec3f&,const uint32_t&) { ++vertices; }
  void DrawVertex2(size_t) { vertices+=2; }
};
void RawLegacyAndStrictBoundary() {
  auto values=Positions(4);TArrayList<uint32_t> invalidColors(3);double params[]={2};RawSink raw;
  glObj::SubmitPrimitiveLines(values,invalidColors,params,raw);
  Check(raw.vertices==0 && raw.queries==1 && raw.widths==std::vector<float>{4,2},"legacy malformed color branch still emits no vertices but restores width");
  auto odd=Positions(3);TArrayList<uint32_t> noColors;RawSink oddRaw;
  glObj::SubmitPrimitiveLines(odd,noColors,params,oddRaw);
  Check(oddRaw.vertices==3,"legacy header retains odd raw submission semantics");
  Reject([&]{CapturePrimitiveLines(values,invalidColors);},"strict boundary rejects malformed colors");
  Reject([&]{CapturePrimitiveLines(odd,noColors);},"strict boundary rejects odd pairs");
  LineCommandState selected;selected.selecting=true;
  Check(CapturePrimitiveLines(values,invalidColors,1,selected).vertices.size()==4,"selecting ignores colors exactly as original");
  auto duplicate=Positions(4);duplicate[2]=duplicate[0];const auto tagged=CapturePrimitiveLines(duplicate,noColors);
  Check(tagged.vertices[0].position==tagged.vertices[2].position && tagged.vertices[0].inputVertex==0 && tagged.vertices[2].inputVertex==2,"equal positions never alias source identities");
  values[0][1]=std::numeric_limits<float>::quiet_NaN();
  Reject([&]{CapturePrimitiveLines(values,noColors);},"reject nonfinite positions");values=Positions(4);
  for(double bad:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max(),std::numeric_limits<double>::min()})
    Reject([&]{CapturePrimitiveLines(values,noColors,bad);},"reject invalid or unrepresentable scaled width");
  for(float bad:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    LineCommandState state;state.lineWidth=bad;Reject([&]{CapturePrimitiveLines(values,noColors,1,state);},"reject invalid incoming width");
  }
  auto large=Positions(200002);Reject([&]{CapturePrimitiveLines(large,noColors);},"reject vertex budget overflow");
  Check(CapturePrimitiveLines(Positions(0),noColors).vertices.empty(),"empty strict batch is valid");
}
void MeshRouting() {
  for(auto kind:{MeshKind::Sphere,MeshKind::Torus,MeshKind::Disks,MeshKind::Rims})for(unsigned detail=0;detail<6;++detail) {
    const auto mesh=BuildUpstreamPrimitive(kind,detail);const auto upload=RouteMeshThroughOriginalLines(mesh);++meshes;
    Check(upload.verified && upload.indices==mesh.drawLines && upload.vertices==mesh.drawLines.size(),"original line output drives exact existing source-edge indices");
    Check(!upload.colorCommands && !upload.widthQueries && !upload.widthCommands,"GPU boundary is explicitly uncolored width-one only");
    std::string digest;
    const std::string grouped=SHA256::Digest(reinterpret_cast<const char*>(upload.indices.data()),upload.indices.size()*sizeof(uint32_t)).c_str();
    for(unsigned char c:grouped)if(c!=' ')digest+=static_cast<char>(std::tolower(c));
    std::cout<<"LINE_INDICES "<<MeshKindName(kind)<<' '<<detail<<' '<<upload.indices.size()<<' '<<digest<<'\n';
    for(size_t i=0;i<upload.indices.size();++i)
      Check(mesh.submittedVertices.at(upload.indices[i]).position==mesh.submittedVertices.at(mesh.drawLines[i]).position,"every actual source edge endpoint preserved");
  }
  MeshData empty;Reject([&]{RouteMeshThroughOriginalLines(empty);},"reject empty unvalidated upload");
  auto invalid=BuildUpstreamSphere(1);invalid.submissionValidated=false;Reject([&]{RouteMeshThroughOriginalLines(invalid);},"reject missing submission validation");
  invalid=BuildUpstreamSphere(1);invalid.drawLines[0]=uint32_t(invalid.submittedVertices.size());
  Reject([&]{RouteMeshThroughOriginalLines(invalid);},"reject corrupted source edge index");
  invalid=BuildUpstreamSphere(1);invalid.drawLines[0]=invalid.drawLines[1];
  Reject([&]{RouteMeshThroughOriginalLines(invalid);},"reject changed source edge order");
}
}
int main() {
  try {
    OriginalBranches();RawLegacyAndStrictBoundary();MeshRouting();
    std::cout<<"PASS original primitive line commands; "<<checks<<" assertions; "<<cases<<" branch-state cases; "<<meshes<<" source-edge meshes; CPU colors/width traces only; GPU boundary uncolored multiplier-one; not complete renderer\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
