#!/usr/bin/env python3
"""Independently record21 native+vector font qualification; preserve20 failure and19 passes."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import unittest
import uuid
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from vector_font_state_progress_validation import revalidate_window
from vector_font_state_window_validation import IDENTITY
from native_font_progress_validation import ROOT,bound,check_bound,validate_disassembly,require,canonical


def validate_audit_arguments(a):
 require(bool(a.app_report)==bool(a.ui_tools),'Supply both completed UI report and matching tools')
 require(bool(a.platform_report)==bool(a.startup_report),'Supply both current platform and startup reports')
 require(not a.platform_report or bool(a.app_report),'Platform/startup qualification requires completed current UI')
 require(not getattr(a,'mixed_report',None) or bool(a.platform_report),'Mixed qualification requires current UI/platform/startup evidence')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('source','hap','font-report','binary-proof'):p.add_argument('--'+name,type=Path,required=True)
 for name in ('python-log','app-report','ui-tools','platform-report','startup-report','mixed-report'):p.add_argument('--'+name,type=Path)
 a=p.parse_args()
 validate_audit_arguments(a)
 source,hap=a.source.resolve(),a.hap.resolve()
 require(package_identity(hap)==IDENTITY,'Only exact21 accepted')
 verification=verify(hap,hap.with_suffix('.provenance.json'),source);package=bound(hap)
 font=revalidate_window(a.font_report,package['sha256'],source)
 proof=json.loads(a.binary_proof.read_text());require(proof['identity']=='0.22.21' and proof['hap']==package,'Actual21 raster proof differs')
 for name in ('hap','provenance','unstripped_source_build_output','disassembly'):check_bound(proof[name])
 require(proof['provenance']==bound(hap.with_suffix('.provenance.json')),'Binary proof sidecar differs')
 library=check_bound(proof['unstripped_source_build_output']);require(library.is_relative_to(hap.parent/'stage'),'Binary proof is not this captured source build')
 command=['/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-objdump','--disassemble','--start-address='+str(proof['function_start']),'--stop-address='+str(proof['function_end']),str(library)]
 fresh=subprocess.run(command,capture_output=True,text=True,check=True).stdout
 binary=validate_disassembly(check_bound(proof['disassembly']).read_text(),fresh,proof['function_start'],proof['function_end'])
 tests=dict(complete_current_python_suite_verified=False)
 if a.python_log:
  log=a.python_log.read_text();matched=re.search(r'Ran (\d+) tests in ',log);suite=unittest.defaultTestLoader.discover(str(ROOT/'scripts/tests'),pattern='test_*.py')
  require(matched and log.rstrip().endswith('OK') and int(matched[1])==suite.countTestCases(),'Current full Python suite did not pass')
  tests.update(complete_current_python_suite_verified=True,python_cases=int(matched[1]),python_log=bound(a.python_log))
 host=ROOT/'out/gx-application-evaluation/vector-font20-description-host-ctest-attempt1.log';require('100% tests passed, 0 tests failed out of 3' in host.read_text(),'Same unchanged font components lack host regression')
 observation=ROOT/'out/gx-application-evaluation/vector-font20-description-host-observation.log';require('PASS 7584 isolated descriptor/size assertions' in observation.read_text(),'Descriptor assertion log differs')
 for name in ('font_description.h','platform_font_factory.cpp','platform_font_factory.h','original_vector_font_window_probe.cpp','original_vector_font_window_probe.h'):
  require((source/'native'/name).read_bytes()==(ROOT/'out/gx-application-evaluation/source20/native'/name).read_bytes(),'21 is not unchanged20 native font implementation: '+name)
 tests.update(shared_font_host_ctest_cases=3,descriptor_size_assertions=7584,host_models_packaged=False,host_is_GPU_evidence=False,host_log=bound(host),descriptor_log=bound(observation))
 ui=dict(status='not-yet-qualified-for-candidate21',inherited_candidate19_or20_pass=False)
 if a.app_report:
  from vector_font_state_application_validation import validate_application
  summary=canonical(validate_application(json.loads(a.app_report.read_text()),a.ui_tools,IDENTITY))
  ui=dict(status='passed-limited-212-check-diagnostic-ui',report=bound(a.app_report),qualification=summary,inherited_candidate19_or20_pass=False)
 qualification=None
 if a.platform_report:
  from vector_font_state_current_qualification import validate_current_qualifications
  qualification=validate_current_qualifications(a.app_report,ui['qualification'],a.platform_report,a.startup_report,package['sha256'])
 mixed=None
 if a.mixed_report:
  from vector_font_state_mixed_progress_validation import validate_mixed_report
  mixed=validate_mixed_report(a.mixed_report,package['sha256'],source)
 now=datetime.now(ZoneInfo('Asia/Shanghai'));directory=ROOT/'reports/emulator'/('vector-font-state-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);directory.mkdir()
 paths=[ROOT/'reports'/(name+'.json') for name in ('gx-font-status','gx-application-platform-status','gx-persistent-runtime-status','gx-texture-load-status','gx-texture-load-route-status')]
 prior=json.loads(paths[0].read_text());require('qualified_candidate19_before_vector' in prior['history'],'Preserve the qualified19 archive')
 history=dict(prior['history']);archives={}
 for path in paths:
  archive=directory/('before-'+path.name);archive.write_bytes(path.read_bytes());archives[path.stem]=bound(archive)
 history['before_this_vector21_record']=archives
 native_report=directory/'independent-font-revalidation.json';native_report.write_text(json.dumps(font,ensure_ascii=False,indent=2)+'\n');(directory/'actual-raster-disassembly.log').write_text(fresh)
 masks=[m for row in font['rounds'] for m in row['mask_observations']];require(len(masks)==24 and all(m['actual_integer']==[1]*4 for m in masks),'All24 actual driver integer observations required')
 old_failure=ROOT/'reports/emulator/original-vector-font-window-20261110-032543-d01cd356/result.json';old_raw=json.loads(old_failure.read_text());require(old_raw['passed'] is False and len([c for c in old_raw['rounds'][0]['checks'] if not c['passed']])==8,'Preserve original20 eight-gate failure')
 record=dict(kind='original-native-and-vector-font-limited-qualification',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
  production_baseline=prior['production_baseline'],current_build_candidate=dict(identity=IDENTITY,source=source.relative_to(ROOT).as_posix(),hap=package,verification=verification,actual_compiled_native_sources=verification['compiled_native_sources'],upstream_cpp_compiled=379,patches=20,explicit_core_exports=367,promotion='Exact21 original native+vector font windows qualified; all other current-version gates separate'),
  target=dict(target=font['target'],same_existing_emulator='MateBook Pro',device_identity=font['device_identity'],last_verified_install_version='0.22.21',last_observed_font_pid=font['pid'],data_reset=False,agreement_auto_accepted=False),
  current_ui=ui,platform_image_audit=dict(status='not-yet-qualified-for-candidate21'),startup_audit=dict(status='not-yet-qualified-for-candidate21'),mixed_routes=dict(status='not-yet-qualified-for-candidate21',required_exact_requests_per_font_action=6),
  font_window=dict(status='passed-limited-native-ASCII-and-original-vector-window',report=bound(native_report),actual_rounds=3,independent_checks=font['checks'],actual_context_continuations=2,native_point_sizes=[18,27,18],original_vector_description='@18',original_vector_point_size_preserved=18,vector_draw_scales=[1200,1200,1800,1200],vector_real_source_glyphs=95,shared_GPU_vector_reference=True,independent_GPU_line_rasterization_verified=False,complete_font_support=False,raster_binary=binary),
  mask_observations=dict(actual_integer_queries=24,integer_all_color_channels_enabled=True,legacy_boolean_integer_disagreements=sum(not m['boolean_integer_agree'] for m in masks),raw_boolean_values_preserved=True,SDK_modified=False,GL4ES_modified=False,scope='Observed driver query disagreement, not a diagnosis of its internal root cause'),tests=tests,history=history,
  remaining=['本版自己的212诊断UI/全UI PID资源/固定12启动/三路混合（每次字体请求由3变6，不能套旧guard）','完整字体资源/字体族样式选择/原字体对话框/DPI/广泛字符与异常恢复','完整原UI/菜单/命令/文档/资源/Python/插件及P1～P8','真机/物理输入/正式签名发行'],evidence=[bound(a.font_report),bound(a.binary_proof),bound(native_report),bound(old_failure),bound(directory/'actual-raster-disassembly.log'),bound(host),bound(observation)],updated_at=now.isoformat(timespec='seconds'))
 if a.app_report:
  record['remaining'][0]='本版全UI PID资源/固定12启动/三路混合（字体每动作6次请求）'
 if qualification:
  independent=directory/'independent-platform-startup-revalidation.json'
  independent.write_text(json.dumps(qualification,ensure_ascii=False,indent=2)+'\n')
  record['platform_image_audit']=qualification['platform_image_audit']
  record['startup_audit']=qualification['startup_audit']
  record['target']['last_verified_startup_pid']=qualification['last_startup_pid']
  record['evidence']+=[bound(independent)]+qualification['evidence']
  record['remaining'][0]='本版三路混合（字体每动作6次请求，新增vector与整数写掩码检查完整保留）'
 if mixed:
  independent_mixed=directory/'independent-mixed-revalidation.json'
  independent_mixed.write_text(json.dumps(mixed,ensure_ascii=False,indent=2)+'\n')
  record['mixed_routes']=dict(status='passed-limited-native-vector-three-route-interleaving',
   report=bound(independent_mixed),original_report=mixed['raw_report'],actual_actions=18,pids=mixed['pids'],
   checks=mixed['counters'],required_exact_requests_per_font_action=6,exact_font_requests_per_pid=18,
   all_six_directed_route_pairs_covered=True,direct_load_font_counter_available=False,
   vector_reference_shares_GPU_backend=True,inherited_prior_candidate_pass=False)
  record['target']['mixed_window_pids']=mixed['pids']
  record['evidence']+=[bound(independent_mixed)]+mixed['evidence']
  record['remaining'][0]='完整纹理格式/mipmap/异常恢复和原应用图形资源流程'
 for name in ('vector_font_state_mixed_validation.py','vector_font_state_mixed_progress_validation.py','vector_font_state_current_qualification.py','vector_font_state_tool_profiles.py','vector_font_state_application_validation.py','vector_font_state_startup_validation.py','record-vector-font-state-progress.py','vector_font_state_progress_validation.py','vector_font_state_window_validation.py','original_vector_font_window_validation.py','native_font_raster_window_validation.py','native_font_window_validation.py'):
  file=ROOT/'scripts'/name;(directory/name).write_bytes(file.read_bytes());record['evidence'].append(bound(directory/name))
 if a.app_report:record['evidence']+=[bound(a.app_report),bound(a.ui_tools/'tool-profile.json')]
 if a.python_log:record['evidence'].append(bound(a.python_log))
 for path in [directory/'result.json',ROOT/'reports/gx-vector-font-status.json']+paths:path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(status='in_progress',version='0.22.21',font_rounds=3,independent_checks=font['checks'],mask_integer_observations=24,current_ui=ui['status'],report_directory=str(directory),full_port_complete=False),ensure_ascii=False,indent=2))

if __name__=='__main__':main()
