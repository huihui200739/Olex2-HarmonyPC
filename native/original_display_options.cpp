#include "original_display_options.h"
#include "application_runtime.h"
#include "dgrad.h"
#include "primtvs.h"
#include "glbackground.h"
#include "styles.h"
#include "xatom.h"
#include "xbond.h"
#include <stdexcept>
using namespace olex2::harmony::ui;
namespace olex2::harmony::ui {
namespace {
void Ready(const glObj::TGlBackground& background){if(!background.HasPrimitives())throw std::runtime_error("Original background is not created yet");}
glObj::TGlOption ReadCorner(const glObj::TGlBackground& b,size_t i){switch(i){case 0:return b.LT();case 1:return b.RT();case 2:return b.RB();default:return b.LB();}}
void WriteCorner(glObj::TGlBackground& b,size_t i,const glObj::TGlOption& value){switch(i){case 0:b.LT(value);break;case 1:b.RT(value);break;case 2:b.RB(value);break;default:b.LB(value);break;}}
}
GradientColors ReadOriginalGradientBackground(const glObj::TGlBackground& background){
  Ready(background);GradientColors result;
  for(size_t i=0;i<result.size();++i)result[i]=FromOriginalRgba(ReadCorner(background,i).GetRGB());
  return result;
}
void ApplyOriginalGradientBackground(glObj::TGlBackground& background,const GradientColors& colors){
  Ready(background);std::array<glObj::TGlOption,4> next;
  for(size_t i=0;i<next.size();++i)next[i]=ToOriginalRgba(colors[i]);
  ApplyGradientWithRollback(next,[&](size_t i){return ReadCorner(background,i);},[&](size_t i,const glObj::TGlOption& v){WriteCorner(background,i,v);});
}
PrimitiveDisplaySnapshot ReadOriginalPrimitiveDisplay(glObj::AGDrawObject& object){
  if(!object.HasPrimitives())throw std::runtime_error("Original display object has no primitive collection");
  PrimitiveDisplaySnapshot result;TStrList labels;object.ListPrimitives(labels);
  if(labels.Count()>32)throw std::length_error("The original primitive mask supports at most 32 entries");
  for(size_t i=0;i<labels.Count();++i)result.labels.push_back(labels[i]);
  result.mask=static_cast<uint32_t>(object.GetPrimitives().GetStyle().GetParam(object.GetPrimitiveMaskName(),"0").ToInt());
  result.hasApplyScope=object.Is<gxlib::TXAtom>()||object.Is<gxlib::TXBond>();
  if(result.hasApplyScope)result.currentLevel=gxlib::TXAtom::LegendLevel(object.GetPrimitives().GetName());
  return result;
}
}
TdlgGradient::TdlgGradient(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,olex2::harmony::ApplicationRuntime& runtime,std::shared_ptr<DialogPositionStore> positions)
 :TdlgGradient(nodes,std::move(dialogs),std::move(scheduler),parent,runtime.GradientBackgroundStore(),std::move(positions)){}
TdlgPrimitive::TdlgPrimitive(NodeBackend& nodes,std::shared_ptr<DialogBackend> dialogs,std::shared_ptr<UiScheduler> scheduler,Widget* parent,glObj::AGDrawObject& object,std::shared_ptr<DialogPositionStore> positions)
 :TdlgPrimitive(nodes,std::move(dialogs),std::move(scheduler),parent,ReadOriginalPrimitiveDisplay(object),std::move(positions)){}
