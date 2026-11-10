#!/usr/bin/env python3
"""Close out explicit PNG candidate evidence without claiming a complete port."""
import argparse,hashlib,json,re,shutil
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from png_texture_validation import validate_png_observations
from png_texture_hook_validation import validate_png_texture_hook
from gx_application_validation import validate_lifecycle,validate_image_observations,inspect_owned_resource_observations,validate_no_decoder_control
from original_renderer_integration_validation import validate_application
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','build','startup-report','cpu-report','python-log'):p.add_argument('--'+name,required=True,type=Path)
p.add_argument('--ui-report',type=Path);p.add_argument('--ui-collection',type=Path);a=p.parse_args()
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(path):return json.loads(Path(path).read_text())
def evidence(path):
 path=Path(path).resolve();require(path.is_relative_to(ROOT),'Evidence outside this project');return dict(path=path.relative_to(ROOT).as_posix(),bytes=path.stat().st_size,sha256=sha(path))
source=a.source.resolve();build=a.build.resolve();hap=build/'entry-default-unsigned.hap';sidecar=build/'entry-default-unsigned.provenance.json'
proof=verify(hap,sidecar,source);identity=package_identity(hap);require(identity['versionName']=='0.22.9','This closeout is scoped to the exact PNG candidate')
lock=load(source/'source.lock.json');native=load(sidecar)['native'];require(len(lock['patches'])==18,'Candidate upstream patch inventory differs')
require(len(native['generated_headers'])==2,'Both generated configuration headers must be retained')
core=next(row for row in load(sidecar)['native_libraries'] if row['path'].endswith('/libolex2core_graphics.so'));require(len(core['exports'])==367,'Explicit core ABI differs')
require(not any(row['path'].endswith(('png_texture_smoke.cpp','original_texture_manager_mocked.cpp','pixel_store_scope.cpp')) for row in native['compiled_sources']),'Test/mock sources must not enter the HAP')
require(sha(ROOT/'out/app/entry-default-unsigned.hap')=='d1ef66c779452135d803644ce62a47d672702f58fb3274d7582df98aeb830552','Standard production HAP changed')
startup=load(a.startup_report);require(startup['identity']==identity and startup['hap']['sha256']==sha(hap) and startup['texture_backend']=='libpng-unassociated-rgb','Startup evidence is not from this candidate')
require(startup['target']=='127.0.0.1:5555' and len(startup['runs'])==12 and startup['passed'] is True and all(row['passed'] for row in startup['runs']),'Expected all fixed twelve real starts')
fixtures=load(ROOT/'scripts/tests/fixtures/gx_synthetic_images.json')['fixtures'];startup_resources=[];hooks=[]
for row in startup['runs']:
 path=a.startup_report.parent/f"run-{row['run']}"/'native-result.json';require(sha(path)==row['native_result_sha256']==row['target_sha256'],'Startup artifact bytes changed');raw=load(path)
 require(raw['pid']==row['pid'] and raw['execution_completed'] is True,'Startup PID differs')
 validate_lifecycle(raw['lifecycle']);validate_no_decoder_control(raw['no_decoder_control'],'before-first-image-api');validate_no_decoder_control(raw['post_decode_control'],'after-capability-probes')
 validate_image_observations(raw['images'],fixtures,require_resources=True,resource_proof='owned-memory')
 hooks.append(validate_png_texture_hook(raw['texture_hook'],fixtures,require_resources=True,resource_proof='owned-memory'))
 startup_resources.append(inspect_owned_resource_observations(raw,proof='owned-memory',texture_backend='libpng-unassociated-rgb'))
cpu=load(a.cpu_report);require(cpu['passed'] is True and cpu['target']==startup['target'] and len(cpu['runs'])==3,'Expected three actual CPU target runs')
for row in cpu['runs']:
 path=a.cpu_report.parent/f"run-{row['run']}-png-observations.json";require(sha(path)==row['observation_sha256']==row['target_sha256'],'CPU target observation bytes changed');validate_png_observations(load(path))
python_text=a.python_log.read_text();counts=re.findall(r'Ran (\d+) tests in [^\n]+\n\nOK\s*$',python_text);require(len(counts)==1,'Full Python regression has not passed')
ui=dict(status='not-yet-qualified',current_candidate_full_194_pass_claimed=False)
if a.ui_report:
 app=load(a.ui_report);require(app['hap']['sha256']==sha(hap),'UI report HAP differs');qualification=validate_application(app,source,identity)
 ui=dict(status='passed-limited-diagnostic-ui',current_candidate_full_194_pass_claimed=True,report=evidence(a.ui_report),qualification=qualification)
