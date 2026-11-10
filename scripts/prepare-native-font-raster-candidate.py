#!/usr/bin/env python3
"""Record real matrices for the unchanged integer-raster test; no pixel shifts."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 app=json.loads((base/'app/AppScope/app.json5').read_text())
 if (app['app']['versionName'],app['app']['versionCode'])!=('0.22.18',221800):raise ValueError('Requires frozen18')
 inputs={}
 for manifest in ('native-font-inputs.json','native-font-layout-inputs.json'):inputs.update(json.loads((base/manifest).read_text())['files'])
 for name,row in inputs.items():
  path=base/name
  if sha(path)!=row['sha256'] or path.stat().st_size!=row['bytes']:raise ValueError('Frozen input changed: '+name)
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 name='native/original_font_window_probe.cpp';shutil.copy2(ROOT/'experiments/gx-font/native/original_font_raster_window_probe.cpp',out/name)
 app['app'].update(versionName='0.22.19',versionCode=221900);(out/'app/AppScope/app.json5').write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n')
 names=[name,'app/AppScope/app.json5'];result=dict(version='0.22.19',base='0.22.18',base_layout_manifest_sha256=sha(base/'native-font-layout-inputs.json'),
  scope='retain integer32 original glyph draws; additionally record actual original projection/modelview for independent float32 FMA position analysis',
  files={name:dict(sha256=sha(out/name),bytes=(out/name).stat().st_size) for name in names},full_port_complete=False,functional_completion_percent=None)
 (out/'native-font-raster-inputs.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output)
 print(json.dumps(dict(output=str(a.output.resolve()),version=d['version'],changed_files=len(d['files']),target_executed=False,full_port_complete=False),indent=2))
