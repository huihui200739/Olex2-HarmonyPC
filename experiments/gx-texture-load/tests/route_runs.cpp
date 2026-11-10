#include "original_window_route_runs.h"
#include <iostream>
#include <stdexcept>
int main() {
  unsigned assertions = 0;
  const auto require = [&assertions](bool value) {
    ++assertions;
    if (!value) throw std::runtime_error("Route-local generation sequence differs");
  };
  olex2::harmony::OriginalWindowRouteRuns runs;
  require(runs.Next(true) == 1);
  require(runs.Next(false) == 1);
  require(runs.Next(true) == 2);
  require(runs.Next(false) == 2);
  require(runs.Next(false) == 3);
  require(runs.Next(true) == 3);
  for (unsigned i = 4; i <= 30; ++i) {
    require(runs.Next(true) == i);
    require(runs.Next(false) == i);
  }
  olex2::harmony::OriginalWindowRouteRuns nextProcess;
  require(nextProcess.Next(false) == 1);
  require(nextProcess.Next(true) == 1);
  std::cout << "PASS isolated route-local counters; " << assertions
            << " assertions; no GL or device execution\n";
}
