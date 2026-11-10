#!/usr/bin/env python3
"""New28 corrects stale26 diagnostic DPI labels and exposes actual observed metrics."""
import argparse,hashlib,json,shutil
from pathlib import Path
from font_display_source_validation import validate_source27,FILES28
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Frozen27 anchor changed: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base=validate_source27(base);out=out.resolve()
 if not out.is_relative_to(ROOT/'out/gx-application-evaluation') or out==ROOT/'out/gx-application-evaluation':raise ValueError('Keep new28 inside owned evaluation directory')
 if out.exists():raise FileExistsError(out)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 def edit(name,old,new):(out/name).write_text(once((out/name).read_text(),old,new))
 p=out/'native/family_font_window_probe.cpp';text=p.read_text()
 old='\\"point_dpi\\":96,\\"actual_display_DPI_qualified\\":false,'
 new='\\"physical_point_dpi\\":["<<std::setprecision(17)<<scene.DisplayMetrics().dpiX<<","<<scene.DisplayMetrics().dpiY<<"],\\"actual_display_DPI_observed\\":"<<(scene.DisplayMetrics().ready?"true":"false")<<",\\"actual_display_DPI_qualified\\":false,'
 p.write_text(once(text,old,new))
 # The existing UI capture serializes getGraphicsStatus(). A native getter in
 # that actual snapshot binds window before/after metrics to font case records.
 name='app/entry/src/main/cpp/napi_init.cpp';text=(out/name).read_text()
 text=once(text,'napi_value Graphics(napi_env env,napi_callback_info) {','napi_value FontDisplayObject(napi_env,const olex2::harmony::FontDisplayMetrics&);\nnapi_value Graphics(napi_env env,napi_callback_info) {')
 start=text.index('napi_value Graphics(napi_env env,napi_callback_info) {');end=text.index('\nnapi_value ',start+1)
 body=text[start:end];body=once(body,' return out;','\n  Property(env,out,"fontDisplay",FontDisplayObject(env,Renderer::Instance().GetFontDisplayMetrics()));\n  return out;');text=text[:start]+body+text[end:];(out/name).write_text(text)
 name='app/entry/src/main/cpp/types/libolex2native/index.d.ts';text=(out/name).read_text();text=once(text,'export interface GraphicsStatus {','export interface GraphicsStatus {\n  fontDisplay?: FontDisplayMetricsStatus;');(out/name).write_text(text)
 name='app/AppScope/app.json5';app=json.loads((out/name).read_text())['app'];app.update(versionName='0.22.28',versionCode=222800);(out/name).write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n')
 meta=dict(version='0.22.28',base='0.22.27',base_manifest_sha256=sha(base/'font-display-inputs.json'),scope='Correct physical point DPI telemetry and include actual main-window native display metrics in existing GraphicsStatus snapshots; no inherited device qualification',original_font_requests_per_action=20,files={name:dict(bytes=(out/name).stat().st_size,sha256=sha(out/name)) for name in sorted(FILES28)},recipe=dict(path='scripts/prepare-font-display-telemetry-candidate.py',sha256=sha(Path(__file__))),device_executed=False,full_port_complete=False,functional_completion_percent=None)
 (out/'font-display-telemetry-inputs.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n');return meta
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();meta=prepare(a.base,a.output);print(json.dumps(dict(version=meta['version'],files=len(meta['files']),output=str(a.output.resolve()),device_executed=False),indent=2))