if a.ui_collection:
 require(a.ui_report is not None,'A UI collection requires its full UI report');col=load(a.ui_collection)
 require(col['passed'] is True and col['resource_proof']=='owned-memory' and col['texture_backend']=='libpng-unassociated-rgb','UI process PNG collection did not pass')
 require(col['application']['identity']==identity,'UI collection identity differs');ui['platform_collection']=evidence(a.ui_collection);ui['platform_processes']=len(col['processes'])
status_path=ROOT/'reports/gx-application-platform-status.json';old=load(status_path);history=ROOT/'reports/gx-platform-history';history.mkdir(exist_ok=True)
archive=history/'before-png-rgb-status-20261109.json'
if not archive.exists():shutil.copy2(status_path,archive)
remaining=[]
if not a.ui_report:remaining.append('full 194 UI regression for the current PNG candidate')
if not a.ui_collection:remaining.append('strict PNG/platform collection from every current UI process')
remaining.extend(['original LoadTextures actual GPU upload/readback and complete context/state restoration','1D texture creation route','fonts, persistent original scene/context/renderer and sole long-lived runtime owner','complete original UI/resources/Python/plugins','physical PC/input, formal signing and release qualification'])
result=dict(kind='original-gx-platform-connection-evaluation',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
 production_baseline=old['production_baseline'],
 current_build_candidate=dict(identity=identity,source=str(source.relative_to(ROOT)),hap=evidence(hap),provenance=evidence(sidecar),verification=proof,source_patches=len(lock['patches']),actual_compiled_units=len(native['compiled_sources']),actual_upstream_cpp=sum(row['path'].startswith('upstream/') and row['path'].endswith('.cpp') for row in native['compiled_sources']),canonical_exports=len(core['exports']),generated_configuration_headers=native['generated_headers'],mock_or_test_writer_packaged=False),
 target=dict(target=startup['target'],connected_for_current_startup_audit=True,device_identity=startup['device_identity'],same_existing_emulator='MateBook Pro',data_reset=False,agreement_auto_accepted=False,last_verified_install_version=identity['versionName']),
 startup_audit=dict(report=evidence(a.startup_report),actual_starts=12,passed=12,failed=0,process_fd_equality_check_preserved=True,complete_descriptor_metadata_scan_passed=False,
  known_owned_scope_passed=12,observed_image_scopes=sum(row['observed_image_scopes'] for row in startup_resources),owned_input_files_closed=sum(row['owned_input_files_closed'] for row in startup_resources),successful_public_image_handle_release_results=sum(row['successful_public_handle_release_results'] for row in startup_resources),transparent_hidden_rgb_images=sum(row['transparent_hidden_rgb_images'] for row in hooks),observed_libpng_callback_allocations=sum(row['observed_libpng_callback_allocations'] for row in hooks),all_sdk_internal_allocations_proven_released=False),
 png_cpu=dict(report=evidence(a.cpu_report),actual_target_runs=3,checks_per_run=624,images_per_run=56,hidden_rgb_images_per_run=34,adam7_images_per_run=29,source_vendor_files=885,imagekit_executed=False,gpu_executed=False,hap_integration_evidence_is_separate=True),
 current_ui=ui,full_metadata_scan=old['full_metadata_scan'],texture_transfer_remediation=old['texture_transfer_remediation'],
 tests=dict(python=int(counts[0]),full_python_log=evidence(a.python_log),standard_evidence_consistency=329,baseline_host_ctest=18,model_cases_are_not_target_observations=True),
 historical_evidence=dict(before_png_status=evidence(archive),candidate8_startup=evidence(ROOT/'reports/emulator/gx-startup-audit-20261109-213518-b49c4528/result.json'),candidate8_startup_passed=5,candidate8_startup_failed=7,candidate8_full_ui=evidence(ROOT/'out/gx-application-evaluation/source8/reports/app/20261109-213634-f47f68/result.json'),candidate8_full_ui_checks=194,candidate5_same_batch_revalidation=evidence(ROOT/'reports/emulator/gx-startup-revalidation-20261109-204605-999385d4/result.json'),candidate5_passed=9,candidate5_failed=3,old_failures_removed=False,old_fd_mismatch_root_cause_proven=False),
 remaining=remaining,updated_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'))
result['evidence']=[evidence(ROOT/name) for name in ('scripts/build_provenance_png.py','scripts/png_texture_hook_validation.py','scripts/png_texture_validation.py','scripts/run-gx-startup-audit.py','scripts/collect-gx-platform-evaluation.py','scripts/run-png-texture-smoke.py','scripts/record-gx-png-progress.py','experiments/gx-png-image/vendor.lock.json','out/gx-application-evaluation/png-host-ctest-final.log','out/gx-application-evaluation/status-consistency-png-attempt1.log')]
status_path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status='in_progress',passed=False,candidate=identity['versionName'],startup='12/12',ui=ui['status'],python=int(counts[0]),remaining=len(remaining)),ensure_ascii=False))
