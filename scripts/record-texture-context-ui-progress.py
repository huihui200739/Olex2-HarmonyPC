#!/usr/bin/env python3
"""Revalidate full candidate14 diagnostic UI, every UI PID and 12 initialized starts."""
import argparse,hashlib,json,re,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from texture_context_application_validation import validate_application,canonical
from texture_context_startup_validation import validate_runtime_snapshot
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
identity=package_identity(hap);require(identity==dict(bundleName='com.example.olex2harmonypc',versionName='0.22.14',versionCode=221400),'Only exact current candidate14 accepted')
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
 folder=platform_path.parent/('pid-'+str(item['pid']));raw=load(folder/'result.json');require(raw['pid']==item['pid'],'Native process identity differs')
 for name,record in item['files'].items():check_blob(folder/name,record);require(record['sha256']==record['target_sha256'],'Target/received native artifact SHA differs')
 actual=canonical(platform_checks(raw,fixtures))
 for key,value in actual.items():require(item[key]==value,'Raw native resource/image lifecycle evidence differs: '+key)
require(len(startup['runs'])==12 and startup['passed'] is True and startup.get('long_lived_runtime_initialization_required') is True,'Exactly 12 successful initialized long-lived-owner starts required')
require(len({r['pid'] for r in startup['runs']})==12 and [r['run'] for r in startup['runs']]==list(range(1,13)),'Startup process/ordinal protocol differs')
for row in startup['runs']:
 require(row['passed'] is True,'A failed startup must not be hidden')
 folder=startup_path.parent/('run-'+str(row['run']));raw=load(folder/'native-result.json');require(raw['pid']==row['pid'] and sha(folder/'native-result.json')==row['native_result_sha256']==row['target_sha256'],'Startup raw target evidence differs')
 actual=canonical(platform_checks(raw,fixtures))
 for key,value in actual.items():require(row['images' if key=='image_capabilities' else key]==value,'Startup image/resource evidence differs: '+key)
 snapshot=load(folder/'runtime-status.json');runtime=validate_runtime_snapshot(snapshot,identity)
 require(runtime['passed'] and row['runtime_owner']==runtime and sha(folder/'runtime-status.json')==row['runtime_status_sha256'] and sha(folder/'runtime-process-report.log')==row['runtime_log_sha256'],'Initialized runtime startup evidence changed or failed')
 require(snapshot in parse_app_reports((folder/'runtime-process-report.log').read_text()),'Startup snapshot was not produced by the explicit live PID report')
host_path=ROOT/'reports/persistent-gx-host-catalog-reference.json';host=load(host_path);require(host['passed'] is True and host['assertions']==276,'Genuine host metadata reference did not pass')
for record in [host['raw_log']]+host['build_evidence']+host['compiled_sources']:check_blob(ROOT/record['path'],record)
logroot=ROOT/'out/gx-application-evaluation'
log_paths={name:logroot/name for name in ('all-python-tests-texture-context14-ui-final.log','production-host-ctest-texture-context-ui.log','persistent-gx-model-ctest-ui.log','persistent-gx-catalog-host-ctest.log','texture-context-all-verifier-tests-final.log','texture-context-ui-tools14-replay.log','texture-context-host-ctest.log','texture-context-host-observation.json')}
require('Ran 650 tests' in log_paths['all-python-tests-texture-context14-ui-final.log'].read_text() and log_paths['all-python-tests-texture-context14-ui-final.log'].read_text().rstrip().endswith('OK'),'Complete 650-test Python regression did not pass')
for name,count in [('production-host-ctest-texture-context-ui.log',18),('persistent-gx-model-ctest-ui.log',1),('persistent-gx-catalog-host-ctest.log',1)]:require('100% tests passed, 0 tests failed out of '+str(count) in log_paths[name].read_text(),'Host CTest did not pass: '+name)
require('Ran 44 tests' in log_paths['texture-context-all-verifier-tests-final.log'].read_text() and log_paths['texture-context-all-verifier-tests-final.log'].read_text().rstrip().endswith('OK'),'All isolated verifier cases did not pass')
require('PASS exact texture-context UI tool replay; 65 files' in log_paths['texture-context-ui-tools14-replay.log'].read_text(),'Tool recipe did not replay byte-exactly')
require('100% tests passed, 0 tests failed out of 1' in log_paths['texture-context-host-ctest.log'].read_text(),'Detached-context original manager host CTest did not pass')
current=ROOT/'reports/gx-application-platform-status.json';archive=ROOT/'reports/gx-texture-context-before-ui-status.json'
if not archive.exists():
 old=load(current);require(old['current_build_candidate']['identity']==identity and old['current_ui']['status']=='not-run-for-0.22.14','Preserve the exact pre-UI candidate14 report');shutil.copy2(current,archive)
