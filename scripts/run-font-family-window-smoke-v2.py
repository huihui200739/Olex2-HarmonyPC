#!/usr/bin/env python3
"""Single bounded original-font window run; one PID, three actual contexts."""
import argparse,hashlib,json,os,re,shlex,subprocess,sys,time,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
from layout_validation import attributes,rectangle,controls_unclipped
from prepare_texture_load_support import capture_profile
from font_family_window_validation import IDENTITY,validate
from hdc_install_validation import validate_install_output
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('source','hap'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--target',required=True);p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
 if a.target!='127.0.0.1:5555':raise ValueError('Only original existing emulator accepted')
 hap,source=a.hap.resolve(),a.source.resolve()
 if package_identity(hap)!=IDENTITY:raise ValueError('Only exact26 system family candidate accepted')
 report=ROOT/'reports/emulator'/('font-family-window-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);report.mkdir()
 result=dict(passed=False,identity=IDENTITY,target=a.target,hap_sha256=hashlib.sha256(hap.read_bytes()).hexdigest(),rounds=[],full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,full_ui_qualification=False)
 bundle=IDENTITY['bundleName'];remote='/data/app/el2/100/base/'+bundle+'/haps/entry/files';env=dict(os.environ,PYTHONPATH=str(ROOT/'scripts'))
 def hdc(*args):
  command=[a.hdc,'-t',a.target,*map(str,args)];run=subprocess.run(command,capture_output=True,text=True,timeout=80);text=run.stdout+run.stderr
  with (report/'hdc-commands.log').open('a') as f:f.write(' '.join(shlex.quote(s) for s in command)+'\n'+text+'\n')
  if args and args[0]=='install':validate_install_output(text,run.returncode)
  if run.returncode or any(s.lstrip().startswith('[Fail]') for s in text.splitlines()):raise RuntimeError(text)
  return text
 def shell(command):return hdc('shell',command)
 def invoke(script,*args):
  command=[sys.executable,str(script),*map(str,args)];run=subprocess.run(command,capture_output=True,text=True,timeout=180,env=env)
  with (report/'commands.log').open('a') as f:f.write(' '.join(shlex.quote(s) for s in command)+'\n'+run.stdout+run.stderr+'\n')
  if run.returncode:raise RuntimeError(run.stdout+run.stderr)
 def visible(layout):
  ok,details=controls_unclipped(layout);nodes=list(attributes(layout));panels=[x for x in nodes if x.get('id')=='olex2-viewport-panel'];panel=rectangle(panels[0].get('bounds')) if len(panels)==1 else None
  for identifier in ('olex2-original-texture-load-probe','olex2-original-font-probe'):
   found=[x for x in nodes if x.get('id')==identifier];node=found[0] if len(found)==1 else {};box=rectangle(node.get('bounds'))
   passed=bool(node.get('visible')=='true' and node.get('enabled')=='true' and node.get('type')=='Button' and box and box==rectangle(node.get('origBounds')) and panel and panel[0]<=box[0]<box[2]<=panel[2] and panel[1]<=box[1]<box[3]<=panel[3])
   details[identifier]=dict(passed=passed,bounds=box,original_bounds=rectangle(node.get('origBounds')));ok=ok and passed
  return ok,details
 def receive(number,folder):
  folder.mkdir(parents=True);pid=shell('pidof '+bundle).strip();filename=f'shared-original-font-run-{number}.json';deadline=time.monotonic()+30
  while time.monotonic()<deadline:
   try:
    value=json.loads(shell('cat '+shlex.quote(remote+'/'+filename)))
    if value.get('run')==number and str(value.get('original',{}).get('pid'))==pid:break
   except (ValueError,AttributeError):pass
   time.sleep(.2)
  else:raise RuntimeError('No current PID/font-run result')
  artifact=f'original-font-pid-{pid}-run-{number}'
  if value['original']['artifact_subdirectory']!=artifact:raise ValueError('Unexpected font artifact identity')
  path=remote+'/'+artifact;files={}
  for target in shell('find '+shlex.quote(path)+' -type f').splitlines()+[remote+'/'+filename]:
   name=filename if target==remote+'/'+filename else target.removeprefix(path+'/')
   if not target.startswith(path+'/') and target!=remote+'/'+filename:raise ValueError('Unexpected target path')
   if not re.fullmatch(r'[A-Za-z0-9_./-]+',name) or '..' in Path(name).parts or Path(name).is_absolute():raise ValueError('Unexpected font filename')
   digest=shell('sha256sum '+shlex.quote(target)).split()[0];local=folder/name;local.parent.mkdir(parents=True,exist_ok=True)
   hdc('file','recv',target,local);raw=local.read_bytes()
   if hashlib.sha256(raw).hexdigest()!=digest:raise ValueError('Target/local font bytes differ')
   files[name]=dict(bytes=len(raw),sha256=digest,target_sha256=digest)
  if shell('pidof '+bundle).strip()!=pid:raise ValueError('Process changed during font artifact receipt')
  (folder/'received-artifacts.json').write_text(json.dumps(dict(identity=IDENTITY,pid=int(pid),run=number,hap_sha256=result['hap_sha256'],artifact_subdirectory=artifact,files=files),indent=2)+'\n')
  return value
 try:
  result['device_identity']=validate_pc_emulator_identity(shell(PC_IDENTITY_QUERY));result['build_verification']=verify(hap,hap.with_suffix('.provenance.json'),source)
  (report/'capture-profile.py').write_text(capture_profile(ROOT));hdc('install',hap)
  installed=shell('bm dump -n '+bundle);(report/'installed-bundle.log').write_text(installed);data=json.loads(installed[installed.index('{'):])
  for row in (data,data['applicationInfo']):
   if row['versionName']!=IDENTITY['versionName'] or row['versionCode']!=IDENTITY['versionCode']:raise ValueError('Installed identity differs')
  shell('aa force-stop '+bundle);deadline=time.monotonic()+15
  while shell('pidof '+bundle).strip():
   if time.monotonic()>deadline:raise RuntimeError('Existing owned process did not stop')
   time.sleep(.2)
  shell('aa start -a EntryAbility -b '+bundle);previous=None;pid=None
  for number in range(1,4):
   label='font-'+str(number);common=('--target',a.target,'--report',report,'--hap',hap,'--hdc',a.hdc)
   invoke(report/'capture-profile.py',*common,'--label',label+'-before');before=report/'runtime'/(label+'-before');after=report/'runtime'/(label+'-after')
   ok,details=visible(json.loads((before/'layout.json').read_text()))
   if not ok:raise ValueError('Named original controls are clipped: '+json.dumps(details))
   invoke(ROOT/'scripts/original_renderer_click.py','--target',a.target,'--hdc',a.hdc,'--layout',before/'layout.json','--id','olex2-original-font-probe')
   folder=report/'runtime'/label;observation=receive(number,folder)
   invoke(report/'capture-profile.py',*common,'--label',label+'-after')
   checked=validate(folder,previous,source);before_state=json.loads((before/'native-verification.json').read_text());after_state=json.loads((after/'native-verification.json').read_text())
   checked['layout_before']=details;ok,details=visible(json.loads((after/'layout.json').read_text()));checked['layout_after']=details
   checked['checks'].append(dict(name='all original and new controls remain fully visible',passed=bool(ok)))
   checked['checks'].append(dict(name='saved workspace and original initialized runtime preserved',passed=before_state['workspaceState']==after_state['workspaceState'] and before_state['applicationRuntime']==after_state['applicationRuntime']))
   current=observation['original']['pid']
   if pid is None:pid=current
   checked['checks'].append(dict(name='all font rounds use same actual PID',passed=current==pid))
   checked['passed']=all(c['passed'] for c in checked['checks']);checked['folder']=str(folder.relative_to(ROOT));result['rounds'].append(checked)
   if not checked['passed']:raise ValueError('Independent font pixel/resource verification failed')
   previous=folder
  result['passed']=len(result['rounds'])==3 and all(row['passed'] for row in result['rounds'])
 except Exception as error:result.update(passed=False,error=type(error).__name__+': '+str(error))
 finally:
  for name in ('run-font-family-window-smoke-v2.py','hdc_install_validation.py','font_family_window_validation.py','font_family_reference.py','font_raster_flush_window_validation.py','olex_font_resample_window_validation.py','resource_font_resample_reference.py','olex_font_resource_window_validation.py','olex_font_resource_fixtures.py','run-olex-font-resource-window-smoke.py','vector_font_state_window_validation.py','original_vector_font_window_validation.py','native_font_raster_window_validation.py','native_font_window_validation.py','run-vector-font-state-window-smoke.py','prepare_texture_load_support.py'):
   (report/name).write_bytes((ROOT/'scripts'/name).read_bytes())
  (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(passed=result['passed'],rounds=len(result['rounds']),report_directory=str(report),error=result.get('error')),ensure_ascii=False,indent=2));return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
