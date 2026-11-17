// Original body compatibility probe only. No GL/context, documents, science
// inputs or original application behavior. Original upstream licence applies.
#include "linepathsubmit.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
namespace raw_tests {
unsigned assertions=0,cases=0;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++assertions;}
template<class T> struct Values {
 std::vector<T> data;
 size_t Count()const{return data.size();} bool IsEmpty()const{return data.empty();}
 const T& operator[](size_t i)const{return data.at(i);}
};
struct Event{size_t identity;uint32_t color;};
struct Sink{
 uint32_t color;float width;bool selecting,forcePlain;
 unsigned queries=0,selectionQueries=0,colorRequests=0,colorCommands=0;
 std::vector<float> widths;std::vector<Event> output;
 bool IsSelecting(){++selectionQueries;return selecting;}
 void GetLineWidth(float* value){++queries;*value=width;}
 void SetLineWidth(float value){width=value;widths.push_back(value);}
 void SetColor(const uint32_t& value){++colorRequests;if(!forcePlain){color=value;++colorCommands;}}
 void DrawVertex(const size_t& value){output.push_back({value,color});}
 void DrawVertex(const size_t& value,const uint32_t& c){SetColor(c);DrawVertex(value);}
};
void Branches(){
 for(bool loop:{false,true})for(size_t count:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(4),size_t(9)})
 for(size_t nc:std::set<size_t>{0,count/2,count,count+1})
 for(bool selecting:{false,true})for(bool force:{false,true})
 for(uint32_t inherited:{0xffffffffu,0xff123456u})for(float width:{1.f,3.f})for(double multiplier:{1.,.5,1.5,2.}){
  Values<size_t> vertices;Values<uint32_t> colors;
  for(size_t i=0;i<count;++i)vertices.data.push_back(i);
  for(size_t i=0;i<nc;++i)colors.data.push_back(0xff000001u+uint32_t(i));
  Sink sink{inherited,width,selecting,force};const double params[]={multiplier};
  if(loop)glObj::SubmitPrimitiveLineLoop(vertices,colors,params,sink);
  else glObj::SubmitPrimitiveLineStrip(vertices,colors,params,sink);
  ++cases;
  // Independent branch model: strip never queries selecting; its per-vertex
  // SetColor requests remain present while selecting. Loop intentionally differs.
  const bool colored=loop?nc>0 && !selecting && nc==count:nc==count;
  const size_t emitted=!loop || nc==0 || selecting || nc==count?count:0;
  const unsigned requests=colored?unsigned(count):0;
  Check(sink.output.size()==emitted,"original path vertex count / malformed-color semantics");
  Check(sink.colorRequests==requests && sink.colorCommands==(force?0:requests),"original path color requests / ForcePlain");
  Check(sink.selectionQueries==(loop && nc>0?1:0),"strip/loop selection short circuit must remain distinct");
  Check(sink.queries==(multiplier==1?0:1),"width query count");
  const std::vector<float> expected=multiplier==1?std::vector<float>{}:std::vector<float>{float(multiplier*width),width};
  Check(sink.widths==expected && sink.width==width,"original width multiplication and restoration");
  uint32_t finalColor=inherited;
  for(size_t i=0;i<emitted;++i){const uint32_t c=colored && !force?colors.data[i]:inherited;Check(sink.output[i].identity==i && sink.output[i].color==c,"original vertex identity/order and effective color");finalColor=c;}
  Check(sink.color==finalColor,"original final color state");
 }
}
}

