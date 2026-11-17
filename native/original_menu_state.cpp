#include "original_menu_state.h"
#include "olxstate.h"
#include "integration.h"
#include <stdexcept>
namespace olex2::harmony::ui {
bool OriginalMenuStateActive(const olxstr& state,const olxstr& argument) {
  return TStateRegistry::GetInstance().CheckState(state,argument);
}
void OriginalTreeCommand(const olxstr& command) {
  auto* processor=olex2::IOlex2Processor::GetInstance();
  if(!processor)throw std::runtime_error("Original command processor is unavailable");
  processor->processMacro(command);
}
}
