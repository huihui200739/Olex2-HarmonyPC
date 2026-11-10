#!/usr/bin/env python3
"""Record actual partial GX/image/texture evidence, never invent a green port.

This is an evidence closeout tool, not a build, target run or certification.
Inputs are explicit, and model/mocked tests stay separate from real target data.
"""
import argparse,hashlib,json,re,shutil
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from renderer_probe_identity import package_identity
ROOT=Path(__file__).resolve().parents[1];EVAL=ROOT/'out/gx-application-evaluation'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,required=True);p.add_argument('--build',type=Path,required=True)
p.add_argument('--python-log',type=Path,required=True);a=p.parse_args()
def require(v,m):
 if not v:raise ValueError(m)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def evidence(p):
 p=Path(p).resolve();require(p.is_relative_to(ROOT),'Evidence outside this project')
 return dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p))
def load(p):return json.loads(Path(p).read_text())
source=a.source.resolve();build=a.build.resolve();require(source.is_relative_to(EVAL) and build.is_relative_to(EVAL),'Use this project candidate')
hap=build/'entry-default-unsigned.hap';provenance=build/'entry-default-unsigned.provenance.json';proof=load(provenance);lock=load(source/'source.lock.json')
require(proof['hap']['sha256']==sha(hap),'Candidate HAP differs from its actual build')
identity=package_identity(hap);require(identity['bundleName']=='com.example.olex2harmonypc','Wrong evaluation bundle')
require(identity['versionName']=='0.22.8','Historical disconnected-stage recorder: use record-gx-png-progress.py for the PNG stage')
compiled=proof['native']['compiled_sources'];core=next(x for x in proof['native_libraries'] if x['path'].endswith('/libolex2core_graphics.so'))
require(len(lock['patches'])==18 and len(core['exports'])==367,'Unexpected candidate source lock/export set')
require(not any(x['path'].endswith(('pixel_store_scope.cpp','original_texture_manager_mocked.cpp')) for x in compiled),'Mock backend entered the application')
texture=next(x for x in compiled if x['path']=='upstream/glib/gltexture.cpp');require(texture['sha256']==sha(source/'upstream/glib/gltexture.cpp'),'Compiled original texture manager differs')
header=next(x for x in proof['native']['dependencies'] if x['path']=='project/upstream/glib/glpixelstorescope.h');require(header['sha256']==sha(source/'upstream/glib/glpixelstorescope.h'),'Transfer guard was not an actual compiler dependency')
standard=ROOT/'out/app/entry-default-unsigned.hap';standard_sha='d1ef66c779452135d803644ce62a47d672702f58fb3274d7582df98aeb830552'
require(sha(standard)==standard_sha and len(load(ROOT/'source.lock.json')['patches'])==16,'Production baseline changed')
python_text=a.python_log.read_text();counts=re.findall(r'Ran (\d+) tests in [^\n]+\n\nOK\s*$',python_text)
require(len(counts)==1,'Full Python run has not completed successfully')
mock_manager=load(EVAL/'texture-transfer-host-final-manager-result.json');mock_scope=load(EVAL/'texture-transfer-host-final-scope-result.json')
for result in (mock_manager,mock_scope):require(result['passed'] is True and result['real_gpu_executed'] is False,'Mock scope was misrepresented')
startup_path=ROOT/'reports/emulator/gx-startup-revalidation-20261109-204605-999385d4/result.json';startup=load(startup_path)
require(startup['new_runtime_observation_performed'] is False and len(startup['runs'])==12,'Changed original cold-start membership')
require(sum(row['passed'] for row in startup['runs'])==9 and all(row['owned_resources']['passed'] for row in startup['runs']),'Unexpected prior startup evidence')
old_status=ROOT/'reports/gx-application-platform-status.json';old=load(old_status)
history=ROOT/'reports/gx-platform-history';history.mkdir(exist_ok=True)
archive=history/'before-owned-memory-and-transfer-status.json'
if not archive.exists():shutil.copy2(old_status,archive)
connection=EVAL/'target-connection-after-build8.log';require('[Fail][E001005]' in connection.read_text(),'Refresh current target state before changing its recorded condition')
interrupted=EVAL/'source5/reports/app/20261109-204541-d2a818';require(not (interrupted/'result.json').exists(),'Interrupted UI evidence now needs re-evaluation')
launch=EVAL/'emulator-restart-preserving-data-20261109.log';require('Please agree to the agreement first' in launch.read_text(),'Emulator launch condition changed')
remaining=['target cold-start and full 194 UI regression for current candidate',
 'unchanged process fd equality still failed in 3 of 12 retained 0.22.5 starts; attribute and verify post-decode interval',
 'original LoadTextures actual GPU upload/readback and context restoration',
 'transparent PNG hidden-RGB fidelity', '1D texture creation route',
 'fonts, persistent original scene/context/renderer and sole runtime owner upgrade',
 'complete original UI/resources/Python/plugins and physical-PC/input/signing/release qualification']
