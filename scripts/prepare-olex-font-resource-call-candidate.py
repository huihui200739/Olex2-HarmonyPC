#!/usr/bin/env python3
"""Correct original DrawRasterText call in a new23; retain22 failed snapshot."""
import argparse,hashlib,json,shutil
from pathlib import Path
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 app=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (app['versionName'],app['versionCode'])!=('0.22.22',222200):raise ValueError('Requires exact frozen22')
 manifest=json.loads((base/'olex-font-resource-inputs.json').read_text())
 for name,row in manifest['files'].items():
  if sha(base/name)!=row['sha256'] or (base/name).stat().st_size!=row['bytes']:raise ValueError('Frozen22 input changed: '+name)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 path='native/original_resource_font_window_probe.cpp';text=(out/path).read_text()
 old='font.DrawRasterText(vec3d(32,32,0),olxstr("Ab09"),state);'
 if text.count(old)!=1:raise ValueError('Unexpected original22 call shape')
 text=text.replace(old,'glRasterPos2i(32,32);font.DrawRasterText(olxstr("Ab09"),state);',1);(out/path).write_text(text)
 app.update(versionName='0.22.23',versionCode=222300);(out/'app/AppScope/app.json5').write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n')
 names=[path,'app/AppScope/app.json5'];result=dict(version='0.22.23',base='0.22.22',base_manifest_sha256=sha(base/'olex-font-resource-inputs.json'),
  scope='Use actual original two-argument DrawRasterText after unchanged integer raster origin;22 build failure retained',
  files={name:dict(bytes=(out/name).stat().st_size,sha256=sha(out/name)) for name in names},full_port_complete=False,functional_completion_percent=None)
 (out/'olex-font-resource-call-inputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output);print(json.dumps(dict(version=d['version'],files=len(d['files']),output=str(a.output.resolve()),target_executed=False),indent=2))
