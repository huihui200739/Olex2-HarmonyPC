#!/usr/bin/env python3
"""Candidate18 changes only font-control layout and version, preserving failed17."""
import argparse,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen font layout: '+old[:80])
 return text.replace(old,new,1)
def prepare(base,out):
 base,out=base.resolve(),out.resolve()
 if out.exists():raise FileExistsError(out)
 app=json.loads((base/'app/AppScope/app.json5').read_text())
 if (app['app']['versionName'],app['app']['versionCode'])!=('0.22.17',221700):raise ValueError('Requires frozen17')
 inputs=json.loads((base/'native-font-inputs.json').read_text())
 for name,record in inputs['files'].items():
  path=base/name
  if sha(path)!=record['sha256'] or path.stat().st_size!=record['bytes']:raise ValueError('Frozen17 changed: '+name)
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 name='app/entry/src/main/ets/pages/Index.ets';text=(out/name).read_text()
 start=text.index('      Row() {\n        Button(this.originalFontProbe.length')
 end=text.index("      Text(this.stateSaveStatus === 'saved' ?",start)
 row=text[start:end]
 if row.count("olex2-original-font-probe")!=1:raise ValueError('Unexpected isolated font row')
 text=text[:start]+text[end:]
 button=row.split('      Row() {\n',1)[1].split("      }.width('100%')",1)[0]
 button=once(button,".fontSize(11).height(30).width(124)",".fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })")
 marker="        Button(this.originalTextureLoadProbe.length === 0 ?"
 text=once(text,marker,button+marker)
 text=once(text,".id('olex2-original-texture-load-probe').fontSize(11).height(30).width(108)",".id('olex2-original-texture-load-probe').fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })")
 text=once(text,".fontSize(11).height(30).width(124).padding({ left: 6, right: 6 })",".fontSize(11).height(30).width(92).padding({ left: 6, right: 6 })")
 text=once(text,"Text('上游几何适配视口').fontSize(16)","Text('上游几何适配视口').maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).fontSize(16)")
 text=once(text,"Text('1–4 切换图元 · W 线框 · R 重置 · 方向键 / 滚轮').fontSize(11)","Text('1–4 切换图元 · W 线框 · R 重置 · 方向键 / 滚轮').maxLines(2).textOverflow({ overflow: TextOverflow.Ellipsis }).fontSize(11)")
 (out/name).write_text(text)
 app['app'].update(versionName='0.22.18',versionCode=221800);(out/'app/AppScope/app.json5').write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n')
 names=[name,'app/AppScope/app.json5']
 result=dict(version='0.22.18',base='0.22.17',base_native_font_manifest_sha256=sha(base/'native-font-inputs.json'),scope='repair extra-row clipping without reducing original controls or minimum viewport height',
  files={name:dict(sha256=sha(out/name),bytes=(out/name).stat().st_size) for name in names},full_port_complete=False,functional_completion_percent=None)
 (out/'native-font-layout-inputs.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output)
 print(json.dumps(dict(output=str(a.output.resolve()),version=d['version'],changed_files=len(d['files']),target_executed=False,full_port_complete=False),indent=2))