result=dict(kind='original-gx-platform-connection-evaluation',status='in_progress',passed=False,milestone_complete=False,
 full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
 production_baseline=dict(version='0.22.0',source_patches=16,canonical_exports=365,hap_sha256=standard_sha,source_and_standard_package_preserved=True),
 current_build_candidate=dict(identity=identity,hap=evidence(hap),provenance=evidence(provenance),source=str(source.relative_to(ROOT)),
  source_patches=len(lock['patches']),native_units=proof['native']['object_count'],upstream_cpp=len({r['path'] for r in compiled if r['path'].startswith('upstream/') and r['path'].endswith('.cpp')}),
  explicit_exports=len(core['exports']),compiled_texture_manager=evidence(source/'upstream/glib/gltexture.cpp'),compiled_transfer_guard=evidence(source/'upstream/glib/glpixelstorescope.h'),
  mock_backend_packaged=False,target_runtime_performed=False,full_ui_regression_performed=False,compilation_numbers_are_not_functional_completion=True),
 target=dict(address='127.0.0.1:5555',current_connection_verified=False,last_verified_install_version='0.22.5',
  connection_failure=evidence(connection),same_existing_emulator='MateBook Pro',emulator_restart_requested_without_reset=True,
  restart_succeeded=False,agreement_auto_accepted=False,requires_user_software_agreement_confirmation=True,launch_log=evidence(launch)),
 startup_audit=dict(report=evidence(startup_path),same_original_runtime_starts=12,new_starts_claimed_by_revalidation=0,
  passed=9,failed=3,overall_passed=False,failed_runs=[r['run'] for r in startup['runs'] if not r['passed']],
  known_owned_scope_passed=12,observed_image_scopes=sum(r['owned_resources']['observed_image_scopes'] for r in startup['runs']),
  owned_input_files_closed=sum(r['owned_resources']['owned_input_files_closed'] for r in startup['runs']),
  successful_public_image_handle_release_results=sum(r['owned_resources']['successful_public_handle_release_results'] for r in startup['runs']),
  complete_descriptor_metadata_scan_passed=False,all_sdk_internal_allocations_proven_released=False,
  validation_tool_shadowing_error_corrected_offline=True,old_raw_failure_report=evidence(ROOT/'reports/emulator/gx-startup-audit-20261109-204400-ca3e0512/result.json')),
 full_metadata_scan=dict(status='unavailable-for-some-existing-process-descriptors',fstat_errno_observed=13,
  old_failure_preserved=True,old_closed_during_snapshot_label_not_accepted_as_fact=True,permissions_or_identity_changed=False),
 texture_transfer_remediation=dict(candidate_patch=18,packed_2d_pack_unpack_state_scoped=True,row_length_and_skips_cleared_then_restored=True,
  replacement_reuses_original_id_and_sampler_state=True,unsupported_format_rejected_before_gpu_mutation=True,
  host_actual_original_methods_backend='explicitly mocked GL, never packaged',manager_checks=mock_manager['checks'],scope_checks=mock_scope['checks'],
  mock_upload_calls=mock_manager['uploads'],mock_readback_calls=mock_manager['readbacks'],real_gpu_executed=False,
  full_gl_state_restoration_proven=False,one_dimensional_textures_verified=False),
 tests=dict(python=int(counts[0]),full_python_log=evidence(a.python_log),baseline_host_ctest=18,additional_platform_cpu_ctest=3,
  additional_texture_transfer_mock_ctest=2,model_cases_are_not_target_observations=True),
 interrupted_ui=dict(candidate_version='0.22.5',report_directory=str(interrupted.relative_to(ROOT)),complete_result_present=False,
  full_194_pass_not_claimed=True,interruption_record=evidence(EVAL/'interruption-state-20261109-2050.json')),
 historical_verified_ui=old.get('verified_previous_candidate',old.get('historical_verified_ui')),
 historical_source3_startup=dict(report=evidence(ROOT/'reports/emulator/gx-startup-audit-20261109-201758-88db7ee5/result.json'),passed=2,failed=10),
 remaining=remaining,updated_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'))
result['texture_transfer_remediation']['host_evidence']=evidence(ROOT/'reports/gx-texture-transfer-host-status.json')
result['evidence']=[evidence(p) for p in [EVAL/'patch-replay-candidate8.log',EVAL/'texture-transfer-host-final-ctest.log',EVAL/'texture-transfer-host-final-manager-result.json',
 EVAL/'texture-transfer-host-final-scope-result.json',EVAL/'platform-host-resource-ctest-attempt4.log',
 ROOT/'reports/emulator/image-memory-scope-20261109-204331-14f172be/result.json',EVAL/'production-host-ctest-memory.log',
 EVAL/'production-provenance-memory.log',EVAL/'production-source-memory.log',ROOT/'scripts/gx_application_validation.py',
 ROOT/'scripts/run-gx-startup-audit.py',ROOT/'scripts/collect-gx-platform-evaluation.py',ROOT/'scripts/revalidate-gx-startup-audit.py',ROOT/'scripts/gx_target_identity.py',
 ROOT/'reports/gx-texture-transfer-host-status.json']]
consistency=EVAL/'status-consistency-memory-after-docs.log'
if consistency.exists():
 text=consistency.read_text();require('Evidence consistency only;' in text and not any(line.startswith('FAIL ') for line in text.splitlines()),'Standard summary verification did not complete')
 result['tests']['standard_evidence_consistency']=sum(line.startswith('PASS ') for line in text.splitlines())
 result['evidence'].append(evidence(consistency))
old_status.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status=result['status'],passed=result['passed'],candidate=identity['versionName'],python=result['tests']['python'],remaining=len(remaining),production_preserved=True),ensure_ascii=False))
