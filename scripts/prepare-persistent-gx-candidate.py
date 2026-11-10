#!/usr/bin/env python3
"""Create a separate source-built persistent GX owner candidate; no promotion."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE='upstream/gxlib/gxapp.cpp: original TGXApp empty startup with one long-lived renderer and platform scene (core metadata catalogs separately exposed); upstream/sdl/bapp.cpp: original TBasicApp directories and ExportLibrary functions + settingsfile.cpp: original settings format; platform-owned private directories and atomic SaveOptions override'
def replace_once(s,old,new):
 if s.count(old)!=1:raise ValueError('Unexpected source shape: '+old[:90])
 return s.replace(old,new,1)
def prepare(base,out,version="0.22.13"):
 base=base.resolve();out=out.resolve()
 if out.exists():raise FileExistsError(out)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (identity['versionName'],identity['versionCode'])!=('0.22.11',221100):raise ValueError('Requires frozen source11')
 lock=json.loads((base/'source.lock.json').read_text())
 if len(lock['patches'])!=19:raise ValueError('Requires exactly 19 fixed patches')
 shutil.copytree(base,out,ignore=lambda d,n:[x for x in n if x in ('reports','out','__pycache__','.hvigor','oh_modules')])
 for name in ('platform_window_scene.h','persistent_gx_window.h','persistent_gx_window.cpp'):
  shutil.copy2(ROOT/'experiments/gx-persistent-runtime/native'/name,out/'native'/name)
 p=out/'native/application_runtime.cpp';s=p.read_text()
 s=replace_once(s,'#include "xapp.h"','#include "png_gx_texture_app.h"\n#include "persistent_gx_window.h"')
 s=replace_once(s,'class PlatformApp final:public TXApp {','class PlatformApp final:public PngTextureGXApp {')
 s=replace_once(s,'PlatformApp(const std::string& base,const std::string& shared,const std::string& instance,const std::string& config):TXApp(Text(base+"/olex2-harmony")),config_(config){','PlatformApp(const std::string& base,PlatformWindowScene* scene):PngTextureGXApp(Text(base+"/olex2-harmony"),scene) {}\n  void Configure(const std::string& shared,const std::string& instance,const std::string& config){\n    config_=config;')
 s=replace_once(s,'mutable std::mutex mutex;std::unique_ptr<PlatformApp> app;','mutable std::mutex mutex;PlatformSceneLifetime sceneLifetime;PlatformWindowScene* scene=nullptr;std::unique_ptr<PlatformApp> app;')
 s=replace_once(s,'  impl_->app.reset(new PlatformApp(impl_->base,impl_->shared,impl_->instance,impl_->config));','  auto scene=std::make_unique<PlatformWindowScene>(impl_->sceneLifetime);\n  impl_->app.reset(new PlatformApp(impl_->base,scene.get()));\n  impl_->scene=scene.release(); // The genuine TGXApp renderer now owns it.\n  impl_->app->Configure(impl_->shared,impl_->instance,impl_->config);')
 needle='ApplicationRuntime::~ApplicationRuntime()=default;'
 s=replace_once(s,needle,needle+'''\nstd::string ApplicationRuntime::RunOriginalWindow(const Olex2LegacyHost& host){
  std::lock_guard<std::mutex> lock(impl_->mutex);OriginalInterpreterGuard guard;
  const auto status=impl_->Status();
  if(!impl_->app || !impl_->scene || !status.originalCoreInitialized || !status.originalCoreEmptyFile)
    throw std::runtime_error("The unique original GX runtime is unavailable");
  return RunPersistentGXWindowLocked(host,status,*impl_->app,*impl_->scene,impl_->sceneLifetime);
}''')
 p.write_text(s)
 p=out/'native/application_runtime.h';s=p.read_text()
 s=replace_once(s,'namespace olex2::harmony {','struct Olex2LegacyHost;\nnamespace olex2::harmony {')
 s=replace_once(s,'  ApplicationRuntimeStatus Status() const;','  ApplicationRuntimeStatus Status() const;\n  std::string RunOriginalWindow(const Olex2LegacyHost& host);')
 start=s.index('constexpr const char* ApplicationRuntimeSource()');end=s.index('\nconstexpr const char* CoreCatalogSource()',start)
 s=s[:start]+'constexpr const char* ApplicationRuntimeSource(){return '+json.dumps(SOURCE)+';}'+s[end:]
 s=s.replace('// Actual TXApp empty core startup and TBasicApp directory/registry layer.','// Unique persistent TGXApp owner, original renderer and platform scene.')
 s=s.replace('// This is not TGXApp/Olex2App/wx or the complete Olex2 entry point.','// Font, original UI and complete Olex2 entry point remain unfinished.')
 p.write_text(s)
 # Exported entrypoint dispatches into the runtime: never take the original
 # guard first and recursively call an API that takes that same guard again.
 (out/'native/original_renderer_bridge.cpp').write_text('''#include "original_renderer_host.h"
#include "application_runtime.h"
#include "exception.h"
#include <sstream>
#include <stdexcept>
namespace {
std::string result;
std::string Quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\\\')o<<'\\\\'<<c;else if(c>=32)o<<c;else o<<"\\\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}o<<'"';return o.str();}
}
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost* host){
  if(host && host->completed)*host->completed=0;
  try{
    if(!host || !host->existing_runtime)throw std::invalid_argument("Missing existing original GX runtime");
    result=static_cast<olex2::harmony::ApplicationRuntime*>(host->existing_runtime)->RunOriginalWindow(*host);
    if(host->completed)*host->completed=1;
  }catch(const TExceptionBase& e){result="{\\"execution_completed\\":false,\\"error\\":"+Quote(e.GetException()->GetFullMessage().ToMBStr().c_str())+"}";
  }catch(const std::exception& e){result="{\\"execution_completed\\":false,\\"error\\":"+Quote(e.what())+"}";
  }catch(...){result="{\\"execution_completed\\":false,\\"error\\":\\"Unknown persistent original renderer exception\\"}";}
  return result.c_str();
}
''')
 with (out/'app/entry/src/main/cpp/CMakeLists.txt').open('a') as f:f.write('\n# Persistent GX window implementation remains inside the existing core DSO.\ntarget_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/persistent_gx_window.cpp")\n')
 identity.update(versionName=version,versionCode=220000+int(version.split('.')[-1])*100)
 p=out/'app/AppScope/app.json5';d=json.loads(p.read_text());d['app']=identity;p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n')
 lock['evaluation_scope']='persistent unique TGXApp owner, renderer and bindable platform scene; candidate qualification pending; full port incomplete'
 (out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
 manifest={str(p.relative_to(out)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in [out/'native'/n for n in ('application_runtime.cpp','application_runtime.h','original_renderer_bridge.cpp','platform_window_scene.h','persistent_gx_window.h','persistent_gx_window.cpp')]}
 (out/'persistent-gx-inputs.json').write_text(json.dumps({'version':version,'base':'0.22.11','files':manifest,'full_port_complete':False,'functional_completion_percent':None},indent=2)+'\n')
 print(json.dumps({'source':str(out),'version':version,'build_executed':False,'qualification':'pending','full_port_complete':False},indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--version',choices=('0.22.13',),default='0.22.13');a=p.parse_args();prepare(a.base,a.output,a.version)