// Isolated target boundary/topology prototype only. Not production/GPU evidence.
#include "primitive_line_paths.h"
#include <iostream>
using namespace olex2::harmony;
using Kind=OriginalLinePathKind;using State=LineCommandState;
constexpr size_t MaximumVertices=MaximumOriginalLinePathVertices;
namespace bounded_tests {
unsigned assertions=0,cases=0;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++assertions;}
template<class F>void Reject(F f,const char* message){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}Check(rejected,message);}
std::vector<std::array<float,3>> PositionsFor(size_t n){std::vector<std::array<float,3>> out;for(size_t i=0;i<n;++i)out.push_back({float(i/2),float(i%3),0});return out;}
void Valid(){
 for(Kind kind:{Kind::Strip,Kind::Loop})for(size_t n:{size_t(2),size_t(3),size_t(4),size_t(5),size_t(17),size_t(256)}){
  if(kind==Kind::Loop && n<3)continue;
  auto vertices=PositionsFor(n);vertices.back()=vertices.front();
  for(bool colored:{false,true})for(bool selecting:{false,true})for(bool force:{false,true})for(uint32_t color:{0xffffffffu,0xff123456u})for(float width:{1.f,3.f})for(double multiplier:{1.,.5,1.5,2.}){
   std::vector<uint32_t> colors;if(colored)for(size_t i=0;i<n;++i)colors.push_back(0xff000001u+uint32_t(i));State state{color,width,force,selecting};const auto batch=CaptureOriginalLinePath(vertices,colors,kind,multiplier,state);++cases;
   const bool applies=colored && !(kind==Kind::Loop && selecting);const unsigned requests=applies?unsigned(n):0;
   Check(batch.vertices.size()==n,"accepted original output count");Check(batch.indices.size()==2*(n-1+(kind==Kind::Loop?1:0)),"topology index count");
   Check(batch.selectionQueries==(kind==Kind::Loop && colored?1:0),"distinct original selection query semantics");Check(batch.colorRequests==requests && batch.colorCommands==(force?0:requests),"original color requests/ForcePlain commands");
   Check(batch.widthQueries==(multiplier==1?0:1) && batch.finalState.lineWidth==width,"width query/restoration");
   const std::vector<float> widths=multiplier==1?std::vector<float>{}:std::vector<float>{float(multiplier*width),width};Check(batch.widthCommands==widths,"width trace order");
   for(size_t i=0;i<n;++i){const uint32_t expected=applies && !force?colors[i]:color;Check(batch.vertices[i].inputVertex==i && batch.vertices[i].position==vertices[i] && batch.vertices[i].color==expected,"original output identity/coordinates/color");}
   for(size_t i=1;i<n;++i)Check(batch.indices[2*(i-1)]==i-1 && batch.indices[2*(i-1)+1]==i,"independent consecutive source edge topology");
   if(kind==Kind::Loop)Check(batch.indices[2*(n-1)]==n-1 && batch.indices.back()==0,"only loop closes last-to-first");
   Check(batch.finalState.color==(applies && !force?colors.back():color) && batch.finalState.selecting==selecting && batch.finalState.forcePlain==force,"explicit final path state");
   Check(batch.vertices.front().position==batch.vertices.back().position && batch.vertices.front().inputVertex!=batch.vertices.back().inputVertex,"coincident endpoints remain distinct source identities");
  }
 }
}
void Invalid(){
 const auto v=PositionsFor(4);const std::vector<uint32_t> none,badColors(3);
 for(Kind kind:{Kind::Strip,Kind::Loop}){
  Reject([&]{CaptureOriginalLinePath({},none,kind);},"empty target path rejected");Reject([&]{CaptureOriginalLinePath(PositionsFor(1),none,kind);},"single target point rejected");
  Reject([&]{CaptureOriginalLinePath(PositionsFor(MaximumVertices+1),none,kind);},"vertex budget enforced");Reject([&]{CaptureOriginalLinePath(v,std::vector<uint32_t>(MaximumVertices+1),kind);},"color budget enforced");
  Reject([&]{CaptureOriginalLinePath(v,badColors,kind);},"malformed colors rejected at explicit target boundary");
  for(double x:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max(),std::numeric_limits<double>::min()})Reject([&]{CaptureOriginalLinePath(v,none,kind,x);},"nonrepresentable width multiplier rejected");
  for(float x:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){State s;s.lineWidth=x;Reject([&]{CaptureOriginalLinePath(v,none,kind,1,s);},"invalid initial width rejected");}
  for(size_t axis=0;axis<3;++axis){auto nonfinite=v;nonfinite[0][axis]=std::numeric_limits<float>::quiet_NaN();Reject([&]{CaptureOriginalLinePath(nonfinite,none,kind);},"nonfinite source coordinate rejected");}
 }
 Reject([&]{CaptureOriginalLinePath(PositionsFor(2),none,Kind::Loop);},"loop needs three target vertices");Reject([&]{CaptureOriginalLinePath(v,none,static_cast<Kind>(99));},"unknown target topology rejected");
 State selected;selected.selecting=true;Check(CaptureOriginalLinePath(v,badColors,Kind::Loop,1,selected).vertices.size()==v.size(),"loop selection ignores bounded color mismatch as original");Reject([&]{CaptureOriginalLinePath(v,badColors,Kind::Strip,1,selected);},"strip target boundary does not import loop selection exception");
}
}

int main(){try{raw_tests::Branches();bounded_tests::Valid();bounded_tests::Invalid();std::cout<<"PASS original line strip/loop paths; "<<raw_tests::assertions+bounded_tests::assertions<<" assertions; "<<raw_tests::cases<<" raw branch-state cases; "<<bounded_tests::cases<<" bounded branch-state cases; 2 independent edge topologies; CPU traces are not GPU color/width/selection qualification\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
