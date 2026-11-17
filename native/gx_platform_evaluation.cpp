#include <napi/native_api.h>
#include <hilog/log.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "synthetic_image_fixtures.h"
#include "descriptor_control.h"
extern "C" const char* olex2_gx_lifecycle_run(const char*);
extern "C" const char* olex2_gx_texture_decode_run(const char*,const char*);
std::string ImageDecodeProbe(const char*,const char*);
napi_value RunGxPlatformEvaluation(napi_env env,napi_callback_info info) {
  std::string result;
  try {
    size_t argc=1; napi_value argument; napi_get_cb_info(env,info,&argc,&argument,nullptr,nullptr);
    size_t length=0;
    if(argc!=1 || napi_get_value_string_utf8(env,argument,nullptr,0,&length)!=napi_ok || !length || length>4096)throw std::runtime_error("Expected app files directory");
    std::vector<char> text(length+1);napi_get_value_string_utf8(env,argument,text.data(),text.size(),&length);
    std::string root(text.data(),length);
    if(root.empty()||root[0]!='/'||root.find('\0')!=std::string::npos)throw std::runtime_error("Invalid app files directory");
    root+="/gx-platform-evaluation-pid-"+std::to_string(getpid());
    if(mkdir(root.c_str(),0755)!=0)throw std::runtime_error("Exclusive synthetic artifact directory already exists or unavailable");
    const std::string control=ObserveNoDecoderDescriptorControl();
    std::ostringstream out;out<<"{\"scope\":\"source-built evaluation only, no production replacement\",\"pid\":"<<getpid()<<",\"no_decoder_control\":"<<control<<",\"lifecycle\":"<<olex2_gx_lifecycle_run((root+"/olex2-harmony").c_str())<<",\"images\":[";
    bool first=true;
    for(const auto& image:synthetic_images) {
      const std::string path=root+"/"+image.name;
      {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(image.bytes),image.size);file.close();if(!file)throw std::runtime_error("Cannot write own synthetic fixture");}
      for(const char* mode:{"rgb","rgba"}) {
        if(!first)out<<',';first=false;
        out<<"{\"fixture\":\""<<image.name<<"\",\"requested\":\""<<mode<<"\",\"observation\":"<<ImageDecodeProbe(path.c_str(),mode)<<'}';
      }
    }
    {const auto& image=synthetic_images[0];std::ofstream file(root+"/合成颜色-2.png",std::ios::binary);file.write(reinterpret_cast<const char*>(image.bytes),image.size);file.close();if(!file)throw std::runtime_error("Cannot retain synthetic Unicode image");}
    const std::string afterDecode=ObserveNoDecoderDescriptorControl("after-capability-probes");
    out<<"],\"post_decode_control\":"<<afterDecode<<",\"texture_hook\":"<<olex2_gx_texture_decode_run((root+"/olex2-gx-textures").c_str(),root.c_str())<<",\"execution_completed\":true}";result=out.str();
    {std::ofstream file(root+"/result.json");file<<result<<'\n';file.close();if(!file)throw std::runtime_error("Cannot retain own observation");}
  } catch(const std::exception&) {result="{\"execution_completed\":false,\"error\":\"Platform evaluation setup failed\"}";}
  const size_t chunk=1200,total=(result.size()+chunk-1)/chunk;
  for(size_t i=0;i<total;++i)OH_LOG_Print(LOG_APP,LOG_INFO,0x3801,"Olex2GX","GX_EVALUATION_CHUNK %{public}u/%{public}u %{public}s",unsigned(i+1),unsigned(total),result.substr(i*chunk,chunk).c_str());
  napi_value output;napi_create_string_utf8(env,result.data(),result.size(),&output);return output;
}
