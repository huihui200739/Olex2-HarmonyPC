#!/usr/bin/env python3
"""Separate route generation counters without altering the frozen source15 package."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def once(text,old,new):
    if text.count(old)!=1:
        raise ValueError('Unexpected source15 route shape: '+old[:110])
    return text.replace(old,new,1)

def prepare(base,out):
    base,out=base.resolve(),out.resolve()
    if out.exists():raise FileExistsError(out)
    identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
    if (identity['versionName'],identity['versionCode'])!=('0.22.15',221500):
        raise ValueError('Requires the frozen source15')
    previous=json.loads((base/'texture-load-inputs.json').read_text())
    for name,record in previous['files'].items():
        path=base/name
        if sha(path)!=record['sha256'] or path.stat().st_size!=record['bytes']:
            raise ValueError('Frozen source15 input changed: '+name)
    shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in
                    ('out','reports','__pycache__','.hvigor','oh_modules')])
    prefix='app/entry/src/main/cpp/'
    header=prefix+'original_window_route_runs.h'
    shutil.copy2(ROOT/'experiments/gx-texture-load/native/original_window_route_runs.h',out/header)
    name=prefix+'shared_original_probe.cpp';path=out/name;text=path.read_text()
    text=once(text,'#include "shared_original_probe.h"','#include "shared_original_probe.h"\n#include "original_window_route_runs.h"')
    text=once(text,'unsigned run=0;int width=0,height=0;void* driver=nullptr;',
              'unsigned run=0;olex2::harmony::OriginalWindowRouteRuns routeRuns;int width=0,height=0;void* driver=nullptr;')
    text=once(text,'SharedOriginalWindowResult output;++run;output.run=run;',
              'SharedOriginalWindowResult output;run=routeRuns.Next(loadTextures);output.run=run;')
    path.write_text(text)
    app_path=out/'app/AppScope/app.json5';app=json.loads(app_path.read_text());app['app'].update(versionName='0.22.16',versionCode=221600)
    app_path.write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n')
    names=[header,name,'app/AppScope/app.json5']
    result=dict(version='0.22.16',base='0.22.15',base_input_manifest_sha256=sha(base/'texture-load-inputs.json'),
                scope='route-local original-window and texture-load generations; prepared only, actual target mixed-route execution pending',
                files={name:dict(sha256=sha(out/name),bytes=(out/name).stat().st_size) for name in names},
                full_port_complete=False,functional_completion_percent=None)
    (out/'texture-load-route-inputs.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=prepare(a.base,a.output)
    print(json.dumps(dict(output=str(a.output.resolve()),version=result['version'],changed_files=len(result['files']),
                         built=False,target_executed=False,full_port_complete=False),indent=2))
