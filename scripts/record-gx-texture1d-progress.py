#!/usr/bin/env python3
"""Bind limited current-window 1D evidence without claiming complete adaptation."""
import argparse,hashlib,json,re,shutil
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from texture_1d_application_validation import validate_application
from gx_application_validation import validate_lifecycle,validate_image_observations,inspect_owned_resource_observations,validate_no_decoder_control
from png_texture_hook_validation import validate_png_texture_hook
ROOT=Path(__file__).resolve().parents[1]
def require(v,message):
 if not v:raise ValueError(message)
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def evidence(p):
 p=Path(p).resolve();require(p.is_relative_to(ROOT),'Evidence must stay in this project')
 return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=sha(p))
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','build','ui-report','ui-collection','startup-report','python-log','patch-report'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();source=a.source.resolve();build=a.build.resolve();hap=build/'entry-default-unsigned.hap';sidecar=build/'entry-default-unsigned.provenance.json'
identity=package_identity(hap);require(identity['versionName']=='0.22.11' and identity['versionCode']==221100,'Only this exact candidate is supported')
proof=verify(hap,sidecar,source);lock=load(source/'source.lock.json');native=load(sidecar)['native'];core=next(r for r in load(sidecar)['native_libraries'] if r['path'].endswith('/libolex2core_graphics.so'))
require(len(lock['patches'])==19 and len(core['exports'])==367 and len(native['compiled_sources'])==601,'Exact candidate inventory changed')
require(len(native['generated_headers'])==2,'Both generated PNG configuration headers must be retained')
require(any(r['path']=='native/window_texture_1d_probe.cpp' for r in native['compiled_sources']),'Current-window adapter was not compiled')
require(not any('mocked' in r['path'] or r['path'].endswith(('pixel_store_scope.cpp','png_texture_smoke.cpp')) for r in native['compiled_sources']),'Test-only code must not enter the package')
require(sha(ROOT/'out/app/entry-default-unsigned.hap')=='d1ef66c779452135d803644ce62a47d672702f58fb3274d7582df98aeb830552','Production baseline changed')
app=load(a.ui_report);require(app['hap']['sha256']==sha(hap),'UI belongs to another HAP');ui=validate_application(app,source,identity)
collection=load(a.ui_collection);require(collection['passed'] is True and collection['target']=='127.0.0.1:5555' and collection['application']['sha256']==sha(a.ui_report) and collection['application']['hap_sha256']==sha(hap),'Platform collection is not bound to this UI/HAP')
# Re-use exactly the collector's explicit PID inventory, never glob old data.
processes=collection['processes'];require(processes and all(row['owned_resources']['passed'] is True for row in processes),'A required UI process was not collected')
fixtures=load(ROOT/'scripts/tests/fixtures/gx_synthetic_images.json')['fixtures']
for row in processes:
 directory=a.ui_collection.parent/('pid-'+str(row['pid']))
 for name,record in row['files'].items():
  require(Path(name).name==name and sha(directory/name)==record['sha256']==record['target_sha256'],'A received platform artifact changed')
 raw=load(directory/'result.json');require(raw['pid']==row['pid'] and raw['execution_completed'] is True,'Native PID observation differs')
 actual=dict(lifecycle=validate_lifecycle(raw['lifecycle']),image_capabilities=validate_image_observations(raw['images'],fixtures,require_resources=True,resource_proof='owned-memory'),owned_resources=inspect_owned_resource_observations(raw,proof='owned-memory',texture_backend='libpng-unassociated-rgb'),no_decoder_control=validate_no_decoder_control(raw['no_decoder_control'],'before-first-image-api'),post_decode_control=validate_no_decoder_control(raw['post_decode_control'],'after-capability-probes'),texture_hook=validate_png_texture_hook(raw['texture_hook'],fixtures,require_resources=True,resource_proof='owned-memory'))
 actual=json.loads(json.dumps(actual,ensure_ascii=False,allow_nan=False))
 for key,value in actual.items():require(row[key]==value,'Platform validator output changed: '+key)
