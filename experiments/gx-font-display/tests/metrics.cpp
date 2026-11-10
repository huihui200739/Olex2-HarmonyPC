#include "display_font_metrics.h"
#include <iostream>
#include <limits>
#include <string>
using namespace olex2::harmony;
unsigned assertions=0;
void Check(bool ok,const char* message){++assertions;if(!ok)throw std::runtime_error(message);}
template<class F>void Reject(F function){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}Check(rejected,"Invalid synthetic observation accepted");}
int main(){
  FontDisplayMetricsStore store;Check(!store.Get().ready,"Unobserved DPI must be unavailable");Reject([&]{RasterForDisplay(18,store.Get());});
  auto first=store.Set(0,96,96,1);Check(first.ready&&first.revision==1,"First observation revision");
  auto raster=RasterForDisplay(18,first);Check(raster.pixelSize==24&&raster.scaleX==1,"Observed96 conversion");
  Check(store.Set(0,96,96,1).revision==first.revision,"Unchanged observation does not churn cache");
  auto other=store.Set(1,144,192,2);raster=RasterForDisplay(18,other);Check(raster.pixelSize==48&&raster.scaleX==0.75f,"Separate actual x/y DPI conversion");
  Check(other.revision==2&&!SameFontDisplayMetrics(first,other),"Monitor transition invalidates identity");
  auto density=store.Set(1,144,192,3);Check(RasterForDisplay(18,density).pixelSize==48,"VP density must not multiply physical font DPI");
  const auto revision=store.Get().revision;
  for(double invalid:{-1.0,0.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),8193.0}){
    Reject([&]{store.Set(1,invalid,96,1);});Reject([&]{store.Set(1,96,invalid,1);});Check(store.Get().revision==revision,"Invalid observation must not modify valid state");
  }
  for(double invalid:{-1.0,0.5,9007199254740992.0,std::numeric_limits<double>::quiet_NaN()})Reject([&]{store.Set(invalid,96,96,1);});
  for(double invalid:{-1.0,0.0,65.0,std::numeric_limits<double>::infinity()})Reject([&]{store.Set(1,96,96,invalid);});
  Reject([&]{store.Set(1,1,8192,1);});Reject([&]{store.Set(1,8192,1,1);});
  auto cleared=store.Clear();Check(!cleared.ready&&cleared.revision==revision+1,"Cleared metrics must invalidate stale DPI");Reject([&]{RasterForDisplay(18,cleared);});
  Check(store.Clear().revision==cleared.revision,"Repeated clear is stable");Check(store.Set(1,144,192,2).revision==cleared.revision+1,"New observation after failure");
  for(unsigned dpi=72;dpi<=384;dpi+=3)for(unsigned points=3;points<=72;++points){
    const auto value=store.Set(dpi,static_cast<double>(dpi),static_cast<double>(dpi),static_cast<double>(dpi)/160);
    const auto measured=RasterForDisplay(points,value);Check(measured.pixelSize==static_cast<float>(points*static_cast<double>(dpi)/72.0),"Exact point/DPI model");Check(measured.scaleX==1,"Square pixels");
  }
  Reject([&]{RasterForDisplay(0,store.Get());});Reject([&]{RasterForDisplay(257,store.Get());});
  auto huge=store.Set(0,8192,8192,1);Reject([&]{RasterForDisplay(128,huge);});
  std::cout<<"PASS "<<assertions<<" isolated synthetic display-metric assertions; no observed display or device execution\n";
}
