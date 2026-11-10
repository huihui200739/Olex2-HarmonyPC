#pragma once
#include "macrolib.h"
#include <mutex>
namespace olex2::harmony {
// All platform consumers of the legacy current-processor pointer share this
// guard. This does not serialize unrelated upstream consumers.
class OriginalInterpreterGuard {
  std::unique_lock<std::mutex> lock_;
  olex2::IOlex2Processor* previous_;
public:
  OriginalInterpreterGuard();
  ~OriginalInterpreterGuard();
  OriginalInterpreterGuard(const OriginalInterpreterGuard&)=delete;
  OriginalInterpreterGuard& operator=(const OriginalInterpreterGuard&)=delete;
};
// Common adapter for the real interpreter, not a replacement command parser.
// Owners must hold OriginalInterpreterGuard before constructing this object.
class OriginalProcessor:public IOlxObject,public olex2::IOlex2Processor {
protected:
  TLibrary library_{""};
private:
  macrolib::TEMacroLib macros_;
  unsigned depth_=0;
  struct Depth {
    unsigned& count;
    explicit Depth(unsigned& c);
    ~Depth();
  };
public:
  OriginalProcessor();
  TLibrary& GetLibrary() override;
  bool processMacro(const olxstr&,const olxstr& location=EmptyString(),bool quiet=false) override;
  bool processMacroEx(const olxstr&,TMacroData&,const olxstr& location=EmptyString(),bool quiet=false) override;
  bool processMacroExt(const olxstr&,TStrObjList&,const TParamList&,TMacroData&,const olxstr& location=EmptyString(),bool quiet=false) override;
  bool processFunction(olxstr&,const olxstr& location=EmptyString(),bool quiet=false) override;
  bool Evaluate(olxstr&,TMacroData&);
  bool registerCallbackFunc(const olxstr&,ABasicFunction*) override;
  void unregisterCallbackFunc(const olxstr&,const olxstr&) override;
  void callCallbackFunc(const olxstr&,const TStrList&) override;
};
}
