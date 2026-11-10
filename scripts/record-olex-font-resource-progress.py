#!/usr/bin/env python3
"""Record only exact23 finite resource qualification; never inherit21 UI/startup passes."""
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
from olex_font_resource_progress_validation import revalidate_window
from olex_font_resource_window_validation import IDENTITY
from native_font_progress_validation import ROOT,bound,check_bound,validate_disassembly,require

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('source','hap','font-report','binary-proof','python-log','replay-report'):parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args();source,hap=args.source.resolve(),args.hap.resolve()
 require(package_identity(hap)==IDENTITY,'Only exact23 may be recorded')
 verification=verify(hap,hap.with_suffix('.provenance.json'),source);package=bound(hap)
 actual=revalidate_window(args.font_report,package['sha256'],source)
 proof=json.loads(args.binary_proof.read_text());require(proof['identity']=='0.22.23' and proof['hap']==package,'Wrong actual resource binary proof')
 for name in ('hap','provenance','unstripped_source_build_output','disassembly'):check_bound(proof[name])
 require(proof['provenance']==bound(hap.with_suffix('.provenance.json')),'Wrong source provenance sidecar')
 library=check_bound(proof['unstripped_source_build_output']);require(library.is_relative_to(hap.parent/'stage'),'Not this actual source-built DSO')
 command=['/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-objdump','--disassemble','--start-address='+str(proof['function_start']),'--stop-address='+str(proof['function_end']),str(library)]
 fresh=subprocess.run(command,capture_output=True,text=True,check=True).stdout
 binary=validate_disassembly(check_bound(proof['disassembly']).read_text(),fresh,proof['function_start'],proof['function_end'])
 log=args.python_log.read_text();matched=re.search(r'Ran (\d+) tests in ',log);count=unittest.defaultTestLoader.discover(str(ROOT/'scripts/tests'),pattern='test_*.py').countTestCases()
 require(matched and log.rstrip().endswith('OK') and int(matched[1])==count,'Full current Python suite did not pass')
 replay=json.loads(args.replay_report.read_text());require(replay['passed'] and [row['files'] for row in replay['replayed']]==[5,2519,2520] and all(row['all_original_bytes_equal'] for row in replay['replayed']),'Exact fixture22/23 byte replay missing')
 for row in replay['recipes']:check_bound(row)
 now=datetime.now(ZoneInfo('Asia/Shanghai'));directory=ROOT/'reports/emulator'/('olex-font-resource-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);directory.mkdir()
 names=('gx-vector-font-status','gx-font-status','gx-application-platform-status','gx-persistent-runtime-status','gx-texture-load-status','gx-texture-load-route-status')
 paths=[ROOT/'reports'/(name+'.json') for name in names];previous=json.loads(paths[0].read_text());archives={}
 for path in paths:
  retained=directory/('before-'+path.name);retained.write_bytes(path.read_bytes());archives[path.stem]=bound(retained)
 independent=directory/'independent-font-revalidation.json';independent.write_text(json.dumps(actual,ensure_ascii=False,indent=2)+'\n')
 (directory/'actual-raster-disassembly.log').write_text(fresh)
 masks=[]
 for row in actual['rounds']:
  folder=ROOT/row['folder']
  for case in row['vector_cases']:
   for route in ('actual','reference'):
    gpu=json.loads((folder/f'actual-gpu-run-{row["run"]}-vector-{case["label"]}-{route}.json').read_text())
    masks.append(dict(actual_integer=gpu['color_write_integer'],legacy_boolean=gpu['color_write'],raw_symbol_boolean=gpu['color_write_raw_boolean']))
  masks+=row['resource_mask_observations']
 require(len(masks)==33 and all(m['actual_integer']==[1]*4 for m in masks),'All33 actual driver integer snapshots required')
 history=dict(previous['history']);history['qualified_candidate21_before_resource']=archives
 record=dict(kind='original-font-resource-limited-qualification',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
  production_baseline=previous['production_baseline'],current_build_candidate=dict(identity=IDENTITY,source=source.relative_to(ROOT).as_posix(),hap=package,verification=verification,actual_compiled_native_sources=verification['compiled_native_sources'],upstream_cpp_compiled=379,patches=20,explicit_core_exports=367,promotion='Exact23 native/vector/original resource font windows only; own full UI/platform/startup/mixed not yet qualified'),
  target=dict(target=actual['target'],same_existing_emulator='MateBook Pro',device_identity=actual['device_identity'],last_verified_install_version='0.22.23',last_observed_font_pid=actual['pid'],data_reset=False,agreement_auto_accepted=False),
  current_ui=dict(status='not-yet-qualified-for-candidate23'),platform_image_audit=dict(status='not-yet-qualified-for-candidate23'),startup_audit=dict(status='not-yet-qualified-for-candidate23'),mixed_routes=dict(status='not-yet-qualified-for-candidate23',required_exact_requests_per_font_action=9),
  font_window=dict(status='passed-limited-native-vector-original-resource-window',report=bound(independent),actual_rounds=3,independent_checks=actual['checks'],actual_context_continuations=2,native_point_sizes=[18,27,18],original_vector_description='@18',resource_description='#harmony-synthetic-resource.fnt:nr__18',resource_entries=['nr__18','nr__27','nr__18'],resource_actual_frames=9,resource_all256_metrics_verified=True,resource_style_device_qualification=['nr__'],resource_ordinary_entry_scaling_only=True,complete_font_support=False,raster_binary=binary),
  mask_observations=dict(actual_integer_queries=33,integer_all_color_channels_enabled=True,raw_boolean_values_preserved=True,SDK_modified=False,GL4ES_modified=False),
  tests=dict(complete_current_python_suite_verified=True,python_cases=count,python_log=bound(args.python_log),source_replay=bound(args.replay_report),host_models_packaged=False),history=history,
  remaining=['23自己的212诊断UI/全部UI PID资源/固定12启动/三路混合，不能继承21','原字体资源小于8/大于28的原wx高质量重采样、资源样式设备验证、字体族/对话框/DPI/广泛字符','完整原UI/菜单/命令/文档/资源/Python/插件及P1～P8','真机/物理输入/正式签名发行'],
  evidence=[bound(args.font_report),bound(args.binary_proof),bound(independent),bound(args.python_log),bound(args.replay_report),bound(directory/'actual-raster-disassembly.log')]+list(archives.values()),updated_at=now.isoformat(timespec='seconds'))
 for name in ('record-olex-font-resource-progress.py','olex_font_resource_progress_validation.py','olex_font_resource_window_validation.py','olex_font_resource_fixtures.py','native_font_progress_validation.py','vector_font_state_window_validation.py','original_vector_font_window_validation.py','native_font_raster_window_validation.py','native_font_window_validation.py'):
  saved=directory/name;saved.write_bytes((ROOT/'scripts'/name).read_bytes());record['evidence'].append(bound(saved))
 for path in [directory/'result.json',ROOT/'reports/gx-resource-font-status.json']+paths:path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(status='in_progress',version='0.22.23',font_rounds=3,independent_checks=actual['checks'],source_compiled_units=verification['compiled_native_sources'],report_directory=str(directory),full_port_complete=False),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