startup=load(a.startup_report);require(startup['identity']==identity and startup['hap']['sha256']==sha(hap) and startup['target']=='127.0.0.1:5555','Startup audit belongs to another HAP/target')
require(startup['texture_backend']=='libpng-unassociated-rgb' and startup['owned_resource_audit_required'] is True and startup['resource_proof']=='owned-memory' and startup['post_decode_control_required'] is True,'Original PNG/resource controls must remain required')
require(len(startup['runs'])==12,'The fixed twelve-run startup audit must reach its terminal state')
for row in startup['runs']:
 path=a.startup_report.parent/f"run-{row['run']}"/'native-result.json'
 require(sha(path)==row['native_result_sha256']==row['target_sha256'],'A startup observation changed')
 raw=load(path);require(raw['pid']==row['pid'] and raw['execution_completed'] is True,'Startup PID differs')
 validate_lifecycle(raw['lifecycle']);validate_no_decoder_control(raw['no_decoder_control'],'before-first-image-api');validate_no_decoder_control(raw['post_decode_control'],'after-capability-probes')
 validate_image_observations(raw['images'],fixtures,require_resources=True,resource_proof='owned-memory');validate_png_texture_hook(raw['texture_hook'],fixtures,require_resources=True,resource_proof='owned-memory')
text=a.python_log.read_text();m=re.findall(r'Ran (\d+) tests in [^\n]+\n\nOK\s*$',text);require(len(m)==1 and int(m[0])>=558,'The full Python suite, including new verifier tests, has not passed')
patch=load(a.patch_report);require(patch['passed'] is True and len(patch['checks'])==143 and all(c['passed'] for c in patch['checks']),'Exact 19-patch replay failed')
for name,count in [('candidate11-original-verifier-negative-tests.log',26),('candidate11-application-evidence-tests.log',12),('picker-readiness-tests.log',8)]:
 text=(ROOT/'out/gx-application-evaluation'/name).read_text()
 require(re.search(r'Ran '+str(count)+r' tests in [^\n]+\n\nOK\s*$',text) is not None,'A required verifier test log has not passed: '+name)
for name,count in [('production-host-ctest-1d.log',18),('texture-1d-pinned-host-ctest.log',1),('texture-2d-after-1d-ctest.log',2)]:
 require('100% tests passed, 0 tests failed out of '+str(count) in (ROOT/'out/gx-application-evaluation'/name).read_text(),'A required host CTest has not passed: '+name)
