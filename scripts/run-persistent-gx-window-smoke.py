#!/usr/bin/env python3
"""Source-bound partial integration gate, not a replacement for the full UI suite."""
import argparse,datetime,hashlib,json,os,shlex,shutil,subprocess,sys,time,uuid
from pathlib import Path
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from persistent_gx_window_validation import validate,replace_once,original_profile
from texture_1d_window_validation import validate as validate_texture
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
identity=package_identity(a.hap)
if (identity['versionName'],identity['versionCode']) not in (('0.22.12',221200),('0.22.13',221300)):raise ValueError('Only explicitly profiled persistent GX candidates are accepted')
report=ROOT/'reports/emulator'/('persistent-gx-window-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);report.mkdir(parents=True)
result=dict(passed=False,full_port_complete=False,functional_completion_percent=None,report_directory=str(report),identity=identity,hap_sha256=hashlib.sha256(a.hap.read_bytes()).hexdigest(),rounds=[],full_ui_qualification=False,target=a.target)
source=a.source.resolve();hap=a.hap.resolve();bundle=identity['bundleName'];hdc=lambda *args:subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=80,check=True).stdout
shell=lambda cmd:hdc('shell',cmd)
env=dict(os.environ,PYTHONPATH=str(ROOT/'scripts'))
def invoke(script,*args):
 cmd=[sys.executable,str(script),*map(str,args)]
 outcome=subprocess.run(cmd,capture_output=True,text=True,timeout=160,env=env)
 (report/'commands.log').open('a').write(' '.join(shlex.quote(x) for x in cmd)+'\n'+outcome.stdout+outcome.stderr+'\n')
 if outcome.returncode:raise RuntimeError(str(script)+': '+outcome.stdout+outcome.stderr)
def start():
 old=shell('pidof '+shlex.quote(bundle)).strip()
 shell('aa force-stop '+shlex.quote(bundle))
 deadline=time.monotonic()+15
 while old and time.monotonic()<deadline:
  if not shell('pidof '+shlex.quote(bundle)).strip():break
  time.sleep(.2)
 else:
  if old:raise RuntimeError('Previous process did not stop')
 output=shell('aa start -a EntryAbility -b '+shlex.quote(bundle));(report/'starts.log').open('a').write(output+'\n')
try:
 proof=hap.with_name('entry-default-unsigned.provenance.json')
 result['build_verification']=verify(hap,proof,source)
 shutil.copy2(proof,report/proof.name)
 capture=replace_once((ROOT/'scripts/original_renderer_capture.py').read_text(),"r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38","r.get('originalCoreMacroCount')==193 and r.get('originalCoreFunctionCount')==46")
 capture=replace_once(capture,'from renderer_probe_identity import package_identity,installed_identity','from renderer_probe_identity import package_identity,installed_identity\nfrom app_report_log import parse_app_reports')
 capture=replace_once(capture,"  d=json.loads(last)","  d=json.loads(last)\n  live_pid=shell('pidof '+BUNDLE).strip()\n  current_logs=shell('hilog -x -P '+live_pid+' -T Olex2Native') if live_pid.isdigit() else ''\n  active_reports=parse_app_reports(current_logs)")
 capture=replace_once(capture,"  if d.get('graphics',{}).get('frames',0)>0","  if d.get('installedPackage')==installed_identity(expected_identity) and d in active_reports and d.get('graphics',{}).get('frames',0)>0")
 capture=replace_once(capture,"(out/'native-verification.json').write_text(last);","(out/'process-scoped-report.log').write_text(current_logs);(out/'native-verification.json').write_text(last);")
 (report/'capture-profile.py').write_text(capture);(report/'original-window-profile.py').write_text(original_profile())
 (report/'install.log').write_text(hdc('install',hap))
 previous=None;pid_owners={}
 for index,label in enumerate(('fresh','restart-1','restart-2','restart-3','repeat-2','repeat-3')):
  if index<4:start();previous=None
  prefix='persistent-'+label;common=['--target',a.target,'--hdc',a.hdc,'--hap',hap]
  invoke(report/'capture-profile.py',*common,'--report',report,'--label',prefix+'-before')
  before=report/'runtime'/(prefix+'-before');after=report/'runtime'/(prefix+'-after');folder=report/'runtime'/(prefix+'-original')
  invoke(ROOT/'scripts/original_renderer_click.py','--target',a.target,'--hdc',a.hdc,'--layout',before/'layout.json','--id','olex2-original-renderer-probe')
  run=1 if index<4 else index-2
  invoke(ROOT/'scripts/original_renderer_receive.py',*common,'--report',report,'--run',run,'--folder',prefix+'-original')
  invoke(report/'capture-profile.py',*common,'--report',report,'--label',prefix+'-after')
  current=validate(folder,before,after,identity,previous)
  (folder/'independent-persistent-gx-validation.json').write_text(json.dumps(current,ensure_ascii=False,indent=2)+'\n')
  texture=validate_texture(folder);(folder/'independent-texture-1d-validation.json').write_text(json.dumps(texture,ensure_ascii=False,indent=2)+'\n')
  result['rounds'].append(dict(label=label,original=current,texture_1d=texture))
  if not current['passed'] or not texture['passed']:raise ValueError('Actual persistent GX window or original 1D evidence failed: '+label)
  previous=current;pid_owners[current['pid']]=current['addresses']
 if len(pid_owners)!=4:raise ValueError('Four independent application processes required')
 result['passed']=True
except Exception as e:result['error']=type(e).__name__+': '+str(e)
finally:
 for name in ('persistent_gx_window_validation.py','run-persistent-gx-window-smoke.py','texture_1d_window_validation.py'):shutil.copy2(ROOT/'scripts'/name,report/name)
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),rounds=len(result['rounds']),error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
