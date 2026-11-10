#include "primitive_line_paths.h"
#include "linepathsubmit.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace olex2::harmony {
namespace {
struct Tagged {const std::array<float,3>& value;size_t inputVertex;};
struct Positions {const std::vector<std::array<float,3>>& data;size_t Count()const{return data.size();}Tagged operator[](size_t i)const{return {data.at(i),i};}};
struct Colors {const std::vector<uint32_t>& data;size_t Count()const{return data.size();}bool IsEmpty()const{return data.empty();}const uint32_t& operator[](size_t i)const{return data.at(i);}};
struct Sink {
 OriginalLinePathBatch& out;
 bool IsSelecting(){++out.selectionQueries;return out.finalState.selecting;}
 void GetLineWidth(float* value){++out.widthQueries;*value=out.finalState.lineWidth;}
 void SetLineWidth(float value){out.widthCommands.push_back(value);out.finalState.lineWidth=value;}
 void SetColor(const uint32_t& value){++out.colorRequests;if(!out.finalState.forcePlain){out.finalState.color=value;++out.colorCommands;}}
 void DrawVertex(const Tagged& p){out.vertices.push_back({p.value,out.finalState.color,p.inputVertex});}
 void DrawVertex(const Tagged& p,const uint32_t& c){SetColor(c);DrawVertex(p);}
};

}
OriginalLinePathBatch CaptureOriginalLinePath(const std::vector<std::array<float,3>>& vertices,const std::vector<uint32_t>& colors,OriginalLinePathKind kind,double multiplier,const LineCommandState& initial){
 if(kind!=OriginalLinePathKind::Strip && kind!=OriginalLinePathKind::Loop)throw std::invalid_argument("Unknown original path topology");
 if(vertices.size()<(kind==OriginalLinePathKind::Loop?3:2) || vertices.size()>MaximumOriginalLinePathVertices)throw std::invalid_argument("Original path vertex count outside target boundary");
 const bool ignored=kind==OriginalLinePathKind::Loop && initial.selecting;
 if(colors.size()>MaximumOriginalLinePathVertices || (!ignored && !colors.empty() && colors.size()!=vertices.size()))throw std::invalid_argument("Original path colors must be absent or per vertex");
 const double product=multiplier*initial.lineWidth;
 if(!std::isfinite(initial.lineWidth) || initial.lineWidth<=0 || !std::isfinite(multiplier) || multiplier<=0 || !std::isfinite(product) || product>std::numeric_limits<float>::max() || static_cast<float>(product)<=0)throw std::invalid_argument("Path width product is not finite positive representable float");
 for(const auto& p:vertices)for(float v:p)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite original path position");
 OriginalLinePathBatch out;out.finalState=initial;out.vertices.reserve(vertices.size());const Positions positions{vertices};const Colors c{colors};const double params[]={multiplier};Sink sink{out};
 if(kind==OriginalLinePathKind::Strip)glObj::SubmitPrimitiveLineStrip(positions,c,params,sink);else glObj::SubmitPrimitiveLineLoop(positions,c,params,sink);
 if(out.vertices.size()!=vertices.size() || out.finalState.lineWidth!=initial.lineWidth)throw std::runtime_error("Original path did not emit/restore its accepted complete stream");
 // Explicit topology conversion consumes the real original output identities.
 // Never infer identity from coordinate equality, nor add a strip closing edge.
 out.indices.reserve(2*(out.vertices.size()-1+(kind==OriginalLinePathKind::Loop?1:0)));
 for(size_t i=1;i<out.vertices.size();++i){out.indices.push_back(static_cast<uint32_t>(out.vertices[i-1].inputVertex));out.indices.push_back(static_cast<uint32_t>(out.vertices[i].inputVertex));}
 if(kind==OriginalLinePathKind::Loop){out.indices.push_back(static_cast<uint32_t>(out.vertices.back().inputVertex));out.indices.push_back(static_cast<uint32_t>(out.vertices.front().inputVertex));}
 return out;
}
}