host=load(ROOT/'out/gx-application-evaluation/texture-1d-host-observations.json')
require(host['passed'] is True and host['checks']==830 and host['real_gpu_executed'] is False,'Host capture must stay separate from actual GPU evidence')
standard=load(ROOT/'reports/adaptation-consistency.json');require(standard['passed'] is True and len(standard['checks'])==329,'Standard consistency gate failed')
status_path=ROOT/'reports/gx-application-platform-status.json';old=load(status_path);require(old['status']=='in_progress' and old['full_port_complete'] is False,'Unexpected prior scope')
archive=ROOT/'reports/gx-application-platform-before-texture1d-status.json'
if not archive.exists():shutil.copy2(status_path,archive)
old=load(archive)
passed=sum(row['passed'] is True for row in startup['runs']);all_starts=startup['passed'] is True and passed==12
result=dict(kind='original-gx-platform-connection-evaluation',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
 production_baseline=old['production_baseline'],
 current_build_candidate=dict(identity=identity,source=source.relative_to(ROOT).as_posix(),hap=evidence(hap),provenance=evidence(sidecar),verification=proof,source_patches=19,actual_compiled_units=601,actual_upstream_cpp=sum(r['path'].startswith('upstream/') and r['path'].endswith('.cpp') for r in native['compiled_sources']),canonical_exports=367,generated_configuration_headers=native['generated_headers'],mock_or_test_writer_packaged=False),
 target=dict(target='127.0.0.1:5555',same_existing_emulator='MateBook Pro',device_identity=startup['device_identity'],data_reset=False,agreement_auto_accepted=False,last_verified_install_version=identity['versionName']),
 current_ui=dict(status='passed-limited-1d-extended-diagnostic-ui',report=evidence(a.ui_report),qualification=ui,platform_collection=evidence(a.ui_collection),platform_processes=[r['pid'] for r in processes]),
 startup_audit=dict(report=evidence(a.startup_report),actual_starts=12,passed=passed,failed=12-passed,all_twelve_passed=all_starts,process_fd_equality_check_preserved=True,complete_descriptor_metadata_scan_passed=False,all_sdk_internal_allocations_proven_released=False),
 texture_1d=dict(limited_current_window_verified=True,cases_per_round=4,resource_generations_per_case=2,rounds=6,independent_checks_per_round=64,total_independent_checks=384,actual_context_migration_verified=False,persistent_gx_owner_implemented=False,level=0,border=0,formats=['RGB','RGBA'],tested_gpu_widths=[3,5],host_mock_only_checks=830,host_mocks_packaged=False,third_party_vendor_modified=False),
 png_cpu=old['png_cpu'],full_metadata_scan=old['full_metadata_scan'],texture_transfer_remediation=old['texture_transfer_remediation'],
 tests=dict(python=int(m[0]),full_python_log=evidence(a.python_log),standard_host_ctest=18,one_dimensional_host_ctest=1,retained_two_dimensional_host_ctest=2,original_window_negative_tests=26,texture_window_validator_tests=23,application_evidence_tests=12,picker_readiness_tests=8,exact_patch_replay=evidence(a.patch_report),standard_evidence_consistency=329),
 historical_evidence=dict(before_texture1d_status=evidence(archive),png_candidate_full_ui=old['current_ui'],candidate10_first_ui_failure=evidence(ROOT/'out/gx-application-evaluation/source10/reports/app/20261109-223619-063580/result.json'),candidate11_original_shader_count_failure=evidence(ROOT/'out/gx-application-evaluation/source11/reports/app/20261109-224134-7bc4cd/result.json'),candidate11_missing_host_reference_failure=evidence(ROOT/'out/gx-application-evaluation/source11/reports/app/20261109-224331-f81d69/result.json'),candidate11_picker_focus_failure=evidence(ROOT/'out/gx-application-evaluation/source11/reports/app/20261109-224805-df0620/result.json'),old_failures_removed=False,old_fd_mismatch_root_cause_proven=False),
 remaining=['original TGXApp::LoadTextures actual GPU upload/readback and complete context/state restoration','1D actual context migration, mipmap and remaining sampler/format qualification outside tested level-0 synthetic cases','fonts, persistent original scene/context/renderer and sole long-lived runtime owner','complete original UI/resources/Python/plugins','physical PC/input, formal signing and release qualification'],
 updated_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'))
result['evidence']=[evidence(ROOT/name) for name in ('scripts/prepare-texture-1d-candidate.py','scripts/texture_1d_window_validation.py','scripts/texture_1d_application_validation.py','scripts/collect-gx-texture1d-evaluation.py','scripts/record-gx-texture1d-progress.py','scripts/tests/test_texture_1d_window_validation.py','scripts/picker_readiness.py','scripts/tests/test_picker_readiness.py','experiments/gx-texture-1d/native/window_texture_1d_probe.cpp','experiments/gx-application/patches/0019-texture-1d-upload-and-target-aware-recreation.patch','out/gx-application-evaluation/production-host-ctest-1d.log','out/gx-application-evaluation/texture-2d-after-1d-ctest.log','out/gx-application-evaluation/texture-1d-pinned-host-ctest.log','out/gx-application-evaluation/candidate11-application-evidence-tests.log','out/gx-application-evaluation/candidate11-original-verifier-negative-tests.log','out/gx-application-evaluation/picker-readiness-tests.log','out/gx-application-evaluation/texture-1d-host-observations.json','out/gx-application-evaluation/texture-1d-generator-replay-final.log')]
status_path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status='in_progress',full_port_complete=False,candidate=identity['versionName'],ui=200,original_window_checks=756,texture_window_checks=384,ui_processes=len(processes),startup_passed=passed,startup_failed=12-passed,python=int(m[0])),ensure_ascii=False,indent=2))
