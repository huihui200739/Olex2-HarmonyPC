#include "application_runtime.h"
#include "original_interpreter.h"
#include "xapp.h"
#include "eutf8.h"
#include "settingsfile.h"
#include "sha.h"
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
namespace olex2::harmony {
namespace {
std::string Utf8(const olxstr& s){return TUtf8::Encode(s).c_str();}
olxstr Text(const std::string& s){return TUtf8::Decode(s.c_str());}
std::string Read(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot read application runtime file");return {std::istreambuf_iterator<char>(f),{}};}
std::string Digest(const std::string& bytes){const std::string grouped=SHA256::Digest(bytes.data(),bytes.size()).c_str();std::string canonical;for(unsigned char c:grouped)if(c!=' ')canonical+=static_cast<char>(std::tolower(c));return canonical;}
void Directory(const std::string& path){struct stat st{};if(mkdir(path.c_str(),0700)!=0 && errno!=EEXIST)throw std::runtime_error("Cannot create private application directory");if(lstat(path.c_str(),&st)!=0 || !S_ISDIR(st.st_mode))throw std::runtime_error("Application runtime path is not an ordinary directory");}
// Temporary file ownership is exclusive. Never delete pre-existing files or
// unrelated directory contents; rename replaces only the selected app file.
class AtomicFile {
  std::string destination_,temporary_;
public:
  explicit AtomicFile(const std::string& destination):destination_(destination){
    std::string pattern=destination+".tmp-XXXXXX";std::vector<char> b(pattern.begin(),pattern.end());b.push_back(0);int fd=mkstemp(b.data());if(fd<0)throw std::runtime_error("Cannot create private temporary settings file");temporary_=b.data();close(fd);
  }
  ~AtomicFile(){if(!temporary_.empty())unlink(temporary_.c_str());}
  const std::string& Path()const{return temporary_;}
  void Commit(){int fd=open(temporary_.c_str(),O_RDONLY);if(fd<0)throw std::runtime_error("Cannot open staged runtime file");const int flushed=fsync(fd);close(fd);if(flushed!=0)throw std::runtime_error("Cannot flush staged runtime file");if(rename(temporary_.c_str(),destination_.c_str())!=0)throw std::runtime_error("Cannot commit application runtime file");temporary_.clear();}
};
void ValidateOption(const olxstr& name,const olxstr& value){
  const auto key=Utf8(name),v=Utf8(value);
  if(key.empty() || key.size()>128 || key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos || v.size()>2048 || v.find_first_of("\r\n")!=std::string::npos || v.find('\0')!=std::string::npos)throw std::invalid_argument("Private option name/value is outside the supported text format");
  if(name.Equalsi("ui.commandHints") && v!="true" && v!="false")throw std::invalid_argument("ui.commandHints must be true or false");
}
std::string OptionsKey(const TParamList& o){std::string s;for(size_t i=0;i<o.Count();++i){s+=Utf8(o.GetName(i));s+='\0';s+=Utf8(o.GetValue(i));s+='\0';}return s;}
class PlatformApp final:public TXApp {
  std::string config_;mutable TParamList persisted_;
public:
  mutable bool lastSaveCommitted=false;mutable unsigned saveRevision=0;
  bool optionsRestored=false;
  PlatformApp(const std::string& base,const std::string& shared,const std::string& instance,const std::string& config):TXApp(Text(base+"/olex2-harmony")),config_(config){
    SetSharedDir(Text(shared));SetInstanceDir(Text(instance));SetConfigDir(Text(config));
    // Original ReadOptions also edits PATH. Use the original format reader
    // explicitly instead; never execute desktop environment/launcher behavior.
    const std::string path=config_+"/.options";
    if(access(path.c_str(),F_OK)==0){TSettingsFile settings;settings.LoadUtf8Settings(Text(path));for(size_t i=0;i<settings.ParamCount();++i){ValidateOption(settings.ParamName(i),settings.ParamValue(i));UpdateOption(settings.ParamName(i),settings.ParamValue(i));}optionsRestored=true;}
    persisted_=GetOptions();
    const char* argv[]={"olex2-harmony"};InitArguments(1,argv);
  }
  olxstr GetPlatformString_(bool)const override{
#ifdef __OHOS__
    return sizeof(void*)==8?"OHOS64":"OHOS32";
#else
    return "HOST-COMPATIBILITY-TEST";
#endif
  }
  void MergeOptions(const TSettingsFile& imported) {
    if(imported.ParamCount()==0)throw std::invalid_argument("Selected settings file has no applicable options");
    TParamList candidate=GetOptions();
    for(size_t i=0;i<imported.ParamCount();++i){
      ValidateOption(imported.ParamName(i),imported.ParamValue(i));
      candidate.AddParam(imported.ParamName(i),imported.ParamValue(i));
    }
    if(candidate.Count()>256)throw std::length_error("Merged option count exceeds 256");
    const TParamList before=GetOptions();
    try{Options=candidate;SaveOptions();}
    catch(...){Options=before;throw;}
  }
  void SaveOptions() const override{
    lastSaveCommitted=false;
    try{
      if(GetOptions().Count()>256)throw std::length_error("Private option count exceeds 256");
      const std::string path=config_+"/.options";TSettingsFile settings;if(access(path.c_str(),F_OK)==0)settings.LoadUtf8Settings(Text(path));
      for(size_t i=0;i<GetOptions().Count();++i){ValidateOption(GetOptions().GetName(i),GetOptions().GetValue(i));settings.SetParam(GetOptions().GetName(i),GetOptions().GetValue(i));}
      AtomicFile file(path);settings.SaveSettings(Text(file.Path()));
      // Read the real staged bytes back with the original parser before commit.
      TSettingsFile verified;verified.LoadUtf8Settings(Text(file.Path()));if(verified.ParamCount()!=GetOptions().Count())throw std::runtime_error("Staged original options count mismatch");
      for(size_t i=0;i<GetOptions().Count();++i)if(verified.GetParam(GetOptions().GetName(i))!=GetOptions().GetValue(i))throw std::runtime_error("Staged original options value mismatch");
      file.Commit();lastSaveCommitted=true;++saveRevision;persisted_=GetOptions();
    }catch(...){const_cast<PlatformApp*>(this)->Options=persisted_;throw;}
  }
};
}
struct ApplicationRuntime::Impl {
  mutable std::mutex mutex;std::unique_ptr<PlatformApp> app;std::unique_ptr<OriginalLogCapture> log;std::string base,shared,instance,config;unsigned functionCount=0;
  ApplicationRuntimeStatus Status()const{
    ApplicationRuntimeStatus s;s.initialized=bool(app);if(!app)return s;
    s.originalCoreInitialized=TBasicApp::HasInstance() && &TXApp::GetInstance()==app.get();s.originalCoreMacroCount=app->GetLibrary().MacroCount();s.originalCoreFunctionCount=app->GetLibrary().FunctionCount();s.originalCoreFileCount=app->XFiles().Count();s.originalCoreEmptyFile=s.originalCoreFileCount==1 && !app->XFile().HasLastLoader();
    s.baseDir=Utf8(TBasicApp::GetBaseDir());s.sharedDir=Utf8(TBasicApp::GetSharedDir());s.instanceDir=Utf8(TBasicApp::GetInstanceDir());s.configDir=Utf8(app->GetConfigDir());
    s.optionsRestored=app->optionsRestored;s.licenseSha256=Digest(Read(base+"/OLEX2-LICENCE.txt"));s.licenseVerified=s.licenseSha256==OriginalLicenseSha256();s.originalFunctionCount=functionCount;s.optionsCount=app->GetOptions().Count();s.commandHints=app->GetOptions().FindValue("ui.commandHints","true")!="false";
    s.lastSaveCommitted=app->lastSaveCommitted;s.saveRevision=app->saveRevision;s.platform=Utf8(TBasicApp::GetPlatformString(false));if(access((config+"/.options").c_str(),F_OK)==0)s.optionsSha256=Digest(Read(config+"/.options"));return s;
  }
};
ApplicationRuntime::ApplicationRuntime(const std::string& root,const std::string& license):impl_(new Impl){
  OriginalInterpreterGuard guard;
  if(TBasicApp::HasInstance())throw std::runtime_error("An original application runtime already exists");
  if(root.empty() || root.front()!='/' || root.find('\0')!=std::string::npos || license.empty() || license.size()>65536 || Digest(license)!=OriginalLicenseSha256())throw std::invalid_argument("A private absolute files directory and exact packaged original licence are required");
  char resolved[4096];if(!realpath(root.c_str(),resolved))throw std::runtime_error("Application files directory does not exist");
  const std::string runtime=std::string(resolved)+"/olex2-runtime-v1";Directory(runtime);
  impl_->base=runtime+"/base";impl_->shared=runtime+"/shared";impl_->instance=runtime+"/instance";impl_->config=runtime+"/config";
  for(const auto& d:{impl_->base,impl_->shared,impl_->instance,impl_->config})Directory(d);
  const std::string path=impl_->base+"/OLEX2-LICENCE.txt";
  if(access(path.c_str(),F_OK)!=0 || Read(path)!=license){AtomicFile f(path);{std::ofstream out(f.Path(),std::ios::binary|std::ios::trunc);out.write(license.data(),license.size());out.flush();if(!out)throw std::runtime_error("Cannot stage original licence");}f.Commit();}
  impl_->app.reset(new PlatformApp(impl_->base,impl_->shared,impl_->instance,impl_->config));
  impl_->log.reset(new OriginalLogCapture(impl_->app->GetLog()));
  std::unique_ptr<TLibrary> library(impl_->app->ExportLibrary("app"));impl_->functionCount=library->FunctionCount();
}
ApplicationRuntime::~ApplicationRuntime()=default;
ApplicationRuntimeStatus ApplicationRuntime::Status()const{std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->Status();}
ApplicationFunctionResult ApplicationRuntime::Evaluate(const std::string& input){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;ApplicationFunctionResult r;r.input=input;const auto before=OptionsKey(impl_->app->GetOptions());
  try{
    if(input.empty() || input.size()>4096 || input.find('\0')!=std::string::npos || input.find('(')==std::string::npos)throw std::invalid_argument("Expected an original application function expression, at most 4096 UTF-8 bytes");
    OriginalProcessor processor;processor.GetLibrary().AttachLibrary(impl_->app->ExportLibrary("app"));r.runtime.originalFunctionCount=processor.GetLibrary().GetLibraryByName("app")->FunctionCount();
    olxstr text=Text(input);TMacroData error;error.SetQuiet(true);error.SetLocation("HarmonyPC original application function");r.passed=processor.Evaluate(text,error);if(r.passed)r.text=Utf8(text);else r.error=Utf8(error.GetInfo());
  }catch(const TExceptionBase& e){r.error=Utf8(e.GetException()->GetFullMessage());}catch(const std::exception& e){r.error=e.what();}
  if(!r.passed && r.error.empty())r.error="Original application function failed";
  r.optionsChanged=before!=OptionsKey(impl_->app->GetOptions());r.runtime=impl_->Status();return r;
}
CoreCatalogResult ApplicationRuntime::RunCoreCatalog(const std::string& input){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  CoreCatalogResult result;result.input=input;result.postedBefore=impl_->log->Snapshot().posted;
  try{
    if(input.empty() || input.size()>2048 || input.find('\0')!=std::string::npos || Utf8(Text(input))!=input)throw std::invalid_argument("Expected canonical UTF-8 core catalog command, at most 2048 bytes");
    OriginalProcessor processor;TLibrary* core=processor.GetLibrary().AddLibrary("core");
    for(const char* name:{"LstMac","LstFun"}){
      auto* original=impl_->app->GetLibrary().FindMacro(name);
      if(!original)throw std::runtime_error("Original catalog macro is not registered");
      std::unique_ptr<ABasicFunction> copy(original->Replicate());
      auto* clone=dynamic_cast<AMacro*>(copy.get());
      if(!clone)throw std::runtime_error("Original catalog macro could not be replicated");
      core->Register(clone);copy.release();
    }
    result.exposedMacros=core->MacroCount();TMacroData error;
    result.passed=processor.processMacroEx(Text(input),error,"HarmonyPC original core catalog",true);
    if(!result.passed)result.error=Utf8(error.GetInfo());
  }catch(const TExceptionBase& e){result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){result.error=e.what();}
  if(!result.passed && result.error.empty())result.error="Original catalog command was not accepted";
  // Original notifications already emitted by a failed chain are not undone.
  result.postedAfter=impl_->log->Snapshot().posted;return result;
}
OriginalLogSnapshot ApplicationRuntime::OriginalLogs() const {std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;return impl_->log->Snapshot();}
OriginalLogSnapshot ApplicationRuntime::ClearOriginalLogs(){std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;impl_->log->Clear();return impl_->log->Snapshot();}
ApplicationOptionsTransferResult ApplicationRuntime::ExportOptions() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  OriginalInterpreterGuard guard;
  ApplicationOptionsTransferResult result;
  try {
    TSettingsFile settings;
    const std::string path=impl_->config+"/.options";
    if(access(path.c_str(),F_OK)==0)settings.LoadUtf8Settings(Text(path));
    const auto& options=impl_->app->GetOptions();
    for(size_t i=0;i<options.Count();++i)settings.SetParam(options.GetName(i),options.GetValue(i));
    // The platform consumes this default even before the first explicit save.
    if(!settings.HasParam("ui.commandHints"))settings.SetParam("ui.commandHints","true");
    AtomicFile snapshot(impl_->config+"/.options-export");
    settings.SaveSettings(Text(snapshot.Path()));
    result.bytes=Read(snapshot.Path());
    if(result.bytes.empty() || result.bytes.size()>MaximumOptionsTransferBytes)throw std::length_error("Settings snapshot exceeds the 1 MiB document-transfer limit");
    result.sha256=Digest(result.bytes);result.passed=true;
  }catch(const TExceptionBase& e){result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){result.error=e.what();}
  if(!result.passed){result.bytes.clear();result.sha256.clear();}
  result.runtime=impl_->Status();return result;
}
ApplicationOptionsTransferResult ApplicationRuntime::ImportOptions(const std::string& bytes) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  OriginalInterpreterGuard guard;
  ApplicationOptionsTransferResult result;
  const std::string before=OptionsKey(impl_->app->GetOptions());
  try {
    if(bytes.empty() || bytes.size()>MaximumOptionsTransferBytes || bytes.find('\0')!=std::string::npos)throw std::invalid_argument("Select a nonempty original-format settings file, at most 1 MiB with no NUL bytes");
    std::string utf8=bytes;
    if(utf8.compare(0,3,"\xEF\xBB\xBF")==0)utf8.erase(0,3);
    if(Utf8(Text(utf8))!=utf8)throw std::invalid_argument("Settings documents must contain canonical UTF-8 text");
    AtomicFile input(impl_->config+"/.options-import");
    {std::ofstream out(input.Path(),std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());out.flush();if(!out)throw std::runtime_error("Cannot stage selected settings document");}
    TSettingsFile parsed;parsed.LoadUtf8Settings(Text(input.Path()));
    impl_->app->MergeOptions(parsed);
    result.importedOptions=parsed.ParamCount();result.sha256=Digest(bytes);result.passed=true;
  }catch(const TExceptionBase& e){result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){result.error=e.what();}
  result.optionsChanged=before!=OptionsKey(impl_->app->GetOptions());
  result.runtime=impl_->Status();return result;
}
}
