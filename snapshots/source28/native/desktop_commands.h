#pragma once
#include <string>
namespace olex2::harmony {
// Platform-owned view state only. This does not register the complete legacy
// application, domain commands, Python, callbacks or external program tools.
struct DesktopCommandState {
  unsigned primitive=0,generation=3,colorPreset=0,materialPreset=0;
  bool wireframe=false;
  float yaw=32,pitch=-22,distance=4.4f;
};
bool SameDesktopCommandState(const DesktopCommandState&,const DesktopCommandState&);
struct DesktopCommandResult {
  bool passed=false,changed=false;
  unsigned dispatched=0;
  DesktopCommandState state;
  std::string text,error;
};
// Calls the real unmodified TEMacroLib expression parser and TLibrary/AMacro
// dispatch. New pc.* adapters are explicit; unsupported commands stay errors.
DesktopCommandResult ExecuteDesktopCommand(const std::string& utf8,const DesktopCommandState& initial={});
constexpr const char* DesktopCommandSource() {
  return "upstream/sdl/macrolib.cpp + library.cpp + function.cpp + exparse: original TEMacroLib parser and TLibrary macro dispatch -> explicit pc.* view adapters";
}
}
