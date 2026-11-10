"""Revalidate candidate14 diagnostic UI, preserving every prior 200 check."""
import copy,hashlib,json
from pathlib import Path
from original_renderer_integration_validation import ROUNDTRIPS,summarize_application
from persistent_gx_window_validation import SOURCE,validate as validate_window,original_profile,replace_once
from persistent_gx_catalog_reference import parse_catalog_log
from texture_1d_window_validation import validate as validate_texture
from texture_context_window_validation import validate as validate_context
from original_log_validation import app_original_log_evidence_matches
ROOT=Path(__file__).resolve().parents[1]
CONTEXT={'shared_texture_context_'+label for label in ROUNDTRIPS}
ADDED={'shared_persistent_gx_'+label for label in ROUNDTRIPS}|CONTEXT
TEXTURE={'shared_texture_1d_'+label for label in ROUNDTRIPS}
BASELINE_200=ROOT/'out/gx-application-evaluation/source11/reports/app/20261109-225628-1ef641/result.json'
BASELINE_SHA='542890874086098d382af0963fc2a1f65d59cd0d3e504e5db3613c60296647e3'
def require(value,message):
 if not value:raise ValueError(message)
def canonical(value):return json.loads(json.dumps(value,ensure_ascii=False,allow_nan=False))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def validate_structure(names,baseline):
 require(len(baseline)==200 and len(set(baseline))==200,'Prior 200-check diagnostic baseline differs')
 require(len(names)==212 and len(set(names))==212 and ADDED<=set(names),'Expected exactly 200 prior gates and six persistent GX and six retained texture gates')
 require([name for name in names if name not in ADDED]==baseline,'Prior 200 UI checks removed, renamed or reordered')
def runtime_profile():
 p=ROOT/'scripts/application_runtime_validation.py';s=p.read_text();start=s.index('SOURCE=');end=s.index('\n',start)
 s=s[:start]+'SOURCE='+repr(SOURCE)+s[end:]
 return replace_once(s,"('originalCoreMacroCount',135),('originalCoreFunctionCount',38)","('originalCoreMacroCount',193),('originalCoreFunctionCount',46)")
def verify_tool_profile(root,identity,hap_sha):
 root=Path(root).resolve();manifest=json.loads((root/'tool-profile.json').read_text())
 require(identity.get('versionName')=='0.22.14' and identity.get('versionCode')==221400 and manifest['version']=='0.22.14','Only candidate14 persistent GX profile accepted')
 require(manifest['hap_sha256']==hap_sha and manifest['expected_ui_checks']==212,'Tool profile/HAP/gate count differs')
 require(manifest['root_registry']==dict(macros=193,functions=46) and manifest['recursive_catalog']==dict(macros=196,functions=52),'Root versus recursive GX inventory differs')
 files={str(p.relative_to(root)):p for folder in ('scripts','profile-baseline','app') for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts}
 require(files.keys()==manifest['files'].keys(),'Candidate tools file inventory differs')
 for name,path in files.items():
  row=manifest['files'][name];require(sha(path)==row['sha256'] and path.stat().st_size==row['bytes'],'Captured UI tool changed: '+name)
 require((root/'profile-baseline/scripts/original_renderer_validation.py').read_bytes()==(ROOT/'scripts/original_renderer_validation.py').read_bytes(),'Original analytical verifier baseline changed')
 require((root/'scripts/original_renderer_validation.py').read_text()==original_profile(),'Original 126-check profile differs from exact lifecycle/count/shader changes')
 require((root/'scripts/application_runtime_validation.py').read_text()==runtime_profile(),'Runtime expected semantics changed beyond exact GX source/count profile')
 return manifest

