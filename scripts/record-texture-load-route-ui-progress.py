#!/usr/bin/env python3
"""Revalidate full candidate16 diagnostic UI, every UI PID and 12 initialized starts."""
import argparse,hashlib,json,re,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from texture_load_route_application_validation import validate_application,canonical
from texture_load_route_startup_validation import validate_runtime_snapshot
from texture_load_route_tool_profiles import collection_source,startup_source
from app_report_log import parse_app_reports
from gx_application_validation import validate_lifecycle,validate_image_observations,inspect_owned_resource_observations,validate_no_decoder_control
from png_texture_hook_validation import validate_png_texture_hook
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('ui-tools','app-report','platform-report','startup-report','source','hap'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--target',required=True);p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def load(path):return json.loads(path.read_text())
def bound(path):
 path=path.resolve();return dict(path=str(path.relative_to(ROOT)),bytes=path.stat().st_size,sha256=sha(path))
def shell(command):
 marker='GX_UI_EXIT_'+uuid.uuid4().hex;r=subprocess.run([a.hdc,'-t',a.target,'shell','( '+command+' ); code=$?; printf "\\n'+marker+'=%s\\n" "$code"'],capture_output=True,text=True,timeout=80)
 text=r.stdout+r.stderr;match=re.search(marker+r'=(\d+)',text);require(r.returncode==0 and match and match[1]=='0',text);return text[:match.start()].rstrip()
def check_blob(path,expected):require(path.stat().st_size==expected['bytes'] and sha(path)==expected['sha256'],'Bound evidence changed: '+str(path))
def platform_checks(raw,fixtures):
 return dict(lifecycle=validate_lifecycle(raw['lifecycle']),image_capabilities=validate_image_observations(raw['images'],fixtures,require_resources=True,resource_proof='owned-memory'),owned_resources=inspect_owned_resource_observations(raw,proof='owned-memory',texture_backend='libpng-unassociated-rgb'),no_decoder_control=validate_no_decoder_control(raw['no_decoder_control'],'before-first-image-api'),post_decode_control=validate_no_decoder_control(raw['post_decode_control'],'after-capability-probes'),texture_hook=validate_png_texture_hook(raw['texture_hook'],fixtures,require_resources=True,resource_proof='owned-memory'))
source,hap,tools,app_path,platform_path,startup_path=map(lambda p:p.resolve(),(a.source,a.hap,a.ui_tools,a.app_report,a.platform_report,a.startup_report))
identity=package_identity(hap);require(identity==dict(bundleName='com.example.olex2harmonypc',versionName='0.22.16',versionCode=221600),'Only exact current candidate16 accepted')
verification=verify(hap,hap.with_suffix('.provenance.json'),source)
app=load(app_path);platform=load(platform_path);startup=load(startup_path);ui=canonical(validate_application(app,tools,identity))
require(platform['passed'] is True and platform['target']==startup['target']==a.target,'Platform/startup target or result differs')
require(platform['application']['identity']==startup['identity']==identity and platform['application']['hap_sha256']==startup['hap']['sha256']==sha(hap),'Platform/startup HAP differs')
require(platform['application']['ui']==ui and platform['application']['sha256']==sha(app_path),'Revalidated full diagnostic UI differs')
fixtures=load(ROOT/'scripts/tests/fixtures/gx_synthetic_images.json')['fixtures'];ui_pids=set()
def pids_in(value):
 if isinstance(value,dict):
  for key,item in value.items():
   if key in ('pid','new_pid','prior_pid') and str(item).isdigit() and int(item)>0:ui_pids.add(int(item))
   pids_in(item)
 elif isinstance(value,list):
  for item in value:pids_in(item)
pids_in(app)
require({row['pid'] for row in platform['processes']}==ui_pids and len(ui_pids)==7,'Expected every one of the seven actual UI process IDs, not only drawing PIDs')
for item in platform['processes']:
 folder=platform_path.parent/('pid-'+str(item['pid']));raw=load(folder/'result.json');require(raw['pid']==item['pid'] and raw.get('execution_completed') is True,'Native process identity/completion differs')
 for name,record in item['files'].items():check_blob(folder/name,record);require(record['sha256']==record['target_sha256'],'Target/received native artifact SHA differs')
 actual=canonical(platform_checks(raw,fixtures))
 for key,value in actual.items():require(item[key]==value,'Raw native resource/image lifecycle evidence differs: '+key)
require(len(startup['runs'])==12 and startup['passed'] is True and startup.get('long_lived_runtime_initialization_required') is True,'Exactly 12 successful initialized long-lived-owner starts required')
require(len({r['pid'] for r in startup['runs']})==12 and [r['run'] for r in startup['runs']]==list(range(1,13)),'Startup process/ordinal protocol differs')
for row in startup['runs']:
 require(row['passed'] is True,'A failed startup must not be hidden')
 folder=startup_path.parent/('run-'+str(row['run']));raw=load(folder/'native-result.json');require(raw['pid']==row['pid'] and raw.get('execution_completed') is True and sha(folder/'native-result.json')==row['native_result_sha256']==row['target_sha256'],'Startup raw target evidence differs')
 actual=canonical(platform_checks(raw,fixtures))
 for key,value in actual.items():require(row['images' if key=='image_capabilities' else key]==value,'Startup image/resource evidence differs: '+key)
 snapshot=load(folder/'runtime-status.json');runtime=validate_runtime_snapshot(snapshot,identity)
 require(runtime['passed'] and row['runtime_owner']==runtime and sha(folder/'runtime-status.json')==row['runtime_status_sha256'] and sha(folder/'runtime-process-report.log')==row['runtime_log_sha256'],'Initialized runtime startup evidence changed or failed')
 require(snapshot in parse_app_reports((folder/'runtime-process-report.log').read_text()),'Startup snapshot was not produced by the explicit live PID report')
host_path=ROOT/'reports/persistent-gx-host-catalog-reference.json';host=load(host_path);require(host['passed'] is True and host['assertions']==276,'Genuine host metadata reference did not pass')
for record in [host['raw_log']]+host['build_evidence']+host['compiled_sources']:check_blob(ROOT/record['path'],record)
# Bind exactly the executed candidate16 profiles, not only the baseline tools.
for path,expected in ((platform_path,collection_source()),(startup_path,startup_source())):
 report=load(path);profile=report['execution_profile'];saved=path.parent/profile['source_file']
 require(profile['candidate']=='0.22.16' and saved.read_text()==expected and sha(saved)==profile['source_sha256'],'Executed candidate16 profile changed')
 require(set(report['profile_dependencies'])=={'texture_load_route_tool_profiles.py','texture_load_route_application_validation.py','texture_load_route_startup_validation.py','texture_context_application_validation.py','texture_context_startup_validation.py'},'Exact profile dependency set differs')
 for name,digest in report['profile_dependencies'].items():
  require(sha(path.parent/name)==digest==sha(ROOT/'scripts'/name),'Candidate16 profile dependency changed: '+name)
logroot=ROOT/'out/gx-application-evaluation'
log_paths={name:logroot/name for name in ('all-python-tests-texture-load16-ui-final.log','texture-load16-tool-profile-tests-attempt1.log','texture-load16-standard-consistency.log','texture-load16-generator-replay.log','texture-load-route-ui-tools16-replay.log','production-host-ctest-texture-load15.log','texture-context-host-observation.json')}
require('Ran 720 tests' in log_paths['all-python-tests-texture-load16-ui-final.log'].read_text() and log_paths['all-python-tests-texture-load16-ui-final.log'].read_text().rstrip().endswith('OK'),'Complete current720 Python regression did not pass')
require('Ran 14 tests' in log_paths['texture-load16-tool-profile-tests-attempt1.log'].read_text() and log_paths['texture-load16-tool-profile-tests-attempt1.log'].read_text().rstrip().endswith('OK'),'Exact16 isolated profiles did not pass')
require('100% tests passed, 0 tests failed out of 18' in log_paths['production-host-ctest-texture-load15.log'].read_text(),'Unchanged standard18 host CTest did not pass')
require('PASS: all three candidate16 changed files and exact input manifest reproduced' in log_paths['texture-load16-generator-replay.log'].read_text(),'Candidate16 generator did not replay exactly')
require('PASS exact candidate16 UI tool replay; 65 files; prior 212 analytical gates preserved' in log_paths['texture-load-route-ui-tools16-replay.log'].read_text(),'Candidate16 UI tools did not replay')
current=ROOT/'reports/gx-application-platform-status.json';archive=ROOT/'reports/gx-texture-load-route-before-ui-status.json'
if not archive.exists():
 prior=load(current);require(prior['current_build_candidate']['identity']==identity and prior['mixed_routes']['limited_actual_window_verified'] is True,'Preserve actual candidate16 mixed qualification');shutil.copy2(current,archive)
old=load(archive)
require(old['current_build_candidate']['identity']==identity and old['current_build_candidate']['hap']['sha256']==sha(hap),'Prior mixed qualification was for another build')
for entry in old['evidence']:check_blob(ROOT/entry['path'],entry)
require(old['mixed_routes']['actual_context_leases']==12 and old['mixed_routes']['actual_original_log_notifications']==24,'Do not lose actual mixed-route qualification')
now=datetime.now(ZoneInfo('Asia/Shanghai'));record_dir=ROOT/'reports/emulator'/('texture-load-route-ui-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);record_dir.mkdir(parents=True)
installed=shell('bm dump -n com.example.olex2harmonypc');(record_dir/'installed-bundle.log').write_text(installed);installed=json.loads(installed[installed.index('{'):])
for part in (installed,installed['applicationInfo']):require(part['versionName']==identity['versionName'] and part['versionCode']==identity['versionCode'],'Actual installed package differs')
pid=shell('pidof com.example.olex2harmonypc').strip();require(pid==str(startup['runs'][-1]['pid']),'The final startup process is no longer live')
record=dict(old)
record.update(kind='original-texture-mixed-routes-and-full-diagnostic-ui-qualification',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,
 current_ui=dict(status='passed-limited-212-check-diagnostic-ui',report=bound(app_path),qualification=ui,full_original_ui_implemented=False),
 platform_image_audit=dict(status='passed-for-all-seven-current-UI-PIDs',report=bound(platform_path),pids=sorted(ui_pids),complete_descriptor_metadata_scan_passed=platform['complete_descriptor_metadata_scan_passed']),
 startup_audit=dict(status='passed-fixed-12-current-candidate-initialized-startups',report=bound(startup_path),actual_starts=12,passed=12,failed=0,all_twelve_passed=True,actual_long_lived_owner_startup_checks=120,process_fd_equality_check_preserved=True,complete_descriptor_metadata_scan_passed=startup['complete_descriptor_metadata_scan_passed'],all_sdk_internal_allocations_proven_released=False),
 genuine_host_catalog=dict(report=bound(host_path),root_registry=host['root_registry'],recursive_catalog=host['recursive_catalog'],assertions=276,original_cpp_compiled=379,actual_host_compiled_sources=382,gl_context_executed=False),
 remaining=['15布局读取超时的历史根因未归因；新16本次成功不能证明旧失败原因','字体、完整原UI/菜单/命令/文档/资源/Python/插件','完整纹理格式/mipmap/异常恢复、P1～P8、真机物理输入和签名发行'],updated_at=now.isoformat(timespec='seconds'))
record['history']=dict(old['history'],before_current_UI=bound(archive))
record['tests']=dict(old['tests'],python_cases=720,standard_host_ctest_cases=18,exact16_tool_profile_isolated_cases=14,host_models_packaged=False)
record['current_build_candidate']=dict(old['current_build_candidate'],verification=verification,promotion='current212 diagnostic UI / all7 UI PIDs / fixed12 initialized startups / mixed12 actions qualified; not full original application')
record['persistent_gx']=dict(old['persistent_gx']);record['target']=dict(old['target'])
mock=load(ROOT/'out/gx-application-evaluation/texture-context-host-observation.json');require(mock['passed'] is True and mock['checks']==843 and mock['real_gpu_executed'] is False,'Detached-context mocked interface regression differs')
record['persistent_gx']['total_retained_texture_checks']=ui['total_retained_texture_checks']
record['tests'].update(retained_texture_mock_ctest_cases=1,retained_texture_mock_assertions=843,prior_texture_mock_assertions_preserved=830,host_mock_is_not_gpu_evidence=True)
record['target']['current_pid']=int(pid);record['persistent_gx']['window_rounds']=ui['persistent_window_rounds'];record['persistent_gx']['total_original_window_checks']=756;record['persistent_gx']['total_lifetime_checks']=60;record['persistent_gx']['total_1d_checks']=384
files=[ROOT/'scripts'/name for name in ('texture_load_route_application_validation.py','texture_load_route_tool_profiles.py','texture_load_route_startup_validation.py','collect-gx-texture-load-route-evaluation.py','run-texture-load-route-startup-audit.py','record-texture-load-route-ui-progress.py')]
record['evidence']=old['evidence']+[bound(archive),bound(app_path),bound(platform_path),bound(startup_path),bound(host_path),bound(record_dir/'installed-bundle.log'),bound(tools/'tool-profile.json')]+[bound(path) for path in log_paths.values()]+[bound(path) for path in files]
for destination in (record_dir/'result.json',current,ROOT/'reports/gx-persistent-runtime-status.json',ROOT/'reports/gx-texture-load-status.json',ROOT/'reports/gx-texture-load-route-status.json'):
 destination.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status='in_progress',version='0.22.16',diagnostic_ui_checks=212,all_UI_PIDs=7,successful_starts=12,mixed_route_actions=12,full_port_complete=False,report_directory=str(record_dir)),ensure_ascii=False,indent=2))
