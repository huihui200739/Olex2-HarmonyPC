// The original command action still calls the real application processor.
// Kept out of isolated ordinary UI harnesses; no fake processor is supplied.
#include "ctrls.h"
#include "integration.h"
bool olxCommandAction::Run(){
  TStrList toks=TParamList::StrtokLines(cmd,">>");
  olex2::IOlex2Processor* ip=olex2::IOlex2Processor::GetInstance();
  for(size_t i=0;i<toks.Count();i++)if(!ip->processMacro(toks[i]))return false;
  return true;
}
