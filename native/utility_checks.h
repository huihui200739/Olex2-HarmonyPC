#pragma once
#include <string>
#include <vector>

namespace olex2::harmony {
struct CheckResult {
  std::string name;
  bool passed;
  std::string detail;
};
struct UtilityReport {
  std::vector<CheckResult> checks;
  bool passed = false;
  std::string error;
};
// Only creates exclusively owned temporary files below the supplied writable dir.
UtilityReport RunUtilityChecks(const std::string& writable_directory);
}