old=load(archive);now=datetime.now(ZoneInfo('Asia/Shanghai'));record_dir=ROOT/'reports/emulator'/('texture-context-ui-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);record_dir.mkdir(parents=True)
installed=shell('bm dump -n com.example.olex2harmonypc');(record_dir/'installed-bundle.log').write_text(installed);installed=json.loads(installed[installed.index('{'):])
for part in (installed,installed['applicationInfo']):require(part['versionName']==identity['versionName'] and part['versionCode']==identity['versionCode'],'Actual installed package differs')
pid=shell('pidof com.example.olex2harmonypc').strip();require(pid==str(startup['runs'][-1]['pid']),'The final startup process is no longer live')
record=dict(kind='original-renderer-retained-texture-full-diagnostic-ui-qualification',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,production_baseline=old['production_baseline'],current_build_candidate=dict(old['current_build_candidate']),target=dict(old['target']),
 current_ui=dict(status='passed-limited-212-check-diagnostic-ui',report=bound(app_path),qualification=ui,full_original_ui_implemented=False),platform_image_audit=dict(status='passed-for-all-seven-current-UI-PIDs',report=bound(platform_path),pids=sorted(ui_pids),complete_descriptor_metadata_scan_passed=platform['complete_descriptor_metadata_scan_passed']),
 startup_audit=dict(status='passed-fixed-12-current-candidate-initialized-startups',report=bound(startup_path),actual_starts=12,passed=12,failed=0,all_twelve_passed=True,actual_long_lived_owner_startup_checks=120,process_fd_equality_check_preserved=True,complete_descriptor_metadata_scan_passed=startup['complete_descriptor_metadata_scan_passed'],all_sdk_internal_allocations_proven_released=False),
 persistent_gx=dict(old['persistent_gx']),genuine_host_catalog=dict(report=bound(host_path),root_registry=host['root_registry'],recursive_catalog=host['recursive_catalog'],assertions=276,original_cpp_compiled=379,actual_host_compiled_sources=382,gl_context_executed=False),
 tests=dict(python_cases=650,standard_host_ctest_cases=18,genuine_gx_metadata_host_ctest_cases=1,genuine_gx_metadata_assertions=276,modeled_scene_ctest_cases=1,modeled_scene_assertions=83,isolated_verifier_cases=44,host_models_packaged=False),
 remaining=['原TGXApp::LoadTextures真实GPU上传/替换/过滤及真实日志增量','二维纹理实际跨context迁移、一维mipmap/其他采样格式和GPU资源异常恢复','真实字体后端、完整原UI/资源/命令/Python/插件','P1～P8完整交付、真机与物理输入、签名和发行'],history=dict(before_current_UI=bound(archive),previous_candidate13_complete_diagnostic=bound(ROOT/'reports/gx-application-platform-before-texture-context-status.json'),old_failures_preserved=True),updated_at=now.isoformat(timespec='seconds'))
record['current_build_candidate']['verification']=verification;record['current_build_candidate']['promotion']='current fully diagnostic-UI-qualified development candidate; standard 0.22.0 unchanged'
require(ui['prior_206_ui_checks_preserved'] is True and ui['actual_context_transitions']==2 and ui['total_retained_texture_checks']==294,'New diagnostic UI did not preserve all 206 prior gates and two real context transitions')
mock=load(ROOT/'out/gx-application-evaluation/texture-context-host-observation.json');require(mock['passed'] is True and mock['checks']==843 and mock['real_gpu_executed'] is False,'Detached-context mocked interface regression differs')
record['persistent_gx']['total_retained_texture_checks']=ui['total_retained_texture_checks']
record['tests'].update(retained_texture_mock_ctest_cases=1,retained_texture_mock_assertions=843,prior_texture_mock_assertions_preserved=830,host_mock_is_not_gpu_evidence=True)
record['target']['current_pid']=int(pid);record['persistent_gx']['window_rounds']=ui['persistent_window_rounds'];record['persistent_gx']['total_original_window_checks']=756;record['persistent_gx']['total_lifetime_checks']=60;record['persistent_gx']['total_1d_checks']=384
files=[ROOT/'scripts'/name for name in ('texture_context_application_validation.py','persistent_gx_catalog_reference.py','texture_context_startup_validation.py','prepare-texture-context-ui-tools.py','collect-gx-texture-context-evaluation.py','run-texture-context-startup-audit.py','capture-persistent-gx-host-reference.py','record-texture-context-ui-progress.py','texture_context_window_validation.py')]+[ROOT/'experiments/gx-persistent-runtime/host'/name for name in ('CMakeLists.txt','catalog_host.cpp')]
record['evidence']=[bound(app_path),bound(platform_path),bound(startup_path),bound(host_path),bound(record_dir/'installed-bundle.log'),bound(tools/'tool-profile.json')]+[bound(path) for path in log_paths.values()]+[bound(path) for path in files]
(record_dir/'result.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');current.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');(ROOT/'reports/gx-persistent-runtime-status.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');(ROOT/'reports/gx-texture-context-status.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status='in_progress',version='0.22.14',diagnostic_ui_checks=212,all_UI_PIDs=7,successful_starts=12,full_port_complete=False,report_directory=str(record_dir)),ensure_ascii=False,indent=2))
