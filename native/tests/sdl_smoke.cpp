#include "utility_checks.h"
#include <iostream>
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: olex2_sdl_smoke <writable-test-directory>\n";
    return 2;
  }
  const auto report = olex2::harmony::RunUtilityChecks(argv[1]);
  for (const auto& check : report.checks) {
    std::cout << (check.passed ? "PASS " : "FAIL ") << check.name << '\n';
  }
  if (!report.passed) { std::cerr << "FAIL " << report.error << '\n'; return 1; }
  std::cout << "PASS utility-layer baseline only; GUI is not tested\n";
  return 0;
}
