#include "original_renderer_host.h"
#include "application_runtime.h"
#include "exception.h"
#include <sstream>
#include <stdexcept>
namespace {
std::string result;
std::string Quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<c;else o<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}o<<'"';return o.str();}
}
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost* host){
  if(host && host->completed)*host->completed=0;
  try{
    if(!host || !host->existing_runtime)throw std::invalid_argument("Missing existing original GX runtime");
    result=static_cast<olex2::harmony::ApplicationRuntime*>(host->existing_runtime)->RunOriginalWindow(*host);
    if(host->completed)*host->completed=1;
  }catch(const TExceptionBase& e){result="{\"execution_completed\":false,\"error\":"+Quote(e.GetException()->GetFullMessage().ToMBStr().c_str())+"}";
  }catch(const std::exception& e){result="{\"execution_completed\":false,\"error\":"+Quote(e.what())+"}";
  }catch(...){result="{\"execution_completed\":false,\"error\":\"Unknown persistent original renderer exception\"}";}
  return result.c_str();
}
