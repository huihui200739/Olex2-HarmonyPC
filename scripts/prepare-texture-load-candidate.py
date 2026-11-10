#!/usr/bin/env python3
"""Prepare a new candidate with genuine LoadTextures in the unique GX runtime."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Unexpected source shape: ' + old[:110])
    return text.replace(old, new, 1)

def prepare(base, out):
    base, out = base.resolve(), out.resolve()
    if out.exists():
        raise FileExistsError(out)
    identity = json.loads((base/'app/AppScope/app.json5').read_text())['app']
    if (identity['versionName'], identity['versionCode']) != ('0.22.14', 221400):
        raise ValueError('Requires the frozen candidate14')
    lock = json.loads((base/'source.lock.json').read_text())
    if len(lock['patches']) != 20:
        raise ValueError('Requires all twenty frozen patches')
    manifest = json.loads((base/'texture-context-inputs.json').read_text())
    for name, record in manifest['files'].items():
        path = base/name
        if sha(path) != record['sha256'] or path.stat().st_size != record['bytes']:
            raise ValueError('Frozen candidate14 input changed: ' + name)
    for row in lock['patches']:
        if sha(base/row['file']) != row['sha256']:
            raise ValueError('Frozen patch changed: ' + row['file'])
    for row in lock['patched_files']:
        if sha(base/'upstream'/row['file']) != row['sha256']:
            raise ValueError('Frozen upstream changed: ' + row['file'])
    shutil.copytree(base, out, ignore=lambda directory, names: [n for n in names if n in
                    ('out', 'reports', '__pycache__', '.hvigor', 'oh_modules')])
    changed = []
    def edit(name, old, new):
        path = out/name
        path.write_text(once(path.read_text(), old, new))
        if name not in changed:
            changed.append(name)
    for name in ('original_texture_load_probe.h', 'original_texture_load_probe.cpp', 'original_texture_load_fixtures.h'):
        shutil.copy2(ROOT/'experiments/gx-texture-load/native'/name, out/'native'/name)
        changed.append('native/' + name)
    edit('native/original_renderer_host.h', '  int* completed;\n',
         '  int* completed;\n  bool load_platform_textures=false; // private same-build route, default old diagnostics\n')
    edit('native/persistent_gx_window.h', 'struct ApplicationRuntimeStatus;',
         'struct ApplicationRuntimeStatus;\nclass OriginalLogCapture;')
    edit('native/persistent_gx_window.h', 'TGXApp&, PlatformWindowScene&, PlatformSceneLifetime&);',
         'TGXApp&, PlatformWindowScene&, PlatformSceneLifetime&, OriginalLogCapture* = nullptr);')
    edit('native/application_runtime.cpp', 'return RunPersistentGXWindowLocked(host,status,*impl_->app,*impl_->scene,impl_->sceneLifetime);',
         'return RunPersistentGXWindowLocked(host,status,*impl_->app,*impl_->scene,impl_->sceneLifetime,impl_->log.get());')
    edit('native/persistent_gx_window.cpp', '#include "persistent_texture_window_probe.h"',
         '#include "persistent_texture_window_probe.h"\n#include "original_texture_load_probe.h"\n#include "png_gx_texture_app.h"\n#include "original_log.h"')
    edit('native/persistent_gx_window.cpp', 'TGXApp& app,PlatformWindowScene& scene,PlatformSceneLifetime& lifetime){',
         'TGXApp& app,PlatformWindowScene& scene,PlatformSceneLifetime& lifetime,OriginalLogCapture* textureLog){')
    branch = r'''
    if(h.load_platform_textures){
      auto* pngApp=dynamic_cast<PngTextureGXApp*>(&app);
      if(!pngApp || !textureLog)throw std::runtime_error("Missing unique PNG GX owner/log observer");
      const std::string loaded=RunOriginalTextureLoadProbe(h,*pngApp,*textureLog,w,hh);
      lease.Close();CheckGL("original loaded textures detach before compatibility shutdown");
      const bool detached=OriginalLoadedTexturesDetached(*pngApp);
      if(!detached || scene.IsBound() || scene.IsEnabled() || lifetime.cleanupFailed)
        throw std::runtime_error("Original loaded textures did not retire before GL4ES shutdown");
      out<<"{\"run\":"<<h.run<<",\"width\":"<<w<<",\"height\":"<<hh
         <<",\"shared_existing_runtime\":true,\"unique_owner\":"<<(uniqueOwner?"true":"false")
         <<",\"owner_address\":"<<reinterpret_cast<uintptr_t>(&app)
         <<",\"renderer_address\":"<<reinterpret_cast<uintptr_t>(&genuine)
         <<",\"scene_address\":"<<reinterpret_cast<uintptr_t>(&scene)
         <<",\"binding_before\":"<<bindingBefore<<",\"binding_after\":"<<lifetime.bindings
         <<",\"detachment_before\":"<<detachmentBefore<<",\"detachment_after\":"<<lifetime.detachments
         <<",\"textures_detached_before_gl4es_close\":true,\"scene_unbound\":true,\"scene_disabled\":true"
         <<",\"texture_load\":"<<loaded<<",\"execution_completed\":true,\"error\":\"\"}";
      return out.str();
    }
'''
    edit('native/persistent_gx_window.cpp', '    CheckGL("compatibility initialize");',
         '    CheckGL("compatibility initialize");\n' + branch)
    cpp = 'app/entry/src/main/cpp/'
    edit(cpp+'shared_original_probe.h', 'int,int,void*,const std::string&);',
         'int,int,void*,const std::string&,bool loadTextures=false);')
    edit(cpp+'shared_original_probe.cpp', 'int w,int h,void* runtime,const std::string& directory){',
         'int w,int h,void* runtime,const std::string& directory,bool loadTextures){')
    edit(cpp+'shared_original_probe.cpp', 'const std::string artifactDirectory="original-renderer-pid-"+',
         'const std::string artifactDirectory=std::string(loadTextures?"original-texture-load-pid-":"original-renderer-pid-")+')
    edit(cpp+'shared_original_probe.cpp', 'run,ActualGPUSnapshot,&completed};',
         'run,ActualGPUSnapshot,&completed,loadTextures};')
    edit(cpp+'renderer.h', 'ProbeOriginalRenderer(void* runtime,const std::string& privateDirectory);',
         'ProbeOriginalRenderer(void* runtime,const std::string& privateDirectory,bool loadTextures=false);')
    edit(cpp+'renderer.cpp', 'ProbeOriginalRenderer(void* runtime,const std::string& directory){',
         'ProbeOriginalRenderer(void* runtime,const std::string& directory,bool loadTextures){')
    edit(cpp+'renderer.cpp', 'status_.height,runtime,directory);',
         'status_.height,runtime,directory,loadTextures);')
    edit(cpp+'renderer.cpp', 'directory+"/shared-original-integrated-run-"+',
         'directory+(loadTextures?"/shared-original-texture-load-run-":"/shared-original-integrated-run-")+')
    old='napi_value ProbeOriginalRenderer(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);return String(env,Renderer::Instance().ProbeOriginalRenderer(owner.runtime.get(),owner.filesDir));}'
    edit(cpp+'napi_init.cpp', old, old+'\nnapi_value ProbeOriginalTextures(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);return String(env,Renderer::Instance().ProbeOriginalRenderer(owner.runtime.get(),owner.filesDir,true));}')
    old='    {"runOriginalRendererProbe",nullptr,ProbeOriginalRenderer,nullptr,nullptr,nullptr,napi_default,nullptr},'
    edit(cpp+'napi_init.cpp', old, old+'\n    {"runOriginalTextureLoadProbe",nullptr,ProbeOriginalTextures,nullptr,nullptr,nullptr,napi_default,nullptr},')
    old='export const runOriginalRendererProbe: () => string;'
    edit(cpp+'types/libolex2native/index.d.ts', old, old+'\nexport const runOriginalTextureLoadProbe: () => string;')
    ets='app/entry/src/main/ets/pages/Index.ets'
    old="  @State private originalRendererProbe: string = '';"
    edit(ets, old, old+"\n  @State private originalTextureLoadProbe: string = '';")
    button='''        Button(this.originalTextureLoadProbe.length === 0 ? '加载原版纹理' : '再次加载纹理')
          .id('olex2-original-texture-load-probe').fontSize(11).height(30).width(108)
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed()).onClick(() => {
            this.originalTextureLoadProbe = nativeBridge.runOriginalTextureLoadProbe();
            this.originalLog = nativeBridge.getOriginalLogs();
            this.refreshGraphics();
          })
'''
    marker="        Button(this.originalProbeLabel()).id('olex2-original-renderer-probe')"
    edit(ets, marker, button+marker)
    cmake=out/(cpp+'CMakeLists.txt')
    cmake.write_text(cmake.read_text()+'\n# Genuine original named UI texture loading into the long-lived renderer.\ntarget_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/original_texture_load_probe.cpp")\n')
    changed.append(cpp+'CMakeLists.txt')
    path=out/'app/AppScope/app.json5'; app=json.loads(path.read_text())
    app['app'].update(versionName='0.22.15',versionCode=221500)
    path.write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n'); changed.append('app/AppScope/app.json5')
    result=dict(version='0.22.15',base='0.22.14',base_lock_sha256=sha(base/'source.lock.json'),
                scope='original UI texture loading, replacement, real log deltas and 2D context preservation; qualification pending',
                files={name:dict(sha256=sha(out/name),bytes=(out/name).stat().st_size) for name in changed},
                full_port_complete=False,functional_completion_percent=None)
    (out/'texture-load-inputs.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); result=prepare(a.base,a.output)
    print(json.dumps(dict(output=str(a.output.resolve()),changed_files=len(result['files']),
                         version=result['version'],full_port_complete=False),indent=2))
if __name__=='__main__':
    main()
