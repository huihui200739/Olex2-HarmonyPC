#!/usr/bin/env python3
"""Freeze a new26 exact system-family/style backend; do not modify25 or its evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from font_raster_flush_window_validation import validate_source
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Changed frozen25 recipe: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve();validate_source(base)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if identity['versionName']!='0.22.25' or identity['versionCode']!=222500:raise ValueError('Requires exact frozen25')
 if not out.is_relative_to(ROOT/'out/gx-application-evaluation'):raise ValueError('Keep26 candidate in this project')
 if out.exists():raise FileExistsError(out)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 names=[]
 for source,dest in [('platform_font_factory_family.cpp','platform_font_factory.cpp'),('platform_font_factory_family.h','platform_font_factory.h'),('family_font_description.h','family_font_description.h'),('system_font_catalog.cpp','system_font_catalog.cpp'),('system_font_catalog.h','system_font_catalog.h'),('family_font_window_probe.cpp','family_font_window_probe.cpp'),('family_font_window_probe.h','family_font_window_probe.h')]:
  path='native/'+dest;shutil.copy2(ROOT/'experiments/gx-font-family/native'/source,out/path);names.append(path)
 path='native/platform_window_scene.h';text=(out/path).read_text();text=once(text,'  bool FontsAllReleased()const {return fonts_.AllReleased();}','  bool FontsAllReleased()const {return fonts_.AllReleased();}\n  const SystemFontCatalog& FontCatalog()const {return fonts_.FontCatalog();}');(out/path).write_text(text);names.append(path)
 path='native/original_font_window_probe.cpp';text=(out/path).read_text();text=once(text,'#include "resampled_resource_font_window_probe.h"','#include "resampled_resource_font_window_probe.h"\n#include "family_font_window_probe.h"')
 text=once(text,'<<RunResampledResourceFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";','<<RunResampledResourceFontWindowProbe(host,scene,w,h)<<",\\\"system_family\\\":"<<RunFamilyFontWindowProbe(host,scene,w,h)<<",\\\"execution_completed\\\":true}";');(out/path).write_text(text);names.append(path)
 path='app/entry/src/main/cpp/CMakeLists.txt';text=(out/path).read_text();text=once(text,'"${OLEX2_ROOT}/native/resampled_resource_font_window_probe.cpp")','"${OLEX2_ROOT}/native/resampled_resource_font_window_probe.cpp" "${OLEX2_ROOT}/native/system_font_catalog.cpp" "${OLEX2_ROOT}/native/family_font_window_probe.cpp")');(out/path).write_text(text);names.append(path)
 path='app/AppScope/app.json5';identity.update(versionName='0.22.26',versionCode=222600);(out/path).write_text(json.dumps(dict(app=identity),ensure_ascii=False,indent=2)+'\n');names.append(path)
 result=dict(version='0.22.26',base='0.22.25',base_manifest_sha256=sha(base/'font-raster-flush-inputs.json'),scope='Exact installed system family/name/raw SDK style selection; five real original256 RGB glyph stages; no default/closest-style or synthetic-style fallback; dialog/actual DPI/full fonts remain incomplete',original_font_requests_per_action=20,system_family_cases=['first','scaled','restored','other-family','other-style'],files={name:dict(bytes=(out/name).stat().st_size,sha256=sha(out/name)) for name in names},recipe=dict(path='scripts/prepare-font-family-candidate.py',sha256=sha(Path(__file__))),full_port_complete=False,functional_completion_percent=None)
 (out/'font-family-inputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=prepare(a.base,a.output);print(json.dumps(dict(version=r['version'],files=len(r['files']),output=str(a.output.resolve()),device_executed=False),indent=2))
