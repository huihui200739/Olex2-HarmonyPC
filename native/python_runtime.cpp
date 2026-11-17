#include "python_runtime.h"
#include "original_interpreter.h"
#include "pyext.h"
#include "ipimp.h"
#include "fsext.h"
#include "olxvar.h"
#include "eutf8.h"
#include <stdexcept>
#include <thread>
#include <vector>
// These are the genuine methods from upstream/repository/pyext.cpp.
extern PyMethodDef Methods[];
extern "C" PyObject* PyInit_olex_reg(void);
namespace olex2::harmony {
namespace {
std::string Utf8(const olxstr& s){return TUtf8::Encode(s).c_str();}
struct ProcessorScope {
  olex2::IOlex2Processor* previous;
  explicit ProcessorScope(olex2::IOlex2Processor* current):previous(olex2::IOlex2Processor::GetInstance()){olex2::IOlex2Processor::GetInstance()=current;}
  ~ProcessorScope(){olex2::IOlex2Processor::GetInstance()=previous;}
};
// Reuse the original callback registry rather than throw an uncaught C++
// logic_error from a Python callback registration.
class PythonProcessor final:public OriginalProcessor {
  olex2::OlexProcessorImp callbacks_{nullptr};
public:
  PythonProcessor(){olex2::IOlex2Processor::GetInstance()=this;}
  bool registerCallbackFunc(const olxstr& event,ABasicFunction* f) override{return callbacks_.registerCallbackFunc(event,f);}
  void unregisterCallbackFunc(const olxstr& event,const olxstr& name) override{callbacks_.unregisterCallbackFunc(event,name);}
  void callCallbackFunc(const olxstr& event,const TStrList& args) override{callbacks_.callCallbackFunc(event,args);}
};
struct PyRef {
  PyObject* value;
  explicit PyRef(PyObject* v=nullptr):value(v){}
  ~PyRef(){Py_XDECREF(value);}
  PyRef(const PyRef&)=delete;PyRef& operator=(const PyRef&)=delete;
};
std::string PyText(PyObject* value){
  if(!value)return {};
  if(!PyUnicode_Check(value))return {};
  const Py_ssize_t maximum=262144;
  PyRef limited(PyUnicode_Substring(value,0,PyUnicode_GetLength(value)<maximum?PyUnicode_GetLength(value):maximum));
  if(!limited.value)return {};
  Py_ssize_t length=0;const char* text=PyUnicode_AsUTF8AndSize(limited.value,&length);
  return text?std::string(text,static_cast<size_t>(length)):std::string();
}
std::string FetchPythonError(){
  PyObject *type=nullptr,*value=nullptr,*trace=nullptr;PyErr_Fetch(&type,&value,&trace);PyErr_NormalizeException(&type,&value,&trace);
  PyRef t(type),v(value),tb(trace);std::string result;
  PyRef module(PyImport_ImportModule("traceback"));
  if(module.value){
    PyRef formatter(PyObject_GetAttrString(module.value,"format_exception"));
    if(formatter.value){
      PyRef lines(PyObject_CallFunctionObjArgs(formatter.value,t.value?t.value:Py_None,v.value?v.value:Py_None,tb.value?tb.value:Py_None,nullptr));
      if(lines.value){PyRef empty(PyUnicode_FromString(""));PyRef text(PyUnicode_Join(empty.value,lines.value));result=PyText(text.value);}
    }
  }
  PyErr_Clear();
  if(result.empty() && v.value){PyRef text(PyObject_Str(v.value));result=PyText(text.value);PyErr_Clear();}
  return result.empty()?"Python execution failed":result;
}
struct PythonState {
  std::thread::id owner=std::this_thread::get_id();
  std::unique_ptr<PythonProcessor> processor;
  std::unique_ptr<TLibrary> privateLibrary;
  PythonExt* bridge=nullptr;
  PyThreadState* mainThread=nullptr;
  PythonRuntimeStatus status;
  bool attempted=false;
  std::vector<PyMethodDef> wrappedMethods;
};
PythonState* active=nullptr;
bool olexModuleRegistered=false;
bool olexRegModuleRegistered=false;
PyObject* InvokeOriginal(PyObject* self,PyObject* args,PyObject* keywords){
  if(!active || active->owner!=std::this_thread::get_id()){
    PyErr_SetString(PyExc_RuntimeError,"Original Olex2 calls must run on the owning application thread");return nullptr;
  }
  auto* method=static_cast<PyMethodDef*>(PyCapsule_GetPointer(self,"olex2.original-python-method"));
  if(!method)return nullptr;
  try{
    if(method->ml_flags&METH_KEYWORDS)return reinterpret_cast<PyCFunctionWithKeywords>(method->ml_meth)(nullptr,args,keywords);
    return method->ml_meth(nullptr,args);
  }catch(const TExceptionBase& e){PyErr_SetString(PyExc_RuntimeError,Utf8(e.GetException()->GetFullMessage()).c_str());}
  catch(const std::exception& e){PyErr_SetString(PyExc_RuntimeError,e.what());}
  catch(...){PyErr_SetString(PyExc_RuntimeError,"Original Python method raised a native exception");}
  return nullptr;
}
PyObject* InvokeOriginalArgs(PyObject* self,PyObject* args){return InvokeOriginal(self,args,nullptr);}
PyObject* InitializeOlexModule(){
  if(!active){PyErr_SetString(PyExc_RuntimeError,"Original application Python owner is unavailable");return nullptr;}
  PyObject* module=PythonExt::init_module(PythonExt::ModuleName(),Methods);
  if(!module)return nullptr;
  // Keep the upstream method table and handlers; wrappers only enforce the
  // application thread and translate native exceptions at the C boundary.
  size_t count=0;while(Methods[count].ml_name)++count;
  active->wrappedMethods.assign(Methods,Methods+count);
  for(size_t i=0;i<count;++i){
    auto& method=active->wrappedMethods[i];
    method.ml_meth=(method.ml_flags&METH_KEYWORDS)?reinterpret_cast<PyCFunction>(&InvokeOriginal):&InvokeOriginalArgs;
    PyRef token(PyCapsule_New(&Methods[i],"olex2.original-python-method",nullptr));
    PyRef function(token.value?PyCFunction_NewEx(&method,token.value,nullptr):nullptr);
    if(!function.value || PyObject_SetAttrString(module,method.ml_name,function.value)!=0){Py_DECREF(module);return nullptr;}
  }
  return module;
}
void CheckStatus(PyStatus status){if(PyStatus_Exception(status))throw std::runtime_error(status.err_msg?status.err_msg:"Python initialization failed");}
void AddPath(PyConfig& config,const std::string& path){
  wchar_t* wide=Py_DecodeLocale(path.c_str(),nullptr);if(!wide)throw std::runtime_error("Cannot decode Python module path");
  const auto result=PyWideStringList_Append(&config.module_search_paths,wide);PyMem_RawFree(wide);CheckStatus(result);
}
struct Config {
  PyConfig value;
  Config(){PyConfig_InitIsolatedConfig(&value);}
  ~Config(){PyConfig_Clear(&value);}
};
}
struct PythonRuntime::Impl:PythonState {
  Impl(){
    if(active || Py_IsInitialized())throw std::runtime_error("A Python interpreter owner already exists");
    processor=std::make_unique<PythonProcessor>();
    ProcessorScope scope(processor.get());
    try {
      bridge=&PythonExt::Init(processor.get());
      // This lazy original manager registers its real Python module. Trigger
      // registration before Py_InitializeFromConfig, never after the VM starts.
      (void)TFileHandlerManager::Count();
      // ExportLibrary creates the genuine spy binding library. Keep desktop
      // Reset/Run private until platform lifecycle/file handling supports them.
      privateLibrary.reset(bridge->ExportLibrary());
      active=this;status.prepared=true;status.version=PY_VERSION;
    } catch (...) {
      if (bridge) { PythonExt::Finilise(); bridge=nullptr; }
      throw;
    }
  }
  ~Impl(){
    if(!bridge)return;
    ProcessorScope scope(processor.get());
    if(mainThread){PyEval_RestoreThread(mainThread);mainThread=nullptr;}
    // The original Python-aware variable singleton owns Python references.
    // Release them while the VM and the owning thread GIL are still alive.
    TOlxVars::Finalise();
    PythonExt::Finilise();bridge=nullptr;if(active==this)active=nullptr;
  }
};
PythonRuntime::PythonRuntime():impl_(new Impl){}
PythonRuntime::~PythonRuntime()=default;
void PythonRuntime::AttachApplicationLibrary(TLibrary* library){
  if(!library)throw std::invalid_argument("Original application library is required");
  std::unique_ptr<TLibrary> owner(library);
  impl_->processor->GetLibrary().AttachLibrary(owner.get());owner.release();
}
void PythonRuntime::Initialize(const std::string& home,const std::string& sha){
  if(impl_->owner!=std::this_thread::get_id())throw std::runtime_error("Python initialization requires the application owner thread");
  if(impl_->status.initialized){if(home!=impl_->status.home || sha!=impl_->status.stdlibSha256)throw std::runtime_error("Cannot rebind the running Python environment");return;}
  if(impl_->attempted)throw std::runtime_error("Python initialization previously failed; restart the application before retrying");
  impl_->attempted=true;impl_->status.home=home;impl_->status.stdlibSha256=sha;
  ProcessorScope scope(impl_->processor.get());
  if(!olexModuleRegistered){if(PyImport_AppendInittab("olex",&InitializeOlexModule)<0)throw std::runtime_error("Cannot register the original Python module");olexModuleRegistered=true;}
  if(!olexRegModuleRegistered){if(PyImport_AppendInittab("olex_reg",&PyInit_olex_reg)<0)throw std::runtime_error("Cannot register the Harmony registry compatibility module");olexRegModuleRegistered=true;}
  try {
  Config config;config.value.install_signal_handlers=0;config.value.site_import=0;config.value.write_bytecode=0;config.value.parse_argv=0;config.value.configure_c_stdio=0;
  CheckStatus(PyConfig_SetBytesString(&config.value,&config.value.program_name,"olex2-harmonypc"));
  CheckStatus(PyConfig_SetBytesString(&config.value,&config.value.home,home.c_str()));
  AddPath(config.value,home+"/python311.zip");AddPath(config.value,home+"/plugins");config.value.module_search_paths_set=1;
  CheckStatus(Py_InitializeFromConfig(&config.value));
  {
    PyRef module(PyImport_ImportModule("olex"));
    if(!module.value){impl_->status.error=FetchPythonError();throw std::runtime_error(impl_->status.error);}
    PyRef registryModule(PyImport_ImportModule("olex_reg"));
    if(!registryModule.value){impl_->status.error=FetchPythonError();throw std::runtime_error(impl_->status.error);}
    impl_->status.originalBridgeImported=true;
  }
  impl_->status.initialized=true;impl_->status.error.clear();impl_->mainThread=PyEval_SaveThread();
  } catch (const std::exception& e) {
    impl_->status.error=e.what();
    if (Py_IsInitialized() && !impl_->mainThread) impl_->mainThread=PyEval_SaveThread();
    throw;
  } catch (...) {
    impl_->status.error="Native exception during Python initialization";
    if (Py_IsInitialized() && !impl_->mainThread) impl_->mainThread=PyEval_SaveThread();
    throw;
  }
}
PythonRuntimeStatus PythonRuntime::Status()const{return impl_->status;}
PythonExecutionResult PythonRuntime::Execute(const std::string& source){
  PythonExecutionResult result;
  if(!impl_->status.initialized || !impl_->mainThread)throw std::runtime_error("Application Python is not initialized");
  if(impl_->owner!=std::this_thread::get_id())throw std::runtime_error("Python execution requires the application owner thread");
  if(source.empty() || source.size()>1024*1024 || source.find('\0')!=std::string::npos)throw std::invalid_argument("Expected 1..1048576 UTF-8 script bytes without NUL");
  ProcessorScope scope(impl_->processor.get());PyEval_RestoreThread(impl_->mainThread);impl_->mainThread=nullptr;
  struct Release {Impl& state;~Release(){state.mainThread=PyEval_SaveThread();}} release{*impl_};
  ++impl_->status.executions;
  PyRef io(PyImport_ImportModule("io"));PyRef stream(io.value?PyObject_CallMethod(io.value,"StringIO",nullptr):nullptr);
  if(!stream.value){result.error=FetchPythonError();result.runtime=impl_->status;return result;}
  PyRef oldOut(Py_XNewRef(PySys_GetObject("stdout"))),oldErr(Py_XNewRef(PySys_GetObject("stderr")));
  struct RestoreStreams {PyObject* out;PyObject* err;~RestoreStreams(){PySys_SetObject("stdout",out);PySys_SetObject("stderr",err);PyErr_Clear();}} streams{oldOut.value,oldErr.value};
  if(PySys_SetObject("stdout",stream.value)!=0 || PySys_SetObject("stderr",stream.value)!=0){result.error=FetchPythonError();result.runtime=impl_->status;return result;}
  PyObject* main=PyImport_AddModule("__main__");PyObject* globals=main?PyModule_GetDict(main):nullptr;
  PyRef code(globals?Py_CompileString(source.c_str(),"<HarmonyPC>",Py_file_input):nullptr);
  PyRef value(code.value?PyEval_EvalCode(code.value,globals,globals):nullptr);
  result.passed=value.value!=nullptr;if(!result.passed)result.error=FetchPythonError();
  PyRef text(PyObject_CallMethod(stream.value,"getvalue",nullptr));
  if(text.value)result.output=PyText(text.value);
  else {const auto error=FetchPythonError();if(result.error.empty())result.error=error;result.passed=false;}
  impl_->status.error=result.error;result.runtime=impl_->status;return result;
}
}
