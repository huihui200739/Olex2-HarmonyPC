#!/usr/bin/env python3
import argparse,json,hashlib,re,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--target',required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--run',type=int,required=True);p.add_argument('--folder',required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--hdc',required=True);a=p.parse_args();out=a.report/'runtime'/a.folder;out.mkdir(parents=True,exist_ok=False)
from renderer_probe_identity import package_identity
BUNDLE=package_identity(a.hap)['bundleName'];HDC=a.hdc;remote=f'/data/app/el2/100/base/{BUNDLE}/haps/entry/files'
def call(*args):return subprocess.run([HDC,'-t',a.target,*args],capture_output=True,text=True,check=True,timeout=60).stdout
def shell(s):return call('shell',s)
pid=shell('pidof '+BUNDLE).strip();deadline=time.monotonic()+25
while time.monotonic()<deadline:
 try:
  current=json.loads(shell(remote and 'cat '+remote+f'/shared-original-integrated-run-{a.run}.json'))
  if str(current.get('original',{}).get('pid'))==pid and current.get('run')==a.run:break
 except (ValueError,AttributeError):pass
 time.sleep(.25)
else:raise RuntimeError('No fresh integrated rendering result for current process/run')
artifact=current['original'].get('artifact_subdirectory')
if artifact!=f'original-renderer-pid-{pid}-run-{a.run}':raise RuntimeError('Expected exclusive current-process/run artifact directory')
artifact_remote=remote+'/'+artifact
names=shell(f'ls {artifact_remote}/* {remote}/shared-original-integrated-run-{a.run}.json')
manifest={}
for item in names.splitlines():
 if not (item.startswith(artifact_remote+'/') or item==remote+f'/shared-original-integrated-run-{a.run}.json'):raise RuntimeError('Artifact outside current process/run directory: '+repr(item))
 name=Path(item).name
 if not re.fullmatch(r'[A-Za-z0-9_.-]+',name):raise RuntimeError('Unexpected private result filename')
 expected=shell('sha256sum '+item).split()[0];path=out/name
 if path.exists():
  if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise RuntimeError('Refusing to overwrite earlier evidence '+name)
 else:(out/'transfers.log').open('a').write(call('file','recv',item,str(path)))
 digest=hashlib.sha256(path.read_bytes()).hexdigest()
 if digest!=expected:raise RuntimeError('Remote/local evidence bytes differ '+name)
 manifest[name]={'sha256':digest,'target_sha256':expected,'bytes':path.stat().st_size}
proof=json.loads((out/f'shared-original-integrated-run-{a.run}.json').read_text());pid=shell('pidof '+BUNDLE).strip();
if str(proof['original']['pid'])!=pid:raise ValueError('Process changed while receiving original drawing evidence')
(out/'received-artifacts.json').write_text(json.dumps({'pid':pid,'run':a.run,'artifact_subdirectory':artifact,'files':manifest},indent=2)+'\n');print('Received',len(manifest),'actual artifacts; PID',pid,'limited path:',proof['limited_path_verified'])