def validate_application(app,root,identity):
 root=Path(root).resolve();manifest=verify_tool_profile(root,identity,app['hap']['sha256'])
 require(sha(BASELINE_200)==BASELINE_SHA,'Prior full diagnostic UI baseline changed')
 baseline=json.loads(BASELINE_200.read_text());checks=app['checks'];names=[c['name'] for c in checks]
 validate_structure(names,[c['name'] for c in baseline['checks']])
 prior_path=ROOT/'out/gx-application-evaluation/persistent-gx-ui-tools13-v2/reports/app/20261110-000055-fc88d4/result.json'
 require(sha(prior_path)=='57a32799135b74a81b4a33eb7f708a305d6321ccb539fe7078622b94ac7d2eb1','Prior 206-check diagnostic baseline changed')
 prior=json.loads(prior_path.read_text())
 require([n for n in names if n not in CONTEXT]==[c['name'] for c in prior['checks']],'Prior 206 checks removed, renamed or reordered')
 require(app['result'].startswith('passed:') and all(c['passed'] is True for c in checks),'Current full diagnostic UI did not pass')
 base=copy.deepcopy(app);base['checks']=[c for c in checks if c['name'] not in ADDED|TEXTURE]
 summary=summarize_application(base,root,identity)
 namespace={'__name__':'independent_persistent_gx_runtime_profile'};exec(compile(runtime_profile(),'independent_persistent_gx_runtime_profile','exec'),namespace)
 require(namespace['app_runtime_evidence_matches'](app) and namespace['app_core_evidence_matches'](app),'Raw application settings/function/runtime evidence differs')
 log_path=root/'reports/app/persistent-gx-host-catalog.log';require(sha(log_path)==manifest['host_catalog_log']['sha256'],'Independent genuine GX host reference changed')
 reference=parse_catalog_log(log_path.read_text());require(json.loads((root/'host-catalog-reference.json').read_text())==reference,'Parsed original host catalog reference changed')
 require(app_original_log_evidence_matches(app,reference),'Original log/visible metadata UI differs from genuine GX host bytes')
 report=(root/app['report_directory']).resolve();require(report.is_relative_to(root/'reports/app'),'Application report escaped exact tool workspace')
 by_name={c['name']:c for c in checks};previous_by_pid={};previous_folders={};rounds=[]
 for label in ROUNDTRIPS:
  prefix='shared-'+label;folder=report/'runtime'/(prefix+'-original');before=report/'runtime'/(prefix+'-before');after=report/'runtime'/(prefix+'-after')
  pid=by_name['shared_original_'+label]['details']['pid'];current=canonical(validate_window(folder,before,after,identity,previous_by_pid.get(pid),create_previews=False));texture=canonical(validate_texture(folder))
  require(current['passed'] and len(current['original']['checks'])==126 and len(current['checks'])==10,'Actual original window or unique long-lived GX proof failed: '+label)
  require(current==by_name['shared_persistent_gx_'+label]['details'] and current['original']==by_name['shared_original_'+label]['details'],'Raw lifetime/original-window evidence changed: '+label)
  require(texture['passed'] and len(texture['checks'])==64 and texture==by_name['shared_texture_1d_'+label]['details'],'Raw actual 1D window evidence changed: '+label)
  retained=validate_context(folder,previous_folders.get(pid))
  require(retained['passed'] and retained==by_name['shared_texture_context_'+label]['details'],'Raw retained-texture context evidence changed: '+label)
  previous_folders[pid]=folder
  previous_by_pid[pid]=current;rounds.append(dict(label=label,pid=pid,run=current['run'],original_window_checks=126,lifetime_checks=10,texture_1d_checks=64,retained_texture_checks=len(retained['checks']),actual_context_migration_verified=retained['actual_context_migration_verified'],addresses=current['addresses']))
 require(sum(r['actual_context_migration_verified'] for r in rounds)==2,'Two actual consecutive context transitions required')
 summary.update(prior_206_ui_checks_preserved=True,actual_context_transitions=2,total_retained_texture_checks=sum(r['retained_texture_checks'] for r in rounds),total_ui_checks=212,prior_200_ui_checks_preserved=True,persistent_window_rounds=rounds,total_independent_lifetime_checks=60,total_independent_texture_checks=384,root_macros=193,root_functions=46,recursive_catalog_macros=196,recursive_catalog_functions=52,functional_completion_percent=None,full_port_complete=False,physical_pc_verified=False,scope='All prior 200 diagnostic UI gates, six long-lived GX/window and six retained-texture gates, genuine recursive host metadata; not complete original UI/resources/Python/plugins or full port.')
 return summary
