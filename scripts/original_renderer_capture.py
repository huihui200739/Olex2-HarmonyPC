#!/usr/bin/env python3
import argparse,json,subprocess,time,sys,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--target',required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('--label',required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--hdc',required=True);a=p.parse_args()
from renderer_probe_identity import package_identity,installed_identity
expected_identity=package_identity(a.hap)
from matrix_validation import original_matrices_match
from polygon_validation import polygon_probe_matches
from line_path_validation import path_probe_matches
from original_log_validation import audit_matches
HDC=a.hdc;BUNDLE=expected_identity['bundleName'];remote=f'/data/app/el2/100/base/{BUNDLE}/haps/entry/files';out=a.report/'runtime'/a.label;out.mkdir(parents=True,exist_ok=False)
def hdc(*cmd):
 r=subprocess.run([HDC,'-t',a.target,*cmd],capture_output=True,text=True,timeout=60)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 return r.stdout+r.stderr
def shell(s):return hdc('shell',s)
deadline=time.monotonic()+25;last=''
while time.monotonic()<deadline:
 last=shell('cat '+remote+'/native-verification.json')
 try:
  d=json.loads(last)
  if d.get('graphics',{}).get('frames',0)>0 and d.get('applicationRuntime',{}).get('initialized') and d.get('utilityPassed') and d.get('workspaceState',{}).get('saveStatus')=='saved':
   break
 except ValueError:pass
 time.sleep(.25)
else:
 (out/'missing-result.log').write_text(last);raise RuntimeError('No complete current UI result')
(out/'native-verification.json').write_text(last);(out/'pid.txt').write_text(shell('pidof '+BUNDLE).strip()+'\n')
checks=[]
def check(name,ok,detail=None):checks.append({'name':name,'passed':bool(ok),'detail':detail})
g=d['graphics'];r=d['applicationRuntime'];identity=d['installedPackage'];check('actual installed identity',identity==installed_identity(expected_identity),identity)
check('real existing utility assertions',d['utilityPassed'] is True and len(d['checks'])==d['checkCount'] and all(c.get('passed') is True for c in d['checks']),d['checkCount'])
check('single original runtime initialized',r.get('initialized') is True and r.get('originalCoreInitialized') is True and r.get('originalCoreEmptyFile') is True and r.get('originalFunctionCount')==22 and r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38 and r.get('error')=='',r)
for key in ['initialized','surfaceReady','meshValidated','submissionValidated','geometryVerified','pixelVerified','coverageVerified','materialVerified','colorVerified']:check('actual main graphics '+key,g.get(key) is True)
check('original camera and actual uniforms',original_matrices_match(g));check('existing actual polygon probe',polygon_probe_matches(g));check('existing actual line-path probe',path_probe_matches(g));check('original log bridge attached',audit_matches(d['originalLog']))
for kind in ['layout','screen']:
 path=f'/data/local/tmp/shared-core-{a.label}.{ "json" if kind=="layout" else "jpeg"}'
 (out/(kind+'.log')).write_text(shell(f'uitest dumpLayout -b {BUNDLE} -p '+path if kind=='layout' else 'snapshot_display -f '+path));hdc('file','recv',path,str(out/('layout.json' if kind=='layout' else 'screen.jpeg')))
summary={'passed':all(c['passed'] for c in checks),'checks':checks,'scope':'Shared-core candidate initial/current UI state only, not full 187-application regression','expected_identity':expected_identity,'functional_completion_percent':None,'target':a.target,'hap_sha256':hashlib.sha256(a.hap.read_bytes()).hexdigest()};(out/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'passed':summary['passed'],'checks':len(checks),'failed':[c for c in checks if not c['passed']]},ensure_ascii=False,indent=2));raise SystemExit(0 if summary['passed'] else 1)
