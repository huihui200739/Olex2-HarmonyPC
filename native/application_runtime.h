#pragma once
#include "original_log.h"
#include "python_runtime.h"
#include <memory>
#include <string>
#include <vector>
struct Olex2LegacyHost;
namespace olex2::harmony {
namespace ui {class GradientStore;}
struct ApplicationRuntimeStatus {
  bool initialized=false,optionsRestored=false,licenseVerified=false,commandHints=true;
  bool lastSaveCommitted=false,originalCoreInitialized=false,originalCoreEmptyFile=false;
  unsigned originalFunctionCount=0,optionsCount=0,saveRevision=0;
  unsigned originalCoreMacroCount=0,originalCoreFunctionCount=0,originalCoreFileCount=0,originalFileFormatCount=0;
  std::string baseDir,instanceDir,sharedDir,configDir,licenseSha256,optionsSha256,platform,error;
};
struct ApplicationFunctionResult {
  bool passed=false,optionsChanged=false;
  std::string input,text,error;
  ApplicationRuntimeStatus runtime;
};
struct ApplicationCommandResult {
  bool passed=false;
  std::string input,text,output,error;
  ApplicationRuntimeStatus runtime;
};
struct ApplicationOptionsTransferResult {
  bool passed=false,optionsChanged=false;
  unsigned importedOptions=0;
  std::string bytes,sha256,error;
  ApplicationRuntimeStatus runtime;
};
struct CoreCatalogResult {
  bool passed=false;
  unsigned exposedMacros=0;
  uint64_t postedBefore=0,postedAfter=0;
  std::string input,error;
};
constexpr size_t MaximumOptionsTransferBytes=1024*1024;
// Unique persistent TGXApp owner, original renderer and platform scene.
// Only LstMac/LstFun metadata macros are exposed via a separate core.* catalog interpreter.
// Font, original UI and complete Olex2 entry point remain unfinished.
// The ability supplies its own filesDir and the packaged original licence.
struct OriginalBadReflectionsResult {
  bool passed = false;
  unsigned count = 0;
  int hklf = 0;
  std::string json, error;
};
struct ApplicationSelectionLabelResult {
  bool passed=false;size_t objectCount=0;std::string text,error;
};
struct ApplicationBondRadiusResult {
  bool passed=false;double radius=0;std::string error;
};
class ApplicationRuntime {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  ApplicationRuntime(const std::string& filesDir,const std::string& licenseBytes);
  ~ApplicationRuntime();
  // Background-only access: revoked before the unique renderer is destroyed.
  std::shared_ptr<ui::GradientStore> GradientBackgroundStore();
  ApplicationRuntime(const ApplicationRuntime&)=delete;
  ApplicationRuntime& operator=(const ApplicationRuntime&)=delete;
  ApplicationRuntimeStatus Status() const;
  PythonRuntimeStatus InitializePython(const std::string& stdlibBytes);
  PythonRuntimeStatus GetPythonStatus() const;
  PythonExecutionResult RunPython(const std::string& source);
  std::string RunOriginalWindow(const Olex2LegacyHost& host);
  // Fresh, verified staged input bytes; no saver, graphics or state changes.
  std::string ReadOriginalDocumentSource();
  ApplicationFunctionResult Evaluate(const std::string& expression);
  // Executes an original application command through the real macro library:
  // the application's registered macros/functions are replicated (never moved)
  // into a bounded interpreter instance, and the Harmony main-form library is
  // attached so ported main-frame actions are callable by command name.
  ApplicationCommandResult ExecuteCommand(const std::string& command);
  // Original bond-radius style change (TXBond::Settings), outside the command
  // interpreter so a pure display-style update cannot disturb the document.
  ApplicationBondRadiusResult SetBondRadius(double radius);
  // Original TMainForm ID_SelLabel: create a text label at the selection centroid.
  OriginalBadReflectionsResult ReadOriginalBadReflections();
  ApplicationSelectionLabelResult CreateSelectionLabel(const std::string& text);
  // Only real LstMac/LstFun clones are attached under core.*, never the full
  // scientific registry. They print metadata through the original TLog bus.
  CoreCatalogResult RunCoreCatalog(const std::string& command);
  OriginalLogSnapshot OriginalLogs() const;
  OriginalLogSnapshot ClearOriginalLogs();
  // Export is a read-only snapshot including the effective platform default.
  // Import merges validated keys using the original parser and save hook.
  ApplicationOptionsTransferResult ExportOptions() const;
  ApplicationOptionsTransferResult ImportOptions(const std::string& bytes);
};
constexpr const char* ApplicationRuntimeSource(){return "upstream/gxlib/gxapp.cpp: original TGXApp empty startup with one long-lived renderer and platform scene (core metadata catalogs separately exposed); upstream/sdl/bapp.cpp: original TBasicApp directories and ExportLibrary functions + settingsfile.cpp: original settings format; platform-owned private directories and atomic SaveOptions override";}
constexpr const char* CoreCatalogSource(){return "upstream/xlib/xmacro.cpp: genuine LstMac/LstFun replicated under core.* and dispatched by original TEMacroLib; metadata only, full core registry unavailable";}
constexpr const char* OriginalLicenseSha256(){return "ffaf87b82fdb58c4afe80e329b2d67c15d9bd792cf6097f751f67aade19fa864";}
}
