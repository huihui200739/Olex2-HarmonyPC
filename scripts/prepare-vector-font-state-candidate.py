#!/usr/bin/env python3
"""Add separate real GLES boolean/raw-boolean/integer mask observations; no state writes."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen20 shape: '+old[:100])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 app=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (app['versionName'],app['versionCode'])!=('0.22.20',222000):raise ValueError('Requires exact20')
 files={}
 for name in ('native-font-inputs.json','native-font-layout-inputs.json','native-font-raster-inputs.json','original-vector-font-inputs.json'):files.update(json.loads((base/name).read_text())['files'])
 for name,row in files.items():
  if sha(base/name)!=row['sha256'] or (base/name).stat().st_size!=row['bytes']:raise ValueError('Frozen input changed: '+name)
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 name='app/entry/src/main/cpp/shared_original_probe.cpp';text=(out/name).read_text()
 text=once(text,'GLboolean masks[4]={};glGetBooleanv(GL_COLOR_WRITEMASK,masks);',
  'GLboolean masks[4]={},rawMasks[4]={};GLint integerMasks[4]={};glGetBooleanv(GL_COLOR_WRITEMASK,masks);reinterpret_cast<decltype(&glGetBooleanv)>(Raw("glGetBooleanv"))(GL_COLOR_WRITEMASK,rawMasks);glGetIntegerv(GL_COLOR_WRITEMASK,integerMasks);')
 marker='<<unsigned(masks[3])<<"],\\\"uniforms\\\":[";'
 replacement='<<unsigned(masks[3])<<"],\\\"color_write_raw_boolean\\\":["<<unsigned(rawMasks[0])<<\',\'<<unsigned(rawMasks[1])<<\',\'<<unsigned(rawMasks[2])<<\',\'<<unsigned(rawMasks[3])<<"],\\\"color_write_integer\\\":["<<integerMasks[0]<<\',\'<<integerMasks[1]<<\',\'<<integerMasks[2]<<\',\'<<integerMasks[3]<<"],\\\"uniforms\\\":[";'
 text=once(text,marker,replacement);(out/name).write_text(text)
 app.update(versionName='0.22.21',versionCode=222100);(out/'app/AppScope/app.json5').write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n')
 names=[name,'app/AppScope/app.json5'];d=dict(version='0.22.21',base='0.22.20',base_vector_manifest_sha256=sha(base/'original-vector-font-inputs.json'),scope='Read-only driver mask-query diagnosis; retains raw legacy bool and adds raw-symbol bool + GLint, no graphics writes added',files={p:dict(sha256=sha(out/p),bytes=(out/p).stat().st_size) for p in names},full_port_complete=False,functional_completion_percent=None)
 (out/'vector-font-state-inputs.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n');return d
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output);print(json.dumps(dict(version=d['version'],changed_files=len(d['files']),output=str(a.output.resolve()),target_executed=False),indent=2))
