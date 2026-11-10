#include "original_window_route_runs_font.h"
#include <iostream>
#include <stdexcept>
using olex2::harmony::OriginalWindowRouteRuns;
int main() {
  unsigned checks=0;
  auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("Route counter separation failed");};
  for(unsigned first=0;first<3;++first) {
    OriginalWindowRouteRuns runs;unsigned counters[3]={};
    for(unsigned i=0;i<300;++i) {
      const auto route=(i+first)%3;const auto actual=route==2?runs.NextFont():runs.Next(route==1);
      check(actual==++counters[route]);
    }
    check(counters[0]==100 && counters[1]==100 && counters[2]==100);
  }
  OriginalWindowRouteRuns a,b;check(a.NextFont()==1);check(a.Next(false)==1);check(a.Next(true)==1);
  check(a.NextFont()==2);check(b.NextFont()==1);check(b.Next(false)==1);check(b.Next(true)==1);
  std::cout<<"PASS "<<checks<<" isolated three-route counter assertions; no GL or device execution\n";
}
