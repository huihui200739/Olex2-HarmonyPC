#!/usr/bin/env python3
"""Freeze a new24 with original resource high-quality resampling; never mutate23."""
import argparse,hashlib,json,shutil
from pathlib import Path
from olex_font_resource_window_validation import validate_source
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected exact23 recipe: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out,fixtures):
 base,out,fixtures=base.resolve(),out.resolve(),fixtures.resolve();validate_source(base)
 app=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if app['versionName']!='0.22.23' or app['versionCode']!=222300:raise ValueError('Requires exact23')
 meta=json.loads((fixtures/'manifest.json').read_text())
 for name,row in meta['files'].items():
  if sha(fixtures/name)!=row['sha256'] or (fixtures/name).stat().st_size!=row['bytes']:raise ValueError('Changed explicit24 synthetic fixture')
 if out.exists():raise FileExistsError(out)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')]);changed=[]
 for name,dest in [('platform_font_factory_resample.cpp','platform_font_factory.cpp'),('platform_font_factory_resample.h','platform_font_factory.h'),('resource_font_resample.cpp','resource_font_resample.cpp'),('resource_font_resample.h','resource_font_resample.h'),('resampled_resource_font_window_probe.cpp','resampled_resource_font_window_probe.cpp'),('resampled_resource_font_window_probe.h','resampled_resource_font_window_probe.h')]:
  path='native/'+dest;shutil.copy2(ROOT/'experiments/gx-font/native'/name,out/path);changed.append(path)
 path='native/olex_font_resample_fixture.h';shutil.copy2(fixtures/'olex_font_resample_fixture.h',out/path);changed.append(path)
 path='native/original_font_window_probe.cpp';text=(out/path).read_text();text=once(text,'#include "original_resource_font_window_probe.h"','#include "original_resource_font_window_probe.h"\n#include "resampled_resource_font_window_probe.h"')
 text=once(text,'<<RunOriginalResourceFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";','<<RunOriginalResourceFontWindowProbe(host,scene,w,h)<<",\\\"resampled_resource\\\":"<<RunResampledResourceFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";');(out/path).write_text(text);changed.append(path)
 path='app/entry/src/main/cpp/CMakeLists.txt';text=(out/path).read_text();text=once(text,'"${OLEX2_ROOT}/native/original_resource_font_window_probe.cpp")','"${OLEX2_ROOT}/native/original_resource_font_window_probe.cpp" "${OLEX2_ROOT}/native/resource_font_resample.cpp" "${OLEX2_ROOT}/native/resampled_resource_font_window_probe.cpp")\nset_source_files_properties("${OLEX2_ROOT}/native/resource_font_resample.cpp" PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")');(out/path).write_text(text);changed.append(path)
 path='app/AppScope/app.json5';app.update(versionName='0.22.24',versionCode=222400);(out/path).write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n');changed.append(path)
 result=dict(version='0.22.24',base='0.22.23',base_manifest_sha256=sha(base/'olex-font-resource-call-inputs.json'),
  scope='Original entry8/72 selection and opaque RGB high-quality resampling; source-model wx3.2.6 strict double; no default fallback, alpha/mask/gamma or complete wx runtime qualification',
  files={p:dict(bytes=(out/p).stat().st_size,sha256=sha(out/p)) for p in changed},fixture_manifest_sha256=sha(fixtures/'manifest.json'),
  wx_image_source_sha256='41c8a0bcad8487b6c1ac0d9758cbd92b3c7b3d72499331de8d4f844cf5afcb39',
  original_font_requests_per_action=15,resample_cases=['small','large','large-scaled','large-restored','fixed-italic','identity'],full_port_complete=False,functional_completion_percent=None)
 (out/'olex-font-resample-inputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('base','output','fixtures'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();d=prepare(a.base,a.output,a.fixtures);print(json.dumps(dict(version=d['version'],files=len(d['files']),output=str(a.output.resolve()),target_executed=False),indent=2))
