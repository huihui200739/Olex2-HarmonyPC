#!/usr/bin/env python3
"""Prepare separate candidate13 UI tools without changing frozen native sources."""
import argparse,hashlib,json,shutil
from pathlib import Path
from build_provenance_png import verify
from persistent_gx_window_validation import SOURCE,original_profile,replace_once
from persistent_gx_catalog_reference import parse_catalog_log
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(source,out,hap,host_log):
 source,out,hap,host_log=map(lambda p:p.resolve(),(source,out,hap,host_log))
 if out.exists():raise FileExistsError(out)
 if not out.is_relative_to(ROOT/'out/gx-application-evaluation'):raise ValueError('Keep tools isolated inside this project')
 identity=json.loads((source/'app/AppScope/app.json5').read_text())['app']
 if identity['versionName']!='0.22.13' or identity['versionCode']!=221300:raise ValueError('Only frozen persistent candidate13 is accepted')
 proof=verify(hap,hap.with_suffix('.provenance.json'),source)
 native=json.loads((source/'persistent-gx-inputs.json').read_text())
 if not all(sha(source/name)==row['sha256'] for name,row in native['files'].items()):raise ValueError('Frozen candidate-native input changed')
 reference=parse_catalog_log(host_log.read_text());(out/'reports/app').mkdir(parents=True)
 shutil.copytree(source/'scripts',out/'scripts',ignore=shutil.ignore_patterns('__pycache__'))
 (out/'app/AppScope').mkdir(parents=True);shutil.copy2(source/'app/AppScope/app.json5',out/'app/AppScope/app.json5')
 (out/'profile-baseline/scripts').mkdir(parents=True)
 shutil.copy2(ROOT/'scripts/original_renderer_validation.py',out/'profile-baseline/scripts/original_renderer_validation.py')
 for name in ('persistent_gx_window_validation.py','persistent_gx_catalog_reference.py'):
  shutil.copy2(ROOT/'scripts'/name,out/'scripts'/name)
 p=out/'scripts/persistent_gx_window_validation.py';s=p.read_text();s=replace_once(s,'ROOT=Path(__file__).resolve().parents[1]',"ROOT=Path(__file__).resolve().parents[1]/'profile-baseline'");p.write_text(s)
 p=out/'scripts/application_runtime_validation.py';s=p.read_text();begin=s.index('SOURCE=');end=s.index('\n',begin);s=s[:begin]+'SOURCE='+repr(SOURCE)+s[end:]
 s=replace_once(s,"('originalCoreMacroCount',135),('originalCoreFunctionCount',38)","('originalCoreMacroCount',193),('originalCoreFunctionCount',46)");p.write_text(s)
 p=out/'scripts/original_log_validation.py';s=p.read_text();begin=s.index('def parse_catalog_log(text):');end=s.index('\ndef audit_matches(',begin)
 s=s[:begin]+'from persistent_gx_catalog_reference import parse_catalog_log\n'+s[end:];p.write_text(s)
 (out/'scripts/original_renderer_validation.py').write_text(original_profile())
 p=out/'scripts/original_renderer_capture.py';s=p.read_text()
 s=replace_once(s,"r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38","r.get('originalCoreMacroCount')==193 and r.get('originalCoreFunctionCount')==46")
 s=replace_once(s,'from renderer_probe_identity import package_identity,installed_identity','from renderer_probe_identity import package_identity,installed_identity\nfrom app_report_log import parse_app_reports')
 s=replace_once(s,'  d=json.loads(last)',"  d=json.loads(last)\n  live_pid=shell('pidof '+BUNDLE).strip()\n  current_logs=shell('hilog -x -P '+live_pid+' -T Olex2Native') if live_pid.isdigit() else ''\n  active_reports=parse_app_reports(current_logs)")
 s=replace_once(s,"  if d.get('graphics',{}).get('frames',0)>0","  if d.get('installedPackage')==installed_identity(expected_identity) and d in active_reports and d.get('graphics',{}).get('frames',0)>0")
 s=replace_once(s,"(out/'native-verification.json').write_text(last);","(out/'process-scoped-report.log').write_text(current_logs);(out/'native-verification.json').write_text(last);");p.write_text(s)
 p=out/'scripts/run-app-smoke.py';s=p.read_text()
 s=replace_once(s,'proof = verify_build_provenance(retained_hap, retained_proof, ROOT)','proof = verify_build_provenance(retained_hap, retained_proof, Path('+repr(str(source))+'))')
 s=replace_once(s,'def shared_original_roundtrip(label):','persistent_previous_by_pid={}\n\ndef shared_original_roundtrip(label):')
 marker="    record('shared_texture_1d_'+label,texture_validation['passed'],texture_validation)"
 s=replace_once(s,marker,marker+'''
    from persistent_gx_window_validation import validate as validate_persistent_gx
    previous=persistent_previous_by_pid.get(validation['pid'])
    from renderer_probe_identity import package_identity as retained_identity
    lifetime=validate_persistent_gx(folder,before,after,retained_identity(retained_hap),previous,create_previews=False)
    (folder/'independent-persistent-gx-validation.json').write_text(json.dumps(lifetime,ensure_ascii=False,indent=2)+'\\n')
    record('shared_persistent_gx_'+label,lifetime['passed'],lifetime)
    persistent_previous_by_pid[validation['pid']]=lifetime
''')
 s=replace_once(s,"reports/app/original-log-verified-host-original_log-tests.log","reports/app/persistent-gx-host-catalog.log");p.write_text(s)
 shutil.copy2(host_log,out/'reports/app/persistent-gx-host-catalog.log')
 (out/'host-catalog-reference.json').write_text(json.dumps(reference,ensure_ascii=False,indent=2)+'\n')
 manifest=dict(version='0.22.13',native_source=str(source.relative_to(ROOT)),hap_sha256=sha(hap),build_verification=proof,host_catalog_log=dict(path=str(host_log.relative_to(ROOT)),sha256=sha(host_log)),root_registry=dict(macros=193,functions=46),recursive_catalog=dict(macros=196,functions=52),expected_ui_checks=206,full_port_complete=False,functional_completion_percent=None,
  files={str(p.relative_to(out)):dict(sha256=sha(p),bytes=p.stat().st_size) for parent in (out/'scripts',out/'profile-baseline',out/'app') for p in sorted(parent.rglob('*')) if p.is_file() and '__pycache__' not in p.parts})
 (out/'tool-profile.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');print(json.dumps(dict(tools=str(out),hap_sha256=manifest['hap_sha256'],full_ui_executed=False,expected_ui_checks=206),ensure_ascii=False,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--host-log',type=Path,required=True);a=p.parse_args();prepare(a.source,a.output,a.hap,a.host_log)
