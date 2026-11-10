#include "original_interpreter.h"
#include <stdexcept>
namespace olex2::harmony {
namespace {std::mutex& InterpreterMutex(){static std::mutex value;return value;}}
OriginalInterpreterGuard::OriginalInterpreterGuard():lock_(InterpreterMutex()),previous_(olex2::IOlex2Processor::GetInstance()){}
OriginalInterpreterGuard::~OriginalInterpreterGuard(){olex2::IOlex2Processor::GetInstance()=previous_;}
OriginalProcessor::Depth::Depth(unsigned& c):count(c){if(count>=8)throw std::length_error("Command evaluation depth exceeded");++count;}
OriginalProcessor::Depth::~Depth(){--count;}
OriginalProcessor::OriginalProcessor():macros_(*this){macros_.SetLogLevel(0);}
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
bool OriginalProcessor::registerCallbackFunc(const olxstr&,ABasicFunction*){throw std::logic_error("Application callback registry is not integrated");}
void OriginalProcessor::unregisterCallbackFunc(const olxstr&,const olxstr&){throw std::logic_error("Application callback registry is not integrated");}
void OriginalProcessor::callCallbackFunc(const olxstr&,const TStrList&){throw std::logic_error("Application callback registry is not integrated");}
}
