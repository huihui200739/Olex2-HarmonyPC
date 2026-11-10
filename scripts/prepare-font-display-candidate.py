#!/usr/bin/env python3
"""Freeze new27 actual main-window DPI wiring without changing source16..26."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from font_family_window_validation import validate_source
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Frozen26 anchor changed: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve();validate_source(base)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if identity['versionName']!='0.22.26' or identity['versionCode']!=222600:raise ValueError('Requires frozen26')
 if not out.is_relative_to(ROOT/'out/gx-application-evaluation') or out==ROOT/'out/gx-application-evaluation':raise ValueError('Keep candidate in owned evaluation directory')
 if out.exists():raise FileExistsError(out)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 names=[]
 def save(name,text):
  (out/name).write_text(text)
  if name not in names:names.append(name)
 def edit(name,old,new):save(name,once((out/name).read_text(),old,new))
 def copied(source,dest):
  (out/dest).parent.mkdir(parents=True,exist_ok=True);(out/dest).write_bytes((ROOT/source).read_bytes());names.append(dest)
 copied('experiments/gx-font-display/native/display_font_metrics.h','native/display_font_metrics.h')
 for name in ('FontDisplayMetrics.ts','MainWindowFontMetrics.ets'):
  copied('experiments/gx-font-display/model/'+name,'app/entry/src/main/ets/model/'+name)
 edit('native/original_renderer_host.h','#include <stddef.h>','#include <stddef.h>\n#include "display_font_metrics.h"')
 edit('native/original_renderer_host.h','  bool load_platform_textures=false; // private same-build route, default old diagnostics','  bool load_platform_textures=false; // private same-build route, default old diagnostics\n  olex2::harmony::FontDisplayMetrics font_display_metrics;')
 edit('native/harmony_glyph_raster.h','  OH_Drawing_Typeface* typeface=nullptr;','  OH_Drawing_Typeface* typeface=nullptr;\n  float scaleX=1.0f; // Physical dpiX/dpiY; never virtual densityPixels.')
 edit('native/harmony_glyph_raster.cpp','    auto* face=spec.typeface;','    if(!std::isfinite(spec.scaleX)||spec.scaleX<0.0625f||spec.scaleX>16.0f)\n      throw std::invalid_argument("Invalid physical glyph horizontal scale");\n    auto* face=spec.typeface;')
 edit('native/harmony_glyph_raster.cpp','    OH_Drawing_FontSetTextSize(font.get(),spec.pixelSize);','    OH_Drawing_FontSetTextSize(font.get(),spec.pixelSize);\n    OH_Drawing_FontSetScaleX(font.get(),spec.scaleX);\n    Require(OH_Drawing_FontGetTextSize(font.get())==spec.pixelSize&&OH_Drawing_FontGetScaleX(font.get())==spec.scaleX,\n      "Native font did not retain actual-display pixel size/scale");')
 edit('native/platform_font_factory.h','#include "system_font_catalog.h"','#include "system_font_catalog.h"\n#include "display_font_metrics.h"')
 edit('native/platform_font_factory.h','// ohos-native:<point-size>:<r|b|i|bi>[:fixed]. Raster size uses 96 DPI.','// ohos-native:<point-size>:<r|b|i|bi>[:fixed]. Native/family rasters use\n// actual main-window xDPI/yDPI; original bitmap/vector semantics stay unchanged.')
 edit('native/platform_font_factory.h','  unsigned resourceEntryPoints=0;','  FontDisplayMetrics displayMetrics;\n  float rasterPixelSize=0,rasterScaleX=0;\n  unsigned resourceEntryPoints=0;')
 edit('native/platform_font_factory.h','  mutable std::unique_ptr<SystemFontCatalog> catalog_;','  mutable std::unique_ptr<SystemFontCatalog> catalog_;\n  FontDisplayMetrics displayMetrics_;')
 edit('native/platform_font_factory.h','  TGlFont& Create(TGlFont&,bool half,double scale) const;','  void SetDisplayMetrics(const FontDisplayMetrics& value) {\n    if(!AllReleased())throw std::logic_error("Display transition requires retired original glyph lists");\n    if(value.ready)RequireFontDisplayMetrics(value);\n    displayMetrics_=value;\n  }\n  const FontDisplayMetrics& DisplayMetrics()const {return displayMetrics_;}\n  TGlFont& Create(TGlFont&,bool half,double scale) const;')
 edit('native/platform_font_factory.cpp','  if(record->description!=description || record->effectivePoints!=effective || record->half!=half) {','  const auto rasterParameters=RasterForDisplay(effective,displayMetrics_);\n  if(record->description!=description || record->effectivePoints!=effective || record->half!=half ||\n     !SameFontDisplayMetrics(record->displayMetrics,displayMetrics_)) {')
 edit('native/platform_font_factory.cpp','    HarmonyGlyphRasterizer raster({static_cast<float>(effective*96.0/72.0),\n      family?false:spec.bold,family?false:spec.italic,selected.typeface.get()});','    HarmonyGlyphRasterizer raster({rasterParameters.pixelSize,\n      family?false:spec.bold,family?false:spec.italic,selected.typeface.get(),rasterParameters.scaleX});')
 edit('native/platform_font_factory.cpp','    ++record->rasterizations;','    record->displayMetrics=displayMetrics_;record->rasterPixelSize=rasterParameters.pixelSize;record->rasterScaleX=rasterParameters.scaleX;\n    ++record->rasterizations;')
 edit('native/platform_window_scene.h','    host_=host;bound_=true;SetEnabled(true);++lifetime_.bindings;','    fonts_.SetDisplayMetrics(host.font_display_metrics);\n    host_=host;bound_=true;SetEnabled(true);++lifetime_.bindings;')
 edit('native/platform_window_scene.h','  const SystemFontCatalog& FontCatalog()const {return fonts_.FontCatalog();}','  const SystemFontCatalog& FontCatalog()const {return fonts_.FontCatalog();}\n  const FontDisplayMetrics& DisplayMetrics()const {return fonts_.DisplayMetrics();}')
 for name in ('original_font_window_probe.cpp','family_font_window_probe.cpp'):
  edit('native/'+name,'<<",\\"cell_width\\":"<<cells.width','<<",\\"display_metrics\\":"<<FontDisplayMetricsJson(record.displayMetrics)\n     <<",\\"raster_pixel_size\\":"<<std::setprecision(9)<<record.rasterPixelSize<<",\\"raster_scale_x\\":"<<record.rasterScaleX\n     <<",\\"cell_width\\":"<<cells.width')
 edit('native/original_font_window_probe.cpp','std::ostringstream out;out<<"{\\"native_rasterizer\\":\\"OH_Drawing\\",','std::ostringstream out;out<<"{\\"display_metrics\\":"<<FontDisplayMetricsJson(scene.DisplayMetrics())<<",\\"native_rasterizer\\":\\"OH_Drawing\\",')
 edit('app/entry/src/main/cpp/shared_original_probe.h','#include <string>','#include <string>\n#include "display_font_metrics.h"')
 edit('app/entry/src/main/cpp/shared_original_probe.h','bool loadTextures=false,bool fontProbe=false);','bool loadTextures=false,bool fontProbe=false,const olex2::harmony::FontDisplayMetrics& fontDisplay={});')
 edit('app/entry/src/main/cpp/shared_original_probe.cpp','const std::string& directory,bool loadTextures,bool fontProbe){','const std::string& directory,bool loadTextures,bool fontProbe,const olex2::harmony::FontDisplayMetrics& fontDisplay){')
 edit('app/entry/src/main/cpp/shared_original_probe.cpp','fontProbe,loadTextures};legacyReport=','fontProbe,loadTextures};host.font_display_metrics=fontDisplay;legacyReport=')
 edit('app/entry/src/main/cpp/renderer.h','#include <mutex>','#include <mutex>\n#include "display_font_metrics.h"')
 edit('app/entry/src/main/cpp/renderer.h','  GraphicsStatus Status();','  GraphicsStatus Status();\n  olex2::harmony::FontDisplayMetrics SetFontDisplayMetrics(double id,double x,double y,double density);\n  olex2::harmony::FontDisplayMetrics ClearFontDisplayMetrics();\n  olex2::harmony::FontDisplayMetrics GetFontDisplayMetrics();')
 edit('app/entry/src/main/cpp/renderer.h','  GraphicsStatus status_;','  GraphicsStatus status_;\n  olex2::harmony::FontDisplayMetricsStore fontDisplay_;')
 edit('app/entry/src/main/cpp/renderer.cpp','std::string Renderer::ProbeOriginalRenderer(','''olex2::harmony::FontDisplayMetrics Renderer::SetFontDisplayMetrics(double id,double x,double y,double density){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Set(id,x,y,density);
}
olex2::harmony::FontDisplayMetrics Renderer::ClearFontDisplayMetrics(){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Clear();
}
olex2::harmony::FontDisplayMetrics Renderer::GetFontDisplayMetrics(){
 std::lock_guard<std::mutex> lock(mutex_);return fontDisplay_.Get();
}
std::string Renderer::ProbeOriginalRenderer(''')
 edit('app/entry/src/main/cpp/renderer.cpp',' const EGLDisplay oldDisplay=eglGetCurrentDisplay();const EGLSurface oldDraw=',' if(fontProbe&&!fontDisplay_.Get().ready)return "{\\"limited_path_verified\\":false,\\"error\\":\\"Actual main-window display DPI is unavailable; no fixed96 fallback\\"}";\n const EGLDisplay oldDisplay=eglGetCurrentDisplay();const EGLSurface oldDraw=')
 edit('app/entry/src/main/cpp/renderer.cpp','status_.height,runtime,directory,loadTextures,fontProbe);','status_.height,runtime,directory,loadTextures,fontProbe,fontDisplay_.Get());')
 napi='''napi_value FontDisplayObject(napi_env env,const olex2::harmony::FontDisplayMetrics& value){
 napi_value out;napi_create_object(env,&out);Boolean(env,out,"ready",value.ready);
 Number(env,out,"displayId",static_cast<double>(value.displayId));Number(env,out,"revision",static_cast<double>(value.revision));
 Number(env,out,"dpiX",value.dpiX);Number(env,out,"dpiY",value.dpiY);Number(env,out,"densityPixels",value.densityPixels);
 Boolean(env,out,"fixedDpiFallback",false);return out;
}
napi_value SetFontDisplayMetrics(napi_env env,napi_callback_info info){
 size_t count=5;napi_value args[5];double values[4];
 if(napi_get_cb_info(env,info,&count,args,nullptr,nullptr)!=napi_ok||count!=4){napi_throw_type_error(env,nullptr,"Expected observed displayId/xDPI/yDPI/densityPixels");return nullptr;}
 for(size_t i=0;i<4;++i)if(napi_get_value_double(env,args[i],&values[i])!=napi_ok){napi_throw_type_error(env,nullptr,"Display observations must be numbers");return nullptr;}
 try{return FontDisplayObject(env,Renderer::Instance().SetFontDisplayMetrics(values[0],values[1],values[2],values[3]));}
 catch(const std::exception& error){napi_throw_error(env,nullptr,error.what());return nullptr;}
}
napi_value ClearFontDisplayMetrics(napi_env env,napi_callback_info){
 try{return FontDisplayObject(env,Renderer::Instance().ClearFontDisplayMetrics());}
 catch(const std::exception& error){napi_throw_error(env,nullptr,error.what());return nullptr;}
}
napi_value GetFontDisplayMetrics(napi_env env,napi_callback_info){return FontDisplayObject(env,Renderer::Instance().GetFontDisplayMetrics());}
'''
 edit('app/entry/src/main/cpp/napi_init.cpp','napi_value ProbeOriginalRenderer(napi_env env,napi_callback_info)',napi+'napi_value ProbeOriginalRenderer(napi_env env,napi_callback_info)')
 edit('app/entry/src/main/cpp/napi_init.cpp','    {"runOriginalFontProbe",nullptr,ProbeOriginalFonts,nullptr,nullptr,nullptr,napi_default,nullptr},','    {"runOriginalFontProbe",nullptr,ProbeOriginalFonts,nullptr,nullptr,nullptr,napi_default,nullptr},\n    {"setFontDisplayMetrics",nullptr,SetFontDisplayMetrics,nullptr,nullptr,nullptr,napi_default,nullptr},\n    {"clearFontDisplayMetrics",nullptr,ClearFontDisplayMetrics,nullptr,nullptr,nullptr,napi_default,nullptr},\n    {"getFontDisplayMetrics",nullptr,GetFontDisplayMetrics,nullptr,nullptr,nullptr,napi_default,nullptr},')
 edit('app/entry/src/main/cpp/types/libolex2native/index.d.ts','export const runOriginalFontProbe: () => string;','''export const runOriginalFontProbe: () => string;
export interface FontDisplayMetricsStatus {
  ready: boolean; displayId: number; revision: number; dpiX: number; dpiY: number;
  densityPixels: number; fixedDpiFallback: boolean;
}
export const setFontDisplayMetrics: (displayId: number, dpiX: number, dpiY: number, densityPixels: number) => FontDisplayMetricsStatus;
export const clearFontDisplayMetrics: () => FontDisplayMetricsStatus;
export const getFontDisplayMetrics: () => FontDisplayMetricsStatus;''')
 edit('app/entry/src/main/ets/entryability/EntryAbility.ets',"import nativeBridge from 'libolex2native.so';","import nativeBridge from 'libolex2native.so';\nimport { MainWindowFontMetrics } from '../model/MainWindowFontMetrics';")
 edit('app/entry/src/main/ets/entryability/EntryAbility.ets','  onDestroy(): void { nativeBridge.shutdownApplicationRuntime(); }','  private fontMetrics: MainWindowFontMetrics = new MainWindowFontMetrics();\n  onWindowStageDestroy(): void { this.fontMetrics.stop(); }\n  onDestroy(): void { this.fontMetrics.stop(); nativeBridge.shutdownApplicationRuntime(); }')
 edit('app/entry/src/main/ets/entryability/EntryAbility.ets',"    stage.loadContent('pages/Index', (error) => {","    this.fontMetrics.start(stage);\n    stage.loadContent('pages/Index', (error) => {")
 edit('app/entry/src/main/ets/pages/Index.ets',"  @State private originalFontProbe: string = '';","  @State private originalFontProbe: string = '';\n  @StorageLink('olex2FontMetricsReady') private fontMetricsReady: boolean = false;\n  @StorageLink('olex2FontMetricsError') private fontMetricsError: string = '';")
 edit('app/entry/src/main/ets/pages/Index.ets',"        Button(this.originalFontProbe.length === 0 ? '测试原版字体' : '再次测试字体')","        Button(this.fontMetricsReady ? (this.originalFontProbe.length === 0 ? '测试原版字体' : '再次测试字体') : '字体参数未就绪')")
 edit('app/entry/src/main/ets/pages/Index.ets',"          .enabled(this.graphicsPassed()).onClick(() => {\n            this.originalFontProbe = nativeBridge.runOriginalFontProbe();","          .accessibilityDescription(this.fontMetricsReady ? '使用当前窗口实际屏幕DPI' : this.fontMetricsError)\n          .enabled(this.graphicsPassed() && this.fontMetricsReady).onClick(() => {\n            this.originalFontProbe = nativeBridge.runOriginalFontProbe();")
 identity.update(versionName='0.22.27',versionCode=222700);save('app/AppScope/app.json5',json.dumps(dict(app=identity),ensure_ascii=False,indent=2)+'\n')
 meta=dict(version='0.22.27',base='0.22.26',base_manifest_sha256=sha(base/'font-family-inputs.json'),scope='Actual own-main-window display xDPI/yDPI for native/exact-family point fonts; monitor/DPI events, invalidation, cache identity and physical horizontal scale; original bitmap/vector semantics unchanged; no fixed96/default-monitor fallback',original_font_requests_per_action=20,device_executed=False,files={name:dict(bytes=(out/name).stat().st_size,sha256=sha(out/name)) for name in names},recipe=dict(path='scripts/prepare-font-display-candidate.py',sha256=sha(Path(__file__))),full_port_complete=False,functional_completion_percent=None)
 (out/'font-display-inputs.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n');return meta
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();m=prepare(a.base,a.output);print(json.dumps(dict(version=m['version'],files=len(m['files']),output=str(a.output.resolve()),device_executed=False),indent=2))
