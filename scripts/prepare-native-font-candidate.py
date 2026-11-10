#!/usr/bin/env python3
"""Prepare candidate17; genuine native raster cells feed original TGlFont lists."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen candidate16 source: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (identity['versionName'],identity['versionCode'])!=('0.22.16',221600):raise ValueError('Requires frozen candidate16')
 inputs={}
 manifests=['persistent-gx-inputs.json','texture-context-inputs.json','texture-load-inputs.json','texture-load-route-inputs.json']
 for name in manifests:inputs.update(json.loads((base/name).read_text())['files'])
 for name,record in inputs.items():
  path=base/name
  if sha(path)!=record['sha256'] or path.stat().st_size!=record['bytes']:raise ValueError('Frozen input changed: '+name)
 lock=json.loads((base/'source.lock.json').read_text())
 if len(lock['patches'])!=20:raise ValueError('Requires unchanged twenty patches')
 for row in lock['patches']:
  if sha(base/row['file'])!=row['sha256']:raise ValueError('Frozen patch changed')
 for row in lock['patched_files']:
  if sha(base/'upstream'/row['file'])!=row['sha256']:raise ValueError('Frozen upstream changed')
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 changed=[]
 def edit(name,old,new):
  path=out/name;path.write_text(once(path.read_text(),old,new))
  if name not in changed:changed.append(name)
 for name in ('glyph_bitmap.h','original_font_cells.cpp','harmony_glyph_raster.h','harmony_glyph_raster.cpp','platform_font_factory.h','platform_font_factory.cpp','original_font_window_probe.h','original_font_window_probe.cpp'):
  shutil.copy2(ROOT/'experiments/gx-font/native'/name,out/'native'/name);changed.append('native/'+name)
 name='native/platform_window_scene.h'
 edit(name,'#include "glscene.h"','#include "glscene.h"\n#include "platform_font_factory.h"\n#include <cmath>')
 edit(name,'bool bound_=false;', 'bool bound_=false;\n  mutable PlatformFontFactory fonts_;\n  double fontScale_=1;\n  bool scaling_=false;')
 edit(name,'TGlFont& DoCreateFont(TGlFont&,bool) const override {\n    ++lifetime_.fontRequests;\n    throw std::runtime_error("Platform font backend has not been implemented");\n  }',
 '''TGlFont& DoCreateFont(TGlFont& font,bool half) const override {
    ++lifetime_.fontRequests;
    if(!bound_ || !IsEnabled() || !host_.make_current || !host_.make_current())throw std::runtime_error("No current leased context for font creation");
    return fonts_.Create(font,half,fontScale_);
  }''')
 edit(name,'void ScaleFonts(double) override {throw std::runtime_error("Platform font scaling has not been implemented");}\n  void RestoreFontScale() override {throw std::runtime_error("Platform font scaling has not been implemented");}',
 '''void ReleaseFontResources() {fonts_.Release();}
  bool FontsAllReleased()const {return fonts_.AllReleased();}
  const NativeFontRecord& FontRecord(const TGlFont& font)const {return fonts_.Record(font);}
  void ScaleFonts(double scale) override {
    if(!bound_ || !std::isfinite(scale) || scale<=0 || scale>8 || scaling_)throw std::invalid_argument("Unbalanced or invalid platform font scale");
    fonts_.Release();fontScale_=scale;scaling_=true;
  }
  void RestoreFontScale() override {
    if(!scaling_)return;
    if(!bound_)throw std::logic_error("Font scale restoration needs a leased context");
    fonts_.Release();fontScale_=1;scaling_=false;
  }''')
 name='native/persistent_gx_window.cpp'
 edit(name,'#include "original_texture_load_probe.h"','#include "original_texture_load_probe.h"\n#include "original_font_window_probe.h"')
 edit(name,'try{renderer_.BeforeContextChange();}', 'try{renderer_.BeforeContextChange();scene_.ReleaseFontResources();}')
 edit(name,'catch(...){lifetime_.cleanupFailed=true;scene_.Unbind();throw;}', 'catch(...){lifetime_.cleanupFailed=true;try{scene_.ReleaseFontResources();}catch(...){}scene_.Unbind();throw;}')
 edit(name,'    if(h.load_platform_textures){', '''    if(h.draw_platform_fonts){
      const std::string font=RunOriginalFontWindowProbe(h,scene,w,hh);
      lease.Close();CheckGL("original font detach before compatibility shutdown");
      if(!scene.FontsAllReleased() || scene.IsBound() || scene.IsEnabled() || lifetime.cleanupFailed)throw std::runtime_error("Original font resources were not retired");
      out<<"{\\"run\\":"<<h.run<<",\\"width\\":"<<w<<",\\"height\\":"<<hh
         <<",\\"unique_owner\\":"<<(uniqueOwner?"true":"false")<<",\\"owner_address\\":"<<reinterpret_cast<uintptr_t>(&app)
         <<",\\"renderer_address\\":"<<reinterpret_cast<uintptr_t>(&genuine)<<",\\"scene_address\\":"<<reinterpret_cast<uintptr_t>(&scene)
         <<",\\"binding_before\\":"<<bindingBefore<<",\\"binding_after\\":"<<lifetime.bindings
         <<",\\"detachment_before\\":"<<detachmentBefore<<",\\"detachment_after\\":"<<lifetime.detachments
         <<",\\"font_requests\\":"<<lifetime.fontRequests<<",\\"font_resources_retired_before_gl4es_close\\":true,\\"scene_unbound\\":true,\\"scene_disabled\\":true,\\"font\\":"<<font<<",\\"execution_completed\\":true,\\"error\\":\\"\\"}";
      return out.str();
    }
    if(h.load_platform_textures){''')
 edit('native/original_renderer_host.h','bool load_platform_textures=false;', 'bool draw_platform_fonts=false;\n  bool load_platform_textures=false;')
 name='app/entry/src/main/cpp/original_window_route_runs.h'
 shutil.copy2(ROOT/'experiments/gx-font/native/original_window_route_runs_font.h',out/name);changed.append(name)
 name='app/entry/src/main/cpp/shared_original_probe.h';edit(name,'bool loadTextures=false);','bool loadTextures=false,bool fontProbe=false);')
 name='app/entry/src/main/cpp/shared_original_probe.cpp'
 edit(name,'const std::string& directory,bool loadTextures){','const std::string& directory,bool loadTextures,bool fontProbe){')
 edit(name,'run=routeRuns.Next(loadTextures);','run=fontProbe?routeRuns.NextFont():routeRuns.Next(loadTextures);')
 edit(name,'std::string(loadTextures?"original-texture-load-pid-":"original-renderer-pid-")','std::string(fontProbe?"original-font-pid-":loadTextures?"original-texture-load-pid-":"original-renderer-pid-")')
 edit(name,'&completed,loadTextures};','&completed,fontProbe,loadTextures};')
 for file in ('renderer.h','renderer.cpp'):
  name='app/entry/src/main/cpp/'+file
  if file.endswith('.h'):edit(name,'bool loadTextures=false);','bool loadTextures=false,bool fontProbe=false);')
  else:
   edit(name,'const std::string& directory,bool loadTextures){','const std::string& directory,bool loadTextures,bool fontProbe){')
   edit(name,'runtime,directory,loadTextures);','runtime,directory,loadTextures,fontProbe);')
   edit(name,'directory+(loadTextures?"/shared-original-texture-load-run-":"/shared-original-integrated-run-")', 'directory+(fontProbe?"/shared-original-font-run-":loadTextures?"/shared-original-texture-load-run-":"/shared-original-integrated-run-")')
 name='app/entry/src/main/cpp/napi_init.cpp'
 marker='napi_value ProbeOriginalTextures(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);return String(env,Renderer::Instance().ProbeOriginalRenderer(owner.runtime.get(),owner.filesDir,true));}'
 edit(name,marker,marker+'\nnapi_value ProbeOriginalFonts(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);return String(env,Renderer::Instance().ProbeOriginalRenderer(owner.runtime.get(),owner.filesDir,false,true));}')
 marker='    {"runOriginalTextureLoadProbe",nullptr,ProbeOriginalTextures,nullptr,nullptr,nullptr,napi_default,nullptr},'
 edit(name,marker,marker+'\n    {"runOriginalFontProbe",nullptr,ProbeOriginalFonts,nullptr,nullptr,nullptr,napi_default,nullptr},')
 name='app/entry/src/main/cpp/types/libolex2native/index.d.ts'
 edit(name,'export const runOriginalTextureLoadProbe: () => string;','export const runOriginalTextureLoadProbe: () => string;\nexport const runOriginalFontProbe: () => string;')
 name='app/entry/src/main/ets/pages/Index.ets'
 edit(name,"  @State private originalTextureLoadProbe: string = '';","  @State private originalTextureLoadProbe: string = '';\n  @State private originalFontProbe: string = '';")
 marker="      Text(this.stateSaveStatus === 'saved' ?"
 edit(name,marker,'''      Row() {
        Button(this.originalFontProbe.length === 0 ? '测试原版字体' : '再次测试字体')
          .id('olex2-original-font-probe').fontSize(11).height(30).width(124)
          .fontColor('#246B49').backgroundColor('#EDF5EF').borderRadius(7)
          .enabled(this.graphicsPassed()).onClick(() => {
            this.originalFontProbe = nativeBridge.runOriginalFontProbe();
            this.refreshGraphics();
          })
      }.width('100%').padding({ left: 18, right: 18, bottom: 6 })
'''+marker)
 name='app/entry/src/main/cpp/CMakeLists.txt'
 text=(out/name).read_text()+'''\n# NativeDrawing cells are consumed by genuine TGlFont display lists.
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/harmony_glyph_raster.cpp" "${OLEX2_ROOT}/native/original_font_cells.cpp" "${OLEX2_ROOT}/native/platform_font_factory.cpp" "${OLEX2_ROOT}/native/original_font_window_probe.cpp")
target_link_libraries(olex2core_graphics PRIVATE native_drawing)
'''
 (out/name).write_text(text);changed.append(name)
 name='app/AppScope/app.json5';app=json.loads((out/name).read_text());app['app'].update(versionName='0.22.17',versionCode=221700)
 (out/name).write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n');changed.append(name)
 manifest=dict(version='0.22.17',base='0.22.16',base_manifests={name:sha(base/name) for name in manifests},
  scope='native raster cells into genuine original font creation/text/scaling/context release; not yet target-qualified',
  files={name:dict(sha256=sha(out/name),bytes=(out/name).stat().st_size) for name in changed},full_port_complete=False,functional_completion_percent=None)
 (out/'native-font-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output)
 print(json.dumps(dict(output=str(a.output.resolve()),version=d['version'],changed_files=len(d['files']),built=False,target_executed=False,full_port_complete=False),indent=2))
