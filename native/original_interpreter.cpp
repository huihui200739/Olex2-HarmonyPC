#include "original_interpreter.h"
#include <stdexcept>
namespace olex2::harmony {
namespace {std::mutex& InterpreterMutex(){static std::mutex value;return value;}}
OriginalInterpreterGuard::OriginalInterpreterGuard():lock_(InterpreterMutex()),previous_(olex2::IOlex2Processor::GetInstance()){}
OriginalInterpreterGuard::~OriginalInterpreterGuard(){olex2::IOlex2Processor::GetInstance()=previous_;}
OriginalProcessor::Depth::Depth(unsigned& c):count(c){if(count>=8)throw std::length_error("Command evaluation depth exceeded");++count;}
OriginalProcessor::Depth::~Depth(){--count;}
OriginalProcessor::OriginalProcessor():macros_(*this){macros_.SetLogLevel(0);}
OriginalProcessor::~OriginalProcessor(){
  for(size_t i=0;i<callbackFuncs_.Count();++i)delete callbackFuncs_.GetValue(i);
}
TLibrary& OriginalProcessor::GetLibrary(){return library_;}
bool OriginalProcessor::processMacro(const olxstr& text,const olxstr& location,bool quiet){TMacroData error;return processMacroEx(text,error,location,quiet);}
bool OriginalProcessor::processMacroEx(const olxstr& text,TMacroData& error,const olxstr& location,bool quiet){
  Depth depth(depth_);error.SetQuiet(quiet);error.SetLocation(location);
  const TStrList commands=TParamList::StrtokLines(text,">>",false);
  for(size_t i=0;i<commands.Count() && error.IsSuccessful();++i)macros_.ProcessMacro(commands[i],error);
  return error.IsSuccessful();
}
bool OriginalProcessor::processMacroExt(const olxstr& text,TStrObjList& args,const TParamList& options,TMacroData& error,const olxstr& location,bool quiet){
  Depth depth(depth_);error.SetQuiet(quiet);error.SetLocation(location);macros_.ProcessMacro(text,args,options,error);return error.IsSuccessful();
}
bool OriginalProcessor::Evaluate(olxstr& text,TMacroData& error){Depth depth(depth_);return macros_.ProcessFunction(text,error,false) && error.IsSuccessful();}
bool OriginalProcessor::processFunction(olxstr& text,const olxstr& location,bool quiet){TMacroData error;error.SetQuiet(quiet);error.SetLocation(location);return Evaluate(text,error);}
bool OriginalProcessor::registerCallbackFunc(const olxstr& cbEvent,ABasicFunction* fn){
  if(!fn)throw std::invalid_argument("Callback function is required");
  callbackFuncs_.Add(cbEvent,fn);return true;
}
void OriginalProcessor::unregisterCallbackFunc(const olxstr& cbEvent,const olxstr& funcName){
  size_t ind=callbackFuncs_.IndexOf(cbEvent),i=ind;
  if(ind==InvalidIndex)return;
  while(i<callbackFuncs_.Count() && callbackFuncs_.GetKey(i).Equals(cbEvent)){
    if(callbackFuncs_.GetValue(i)->GetName()==funcName){delete callbackFuncs_.GetValue(i);callbackFuncs_.Delete(i);return;}
    ++i;
  }
  i=ind-1;
  while(i!=InvalidIndex && callbackFuncs_.GetKey(i).Equals(cbEvent)){
    if(callbackFuncs_.GetValue(i)->GetName()==funcName){delete callbackFuncs_.GetValue(i);callbackFuncs_.Delete(i);return;}
    --i;
  }
}
void OriginalProcessor::callCallbackFunc(const olxstr& cbEvent,const TStrList& params){
  TSizeList indexes;TMacroData error;
  callbackFuncs_.GetIndices(cbEvent,indexes);
  for(size_t i=0;i<indexes.Count();++i){
    callbackFuncs_.GetValue(indexes[i])->Run(params,error);
    if(!error.IsSuccessful()){
      error.Reset();throw std::runtime_error("Original application callback failed");
    }
    error.Reset();
  }
}
}
