#pragma once
#include "macrolib.h"
#include <memory>
#include <string>
namespace olex2::harmony {
struct PythonRuntimeStatus {
  bool prepared=false,initialized=false,originalBridgeImported=false;
  unsigned executions=0;
  std::string version,home,stdlibSha256,error;
};
struct PythonExecutionResult {
  bool passed=false,optionsChanged=false;
  std::string output,error;
  PythonRuntimeStatus runtime;
};
// One interpreter owned by the existing ApplicationRuntime, not another app.
// Only trusted scripts: isolated path configuration is not a security sandbox.
class PythonRuntime {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  PythonRuntime();
  ~PythonRuntime();
  void AttachApplicationLibrary(TLibrary* library);
  void Initialize(const std::string& home,const std::string& stdlibSha256);
  PythonRuntimeStatus Status() const;
  PythonExecutionResult Execute(const std::string& source);
  PythonRuntime(const PythonRuntime&)=delete;
  PythonRuntime& operator=(const PythonRuntime&)=delete;
};
}
