#!/usr/bin/env python3
"""Add genuine original @/Hershey font creation to a new frozen19-derived candidate."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parents[1]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen19 shape: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (identity['versionName'],identity['versionCode'])!=('0.22.19',221900):raise ValueError('Requires exact frozen19')
 inputs={}
 for name in ('persistent-gx-inputs.json','texture-context-inputs.json','texture-load-inputs.json','texture-load-route-inputs.json','native-font-inputs.json','native-font-layout-inputs.json','native-font-raster-inputs.json'):
  inputs.update(json.loads((base/name).read_text())['files'])
 for name,row in inputs.items():
  if sha(base/name)!=row['sha256'] or (base/name).stat().st_size!=row['bytes']:raise ValueError('Frozen base changed: '+name)
 lock=json.loads((base/'source.lock.json').read_text())
 if len(lock['patches'])!=20:raise ValueError('Keep the existing twenty patches')
 for row in lock['patches']:
  if sha(base/row['file'])!=row['sha256']:raise ValueError('Frozen patch changed')
 for row in lock['patched_files']:
  if sha(base/'upstream'/row['file'])!=row['sha256']:raise ValueError('Frozen upstream changed')
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 files=[]
 for source,destination in (('platform_font_factory_vector.cpp','platform_font_factory.cpp'),('platform_font_factory_vector.h','platform_font_factory.h'),('font_description.h','font_description.h'),('original_vector_font_window_probe.cpp','original_vector_font_window_probe.cpp'),('original_vector_font_window_probe.h','original_vector_font_window_probe.h')):
  path='native/'+destination;shutil.copy2(ROOT/'experiments/gx-font/native'/source,out/path);files.append(path)
 path='native/original_font_window_probe.cpp';text=(out/path).read_text()
 text=once(text,'#include "platform_font_factory.h"','#include "platform_font_factory.h"\n#include "original_vector_font_window_probe.h"')
 text=once(text,'out<<"],\\\"font_object_preserved\\\":"<<(&font==&restored?"true":"false")<<",\\\"execution_completed\\\":true}";',
                'out<<"],\\\"font_object_preserved\\\":"<<(&font==&restored?"true":"false")<<",\\\"vector\\\":"<<RunOriginalVectorFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";')
 (out/path).write_text(text);files.append(path)
 path='app/entry/src/main/cpp/CMakeLists.txt';text=(out/path).read_text();text=once(text,'"${OLEX2_ROOT}/native/original_font_window_probe.cpp")','"${OLEX2_ROOT}/native/original_font_window_probe.cpp" "${OLEX2_ROOT}/native/original_vector_font_window_probe.cpp")');(out/path).write_text(text);files.append(path)
 path='app/AppScope/app.json5';identity.update(versionName='0.22.20',versionCode=222000);(out/path).write_text(json.dumps(dict(app=identity),ensure_ascii=False,indent=2)+'\n');files.append(path)
 manifest=dict(version='0.22.20',base='0.22.19',base_manifest_sha256=sha(base/'native-font-raster-inputs.json'),
  scope='Genuine original CreateHershey @ fonts, original vector scale semantics, retained native raster stages; direct-table GL reference explicitly shares GPU backend',
  files={p:dict(sha256=sha(out/p),bytes=(out/p).stat().st_size) for p in files},
  original_vector_source=dict(path='upstream/glib/glfont.cpp',sha256=sha(out/'upstream/glib/glfont.cpp')),
  full_port_complete=False,functional_completion_percent=None)
 (out/'original-vector-font-inputs.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output);print(json.dumps(dict(version=d['version'],output=str(a.output.resolve()),changed_files=len(d['files']),target_executed=False,full_port_complete=False),indent=2))
