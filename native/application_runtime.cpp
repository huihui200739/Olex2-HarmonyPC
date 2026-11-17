#include "application_runtime.h"
#include <sstream>
#include "olxstate.h"
#include "xbond.h"
#include "xatom.h"
#include "original_file_formats.h"
#include "gradient_options.h"
#include "original_display_options.h"
#include "original_interpreter.h"
#include "harmony_mainform.h"
#include "png_gx_texture_app.h"
#include "persistent_gx_window.h"
#include "original_document_view.h"
#include "eutf8.h"
#include "wxzipfs.h"
#include "settingsfile.h"
#include "datafile.h"
#include "refmodel.h"
#include "platform_sha256.h"
#include "cif.h"
#include "ins.h"
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
namespace {
// The wx main form owns the original Save/Load macros (style/scene/view/...).
// Harmony has no wx form, so port the drawing-style subset over the same
// original renderer styles and expose it through the app library.
class HarmonyFormMacros : public IOlxObject {
  TGXApp& app_;
  static olxstr StylePath(const olxstr& name) {
    return TEFile::ChangeFileExt(name, "glds");
  }
public:
  explicit HarmonyFormMacros(TGXApp& app) : app_(app) {}
  void macSave(TStrObjList& Cmds, const TParamList&, TMacroData& Error) {
    if (Cmds.Count() == 0 || !Cmds[0].Equalsi("style")) {
      Error.ProcessingError(__OlxSrcInfo, "Harmony form supports 'save style <file>' only");
      return;
    }
    olxstr name = StylePath(Cmds.Text(' ', 1));
    if (name.IsEmpty()) {
      Error.ProcessingError(__OlxSrcInfo, "no file name is given");
      return;
    }
    TDataFile file;
    app_.GetRenderer().GetStyles().ToDataItem(file.Root().AddItem("style"));
    try {
      file.SaveToXLFile(name);
    }
    catch (const TExceptionBase&) {
      Error.ProcessingError(__OlxSrcInfo, "failed to save file: ") << name;
    }
  }
  void macMode(TStrObjList& Cmds, const TParamList& Options, TMacroData& Error) {
    if (Cmds.IsEmpty()) {
      Error.ProcessingError(__OlxSrcInfo, "mode name is expected");
      return;
    }
    olxstr name = Cmds[0], args;
    Cmds.Delete(0);
    args << Cmds.Text(' ');
    olxstr onExit;
    for (size_t i = 0; i < Options.Count(); i++) {
      if (Options.GetName(i) == 'e') {
        onExit = Options.GetValue(i);
        continue;
      }
      args << " -" << Options.GetName(i) << '=' << Options.GetValue(i);
    }
    TModeRegistry& modes = TModeRegistry::GetInstance();
    AMode* mode = modes.SetMode(name, args);
    if (mode == 0) {
      return;
    }
    try {
      if (!mode->Initialise(Cmds, Options)) {
        Error.ProcessingError(__OlxSrcInfo, "Current mode is unavailable");
        modes.ClearMode(false);
        return;
      }
      if (!onExit.IsEmpty()) {
        modes.OnModeExit.Add(onExit);
      }
    }
    catch (const TExceptionBase& e) {
      modes.ClearMode(false);
      throw TFunctionFailedException(__OlxSrcInfo, e);
    }
  }
  void macLoad(TStrObjList& Cmds, const TParamList&, TMacroData& Error) {
    if (Cmds.Count() == 0 || !Cmds[0].Equalsi("style")) {
      Error.ProcessingError(__OlxSrcInfo, "Harmony form supports 'load style <file>' only");
      return;
    }
    olxstr name = StylePath(Cmds.Text(' ', 1));
    if (name.IsEmpty()) {
      Error.ProcessingError(__OlxSrcInfo, "no file name is given");
      return;
    }
    if (!TEFile::Exists(name)) {
      Error.ProcessingError(__OlxSrcInfo, "file does not exist: ") << name;
      return;
    }
    TDataFile file;
    try {
      file.LoadFromXLFile(name, 0);
      app_.GetRenderer().ClearSelection();
      const vec3d mid = app_.GetRenderer().MinDim();
      const vec3d mad = app_.GetRenderer().MaxDim();
      app_.GetRenderer().GetStyles().FromDataItem(*file.Root().FindItem("style"), false);
      app_.CreateObjects(false);
      app_.GetRenderer().ClearMinMax();
      app_.GetRenderer().UpdateMinMax(mid, mad);
    }
    catch (const TExceptionBase& e) {
      app_.GetRenderer().GetStyles().Clear();
      Error.ProcessingError(__OlxSrcInfo, "failed to load style: ") << e.GetException()->GetFullMessage();
    }
  }
};
}
namespace olex2::harmony {
namespace {
// The desktop main form registers the original drawing-style macros. Harmony has
// no wx form, so the command library registers the same names here.
void HarmonyCommandMacroPers(TStrObjList&,const TParamList&,TMacroData&){TGXApp::GetInstance().AtomRad("pers");}
void HarmonyCommandMacroSfil(TStrObjList&,const TParamList&,TMacroData&){TGXApp::GetInstance().AtomRad("sfil");}
void HarmonyCommandMacroIsot(TStrObjList&,const TParamList&,TMacroData&){TGXApp::GetInstance().AtomRad("isot");}
void HarmonyCommandMacroBond(TStrObjList&,const TParamList&,TMacroData&){TGXApp::GetInstance().AtomRad("bond");}
void HarmonyCommandMacroTelp(TStrObjList&,const TParamList&,TMacroData&){TXAtom::Settings& settings=TXAtom::GetSettings(TGXApp::GetInstance().GetRenderer());settings.SetDS(adsEllipsoid);}
std::string Utf8(const olxstr& s){return TUtf8::Encode(s).c_str();}
olxstr Text(const std::string& s){return TUtf8::Decode(s.c_str());}
std::string Read(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot read application runtime file");return {std::istreambuf_iterator<char>(f),{}};}
std::string Digest(const std::string& bytes){return PlatformSha256(bytes);}
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
class PlatformApp final:public PngTextureGXApp {
  std::string config_;mutable TParamList persisted_;
public:
  mutable bool lastSaveCommitted=false;mutable unsigned saveRevision=0;
  bool optionsRestored=false;
  unsigned registeredFileFormatCount_=0;
  unsigned RegisteredFileFormatCount()const{return registeredFileFormatCount_;}
  PlatformApp(const std::string& base,PlatformWindowScene* scene):PngTextureGXApp(Text(base+"/olex2-harmony"),scene) {}
  void Configure(const std::string& shared,const std::string& instance,const std::string& config){
    config_=config;
    SetSharedDir(Text(shared));SetInstanceDir(Text(instance));SetConfigDir(Text(config));
    // Original ReadOptions also edits PATH. Use the original format reader
    // explicitly instead; never execute desktop environment/launcher behavior.
    const std::string path=config_+"/.options";
    if(access(path.c_str(),F_OK)==0){TSettingsFile settings;settings.LoadUtf8Settings(Text(path));for(size_t i=0;i<settings.ParamCount();++i){ValidateOption(settings.ParamName(i),settings.ParamValue(i));UpdateOption(settings.ParamName(i),settings.ParamValue(i));}optionsRestored=true;}
    persisted_=GetOptions();
    const char* argv[]={"olex2-harmony"};InitArguments(1,argv);
    // Use the Harmony branch of the original launcher to register and verify
    // every native original-format parser without constructing wxWidgets UI.
    registeredFileFormatCount_=RegisterOriginalFileFormats(*this);
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
  mutable std::mutex mutex;OriginalDocumentState document;PlatformSceneLifetime sceneLifetime;PlatformWindowScene* scene=nullptr;std::unique_ptr<PlatformApp> app;std::unique_ptr<OriginalLogCapture> log;std::string base,shared,instance,config;unsigned functionCount=0;std::unique_ptr<PythonRuntime> python;std::string pythonHome,pythonInitializationError;
  // Declared last so access is revoked before Python, app, scene and mutex.
  std::unique_ptr<ui::GradientStoreLifetime> gradientAccess;
  ApplicationRuntimeStatus Status()const{
    ApplicationRuntimeStatus s;s.initialized=bool(app);if(!app)return s;
    s.originalCoreInitialized=TBasicApp::HasInstance() && &TXApp::GetInstance()==app.get();s.originalCoreMacroCount=app->GetLibrary().MacroCount();s.originalCoreFunctionCount=app->GetLibrary().FunctionCount();s.originalCoreFileCount=app->XFiles().Count();s.originalCoreEmptyFile=s.originalCoreFileCount==1 && !app->XFile().HasLastLoader();s.originalFileFormatCount=app->RegisteredFileFormatCount();
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
  // Original file-manager construction registers Python modules, so prepare
  // the genuine bridge before constructing the unique original application.
  TwxZipFileSystem::RegisterFactory();
  impl_->python.reset(new PythonRuntime);impl_->pythonHome=runtime+"/python-v1";
  auto scene=std::make_unique<PlatformWindowScene>(impl_->sceneLifetime);
  impl_->app.reset(new PlatformApp(impl_->base,scene.get()));
  impl_->scene=scene.release(); // The genuine TGXApp renderer now owns it.
  // The original wx main form creates these process-lifetime registries. Harmony
  // has no form, so create them before any command or state callback needs them.
  // Their constructors set the singleton pointer and throw if it already exists.
  try{new TStateRegistry();}catch(const TExceptionBase&){/* already created */}
  try{new TModeRegistry();}catch(const TExceptionBase&){/* already created */}
  impl_->app->Configure(impl_->shared,impl_->instance,impl_->config);
  impl_->log.reset(new OriginalLogCapture(impl_->app->GetLog()));
  std::unique_ptr<TLibrary> library(impl_->app->ExportLibrary("app"));
  {
    // The original main form owns these macros; register the Harmony port in the
    // application library. A pre-existing registration must not abort startup.
    auto* formMacros = new HarmonyFormMacros(*impl_->app);
    TLibrary& applicationLibrary = impl_->app->GetLibrary();
    try {
      applicationLibrary.Register(new TMacro<HarmonyFormMacros>(formMacros, &HarmonyFormMacros::macSave,
        "Save", EmptyString(), fpAny ^ fpNone,
        "Saves style <file> to an original .glds drawing style file"));
    }
    catch (const TExceptionBase&) {}
    try {
      applicationLibrary.Register(new TMacro<HarmonyFormMacros>(formMacros, &HarmonyFormMacros::macLoad,
        "Load", EmptyString(), fpAny ^ fpNone,
        "Loads style <file> from an original .glds drawing style file"));
    }
    catch (const TExceptionBase&) {}
    try {
      applicationLibrary.Register(new TMacro<HarmonyFormMacros>(formMacros, &HarmonyFormMacros::macMode,
        "Mode",
        "a-[name] autocomplete; [grow] grow (rebuild) asymmetric unit only; [fit] afix&;"
        "p-[name] prefix; [grow] inserts the new atoms into the AU with given [-1] part value&;"
        "s-[grow] short interactions; [name] suffix; [fit] split, atoms to split offset [0]&;"
        "t-[name] type&;temperature correction for distance [hfix]"
        "c-[grow] covalent bonds; [move] copy fragments instead of moving&;"
        "r-[split] a restraint/constraint for split atoms; [grow] show radial bonds between the same atoms&;"
        "v-[grow] use user provided delta for connectivity analysis, default 2A&;"
        "shells-[grow] grow atom shells vs fragments&;"
        "l-[name] lock atom types after naming; [grow] list contacts&;"
        "e-a macro to run on mode exit&;"
        "d-[hfix] distance&;"
        "i-[fit, false] use only the selected atoms for 1-3 distances&;",
        (fpAny ^ fpNone) | psFileLoaded,
        "Turns specified mode on. Valid mode: fixu, fixc, grow, himp, match, move, name, occu, pack, part, split, fit"));
    }
    catch (const TExceptionBase&) {}
  }
  impl_->functionCount=library->FunctionCount();impl_->python->AttachApplicationLibrary(library.release());
  auto* state=impl_.get();
  impl_->gradientAccess=std::make_unique<ui::GradientStoreLifetime>([state]{
    std::lock_guard<std::mutex> lock(state->mutex);OriginalInterpreterGuard guard;
    if(!state->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=state->app.get())throw std::runtime_error("Original background owner is unavailable");
    auto* background=state->app->GetRenderer().Background();if(!background)throw std::runtime_error("Original background is unavailable");
    return ui::ReadOriginalGradientBackground(*background);
  },[state](const ui::GradientColors& colors){
    std::lock_guard<std::mutex> lock(state->mutex);OriginalInterpreterGuard guard;
    if(!state->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=state->app.get())throw std::runtime_error("Original background owner is unavailable");
    auto* background=state->app->GetRenderer().Background();if(!background)throw std::runtime_error("Original background is unavailable");
    ui::ApplyOriginalGradientBackground(*background,colors);
  });
}
ApplicationRuntime::~ApplicationRuntime()=default;
std::shared_ptr<ui::GradientStore> ApplicationRuntime::GradientBackgroundStore(){
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if(!impl_->gradientAccess)throw std::runtime_error("Original background access is unavailable");
  return impl_->gradientAccess->Store();
}
std::string ApplicationRuntime::RunOriginalWindow(const Olex2LegacyHost& host){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  if(host.document_request) {
    if(!impl_->app || !impl_->scene)throw std::runtime_error("Original document owner is unavailable");
    return RunOriginalDocumentLocked(host,*impl_->app,*impl_->scene,impl_->sceneLifetime,impl_->document);
  }
  if(impl_->document.compatibilityReady)
    throw std::runtime_error("Close the original document before running isolated graphics diagnostics");
  const auto status=impl_->Status();
  if(!impl_->app || !impl_->scene || !status.originalCoreInitialized || !status.originalCoreEmptyFile)
    throw std::runtime_error("The unique original GX runtime is unavailable");
  return RunPersistentGXWindowLocked(host,status,*impl_->app,*impl_->scene,impl_->sceneLifetime,impl_->log.get());
}
std::string ApplicationRuntime::ReadOriginalDocumentSource(){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  if(!impl_->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=impl_->app.get())
    throw std::runtime_error("Original document owner is unavailable");
  return ReadOriginalDocumentSourceLocked(*impl_->app,impl_->document);
}
ApplicationRuntimeStatus ApplicationRuntime::Status()const{std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->Status();}
ApplicationFunctionResult ApplicationRuntime::Evaluate(const std::string& input){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;ApplicationFunctionResult r;r.input=input;const auto before=OptionsKey(impl_->app->GetOptions());
  try{
    if(input.empty() || input.size()>4096 || input.find('\0')!=std::string::npos || input.find('(')==std::string::npos)throw std::invalid_argument("Expected an original application function expression, at most 4096 UTF-8 bytes");
    OriginalProcessor processor;
    std::unique_ptr<TLibrary> appLibrary(impl_->app->ExportLibrary("app"));
    r.runtime.originalFunctionCount=appLibrary->FunctionCount();
    processor.GetLibrary().AttachLibrary(appLibrary.release());
    processor.GetLibrary().AttachLibrary(CreateHarmonyMainFormLibrary());
    olxstr text=Text(input);TMacroData error;error.SetQuiet(true);error.SetLocation("HarmonyPC original application function");r.passed=processor.Evaluate(text,error);if(r.passed)r.text=Utf8(text);else r.error=Utf8(error.GetInfo());
  }catch(const TExceptionBase& e){r.error=Utf8(e.GetException()->GetFullMessage());}catch(const std::exception& e){r.error=e.what();}
  if(!r.passed && r.error.empty())r.error="Original application function failed";
  r.optionsChanged=before!=OptionsKey(impl_->app->GetOptions());r.runtime=impl_->Status();return r;
}
ApplicationCommandResult ApplicationRuntime::ExecuteCommand(const std::string& input){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  ApplicationCommandResult r;r.input=input;const auto before=impl_->log->Snapshot();
  try{
    if(input.empty() || input.size()>4096 || input.find('\0')!=std::string::npos ||
        Utf8(Text(input))!=input)
      throw std::invalid_argument("Expected a canonical UTF-8 original command, at most 4096 bytes");
    OriginalProcessor processor;TLibrary& library=processor.GetLibrary();
    // Replicate the application's own registry: the app library owns the real
    // macros/functions, so moving or borrowing it would corrupt the runtime.
    size_t replicated=0;
    {
      TLibrary::container_t::iterator_t itr=impl_->app->GetLibrary().IterateMacros();
      while(itr->HasNext()){
        ABasicFunction* original=itr->Next();
        std::unique_ptr<ABasicFunction> copy(original->Replicate());
        AMacro* macro=dynamic_cast<AMacro*>(copy.get());
        if(!macro)throw std::runtime_error("Original library macro could not be replicated");
        try{library.Register(macro);copy.release();++replicated;}
        catch(const TExceptionBase&){/* keep the interpreter builtin with this name */}
      }
    }
    {
      TLibrary::container_t::iterator_t itr=impl_->app->GetLibrary().IterateFunctions();
      while(itr->HasNext()){
        ABasicFunction* original=itr->Next();
        std::unique_ptr<ABasicFunction> copy(original->Replicate());
        AFunction* function=dynamic_cast<AFunction*>(copy.get());
        if(!function)throw std::runtime_error("Original library function could not be replicated");
        try{library.Register(function);copy.release();++replicated;}
        catch(const TExceptionBase&){/* keep the interpreter builtin with this name */}
      }
    }
    if(replicated==0)throw std::runtime_error("Original application macro library is empty");
    library.AttachLibrary(CreateHarmonyMainFormLibrary());
    // The application library may already provide some of these names; keep the
    // original registration and only add the missing drawing styles.
    struct StyleMacro { void (*fn)(TStrObjList&,const TParamList&,TMacroData&); const char* name; const char* description; };
    const StyleMacro styleMacros[] = {
      {&HarmonyCommandMacroPers,"pers","Draw atoms with the original default radii (balls and sticks)"},
      {&HarmonyCommandMacroSfil,"sfil","Draw atoms as space filling spheres"},
      {&HarmonyCommandMacroIsot,"isot","Draw atoms with isotropic thermal displacement radii"},
      {&HarmonyCommandMacroBond,"bond","Draw atoms at bond radii"},
      {&HarmonyCommandMacroTelp,"telp","Draw anisotropic atoms as thermal ellipsoids"},
    };
    for(const auto& styleMacro:styleMacros){
      try{library.Register(new esdl::TStaticMacro(styleMacro.fn,styleMacro.name,EmptyString(),fpNone,styleMacro.description));}
      catch(const TExceptionBase&){/* the original application macro already exists */}
    }
    olxstr text=Text(input);TMacroData error;error.SetQuiet(true);
    error.SetLocation("HarmonyPC original command");
    r.passed=processor.processMacroEx(text,error,"HarmonyPC original command",true);
    if(r.passed){r.text=Utf8(error.GetRetVal());if(r.text.empty())r.text="ok";}
    else r.error=Utf8(error.GetInfo());
  }catch(const TExceptionBase& e){r.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){r.error=e.what();}
  const auto after=impl_->log->Snapshot();
  // Only the bounded log entries produced by this command, in order.
  std::ostringstream output;
  for(const auto& entry:after.entries){
    if(entry.sequence<=before.posted)continue;
    if(output.tellp()>65536)break;
    output<<entry.text<<'\n';
  }
  r.output=output.str();
  if(r.output.size()>65536)r.output.resize(65536);
  if(!r.passed && r.error.empty())r.error="Original command was not accepted";
  r.runtime=impl_->Status();return r;
}
OriginalBadReflectionsResult ApplicationRuntime::ReadOriginalBadReflections(){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  OriginalBadReflectionsResult result;
  try{
    if(!impl_->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=impl_->app.get())
      throw std::runtime_error("Original document owner is unavailable");
    RefinementModel& rm=impl_->app->XFile().GetRM();
    const TTypeList<RefinementModel::BadReflection>& bad=rm.GetBadReflectionList();
    const vec3i_list& omits=rm.GetOmits();
    std::ostringstream o;
    o<<"{\"passed\":true,\"count\":"<<bad.Count()<<",\"hklf\":"<<rm.GetHKLF()<<",\"reflections\":[";
    const size_t limit=bad.Count()<500?bad.Count():500;
    for(size_t i=0;i<limit;i++){
      const auto& item=bad[i];
      bool omitted=false;
      for(size_t j=0;j<omits.Count();j++)if(omits[j]==item.index){omitted=true;break;}
      if(i)o<<',';
      o<<"{\"h\":"<<item.index[0]<<",\"k\":"<<item.index[1]<<",\"l\":"<<item.index[2]
       <<",\"fo\":"<<item.Fo<<",\"fc\":"<<item.Fc<<",\"esd\":"<<item.esd
       <<",\"factor\":"<<item.factor<<",\"omitted\":"<<(omitted?"true":"false")<<"}";
    }
    o<<"]}";
    result.passed=true;result.count=static_cast<unsigned>(bad.Count());
    result.hklf=rm.GetHKLF();result.json=o.str();
  }catch(const TExceptionBase& e){result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){result.error=e.what();}
  catch(...){result.error="Unknown original bad reflection exception";}
  return result;
}
ApplicationSelectionLabelResult ApplicationRuntime::CreateSelectionLabel(const std::string& text){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  ApplicationSelectionLabelResult r;
  try{
    if(!impl_->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=impl_->app.get())
      throw std::runtime_error("Original document owner is unavailable");
    auto& selection=impl_->app->GetRenderer().GetSelection();
    vec3d centre;size_t count=0;
    for(size_t i=0;i<selection.Count();++i){
      auto& item=selection[i];
      if(item.Is<TXAtom>()){centre+=static_cast<TXAtom&>(item).crd();++count;}
      else if(item.Is<TXBond>()){centre+=static_cast<TXBond&>(item).GetCenter();++count;}
    }
    if(count==0)throw std::runtime_error("Select original objects before creating a selection label");
    centre/=static_cast<double>(count);
    std::string caption=text;
    if(caption.empty()){std::ostringstream out;out<<"选中 "<<count<<" 个原版对象";caption=out.str();}
    impl_->app->CreateLabel(centre,Text(caption),4);
    r.passed=true;r.objectCount=count;r.text=caption;
  }catch(const std::exception& e){r.error=e.what();}catch(...){r.error="Original selection label creation failed";}
  return r;
}
ApplicationBondRadiusResult ApplicationRuntime::SetBondRadius(double radius){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  ApplicationBondRadiusResult r;
  try{
    if(!(radius>0) || radius>5)throw std::invalid_argument("Bond radius must be between 0 and 5");
    if(!impl_->app||!TBasicApp::HasInstance()||&TGXApp::GetInstance()!=impl_->app.get())
      throw std::runtime_error("Original document owner is unavailable");
    TXBond::Settings& settings=TXBond::GetSettings(impl_->app->GetRenderer());
    settings.SetRadius(radius);
    r.radius=settings.GetRadius();
    r.passed=true;
  }catch(const std::exception& e){r.error=e.what();}catch(...){r.error="Original bond radius update failed";}
  return r;
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

namespace olex2::harmony {
PythonRuntimeStatus ApplicationRuntime::InitializePython(const std::string& bytes){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  PythonRuntimeStatus status;
  try{
    if(!impl_->python)throw std::runtime_error("Original Python owner is unavailable");
    if(bytes.size()>32*1024*1024 || Digest(bytes)!="f3de63dc85354f89bb41ffc63c48bf68655220d75be44abbbec5f2275711580d")throw std::invalid_argument("Packaged Python standard-library bytes differ from the pinned input");
    Directory(impl_->pythonHome);Directory(impl_->pythonHome+"/plugins");
    const std::string destination=impl_->pythonHome+"/python311.zip";
    if(access(destination.c_str(),F_OK)!=0 || Digest(Read(destination))!=Digest(bytes)){
      AtomicFile file(destination);{std::ofstream out(file.Path(),std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());out.flush();if(!out)throw std::runtime_error("Cannot stage Python standard library");}file.Commit();
    }
    impl_->python->Initialize(impl_->pythonHome,Digest(bytes));status=impl_->python->Status();impl_->pythonInitializationError.clear();
  }catch(const TExceptionBase& e){if(impl_->python)status=impl_->python->Status();status.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){if(impl_->python)status=impl_->python->Status();status.error=e.what();}
  impl_->pythonInitializationError=status.error;
  return status;
}
PythonRuntimeStatus ApplicationRuntime::GetPythonStatus() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  auto status=impl_->python?impl_->python->Status():PythonRuntimeStatus();
  if(!impl_->pythonInitializationError.empty())status.error=impl_->pythonInitializationError;
  return status;
}
PythonExecutionResult ApplicationRuntime::RunPython(const std::string& source){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;PythonExecutionResult result;const auto before=OptionsKey(impl_->app->GetOptions());
  try{if(!impl_->python)throw std::runtime_error("Original Python owner is unavailable");result=impl_->python->Execute(source);}
  catch(const TExceptionBase& e){result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){result.error=e.what();}
  result.optionsChanged=before!=OptionsKey(impl_->app->GetOptions());if(impl_->python)result.runtime=impl_->python->Status();return result;
}
}
