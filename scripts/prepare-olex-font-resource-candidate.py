#!/usr/bin/env python3
"""Exact21-derived original resource bitmap fonts; no change to frozen predecessors."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen21 input: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out,fixtures):
 base,out,fixtures=base.resolve(),out.resolve(),fixtures.resolve()
 if out.exists():raise FileExistsError(out)
 app=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (app['versionName'],app['versionCode'])!=('0.22.21',222100):raise ValueError('Requires exact frozen21')
 files={}
 for name in ('persistent-gx-inputs.json','texture-context-inputs.json','texture-load-inputs.json','texture-load-route-inputs.json','native-font-inputs.json','native-font-layout-inputs.json','native-font-raster-inputs.json','original-vector-font-inputs.json','vector-font-state-inputs.json'):
  files.update(json.loads((base/name).read_text())['files'])
 for name,row in files.items():
  if sha(base/name)!=row['sha256'] or (base/name).stat().st_size!=row['bytes']:raise ValueError('Frozen21 source changed: '+name)
 meta=json.loads((fixtures/'manifest.json').read_text())
 for name,row in meta['files'].items():
  if sha(fixtures/name)!=row['sha256'] or (fixtures/name).stat().st_size!=row['bytes']:raise ValueError('Synthetic font fixture changed: '+name)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 changed=[]
 for source,destination in (('olex_font_resource.cpp','olex_font_resource.cpp'),('olex_font_resource.h','olex_font_resource.h'),('platform_font_factory_resource.cpp','platform_font_factory.cpp'),('platform_font_factory_resource.h','platform_font_factory.h'),('original_resource_font_window_probe.cpp','original_resource_font_window_probe.cpp'),('original_resource_font_window_probe.h','original_resource_font_window_probe.h')):
  path='native/'+destination;shutil.copy2(ROOT/'experiments/gx-font/native'/source,out/path);changed.append(path)
 path='native/olex_font_resource_fixture.h';shutil.copy2(fixtures/'olex_font_resource_fixture.h',out/path);changed.append(path)
 path='native/original_font_window_probe.cpp';text=(out/path).read_text()
 text=once(text,'#include "original_vector_font_window_probe.h"','#include "original_vector_font_window_probe.h"\n#include "original_resource_font_window_probe.h"')
 old='<<RunOriginalVectorFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";'
 new='<<RunOriginalVectorFontWindowProbe(host,scene,w,h)<<",\\\"resource\\\":"<<RunOriginalResourceFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";'
 text=once(text,old,new);(out/path).write_text(text);changed.append(path)
 path='app/entry/src/main/cpp/CMakeLists.txt';text=(out/path).read_text();text=once(text,'"${OLEX2_ROOT}/native/original_vector_font_window_probe.cpp")','"${OLEX2_ROOT}/native/original_vector_font_window_probe.cpp" "${OLEX2_ROOT}/native/olex_font_resource.cpp" "${OLEX2_ROOT}/native/original_resource_font_window_probe.cpp")');(out/path).write_text(text);changed.append(path)
 path='app/AppScope/app.json5';app.update(versionName='0.22.22',versionCode=222200);(out/path).write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n');changed.append(path)
 result=dict(version='0.22.22',base='0.22.21',base_manifest_sha256=sha(base/'vector-font-state-inputs.json'),
  scope='Original #file:style-size ofnt ZIP/bit-array via genuine CreateGlyphs; explicit exact8..28 entry profile, legacy wx resampling still unavailable; retained native/vector stages',
  files={p:dict(bytes=(out/p).stat().st_size,sha256=sha(out/p)) for p in changed},fixture_manifest_sha256=sha(fixtures/'manifest.json'),
  resource_semantics_sources={name:sha(out/name) for name in ('upstream/gxlib/wxglscene.cpp','upstream/glib/glscene.cpp','upstream/glib/glfont.cpp','upstream/sdl/bitarray.h')},
  original_font_requests_per_action=9,full_port_complete=False,functional_completion_percent=None)
 (out/'olex-font-resource-inputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output,a.fixtures);print(json.dumps(dict(version=d['version'],files=len(d['files']),output=str(a.output.resolve()),target_executed=False),indent=2))
