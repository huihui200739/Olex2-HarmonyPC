#!/usr/bin/env python3
"""Record only exact25 finite resource qualification; never inherit21 UI/startup passes."""
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
from font_raster_flush_progress_validation import revalidate_window
from font_raster_flush_window_validation import IDENTITY
from native_font_progress_validation import ROOT,bound,check_bound,validate_disassembly,require,instruction_words

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('source','hap','font-report','binary-proof','python-log','replay-report'):parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args();source,hap=args.source.resolve(),args.hap.resolve()
 require(package_identity(hap)==IDENTITY,'Only exact25 may be recorded')
 verification=verify(hap,hap.with_suffix('.provenance.json'),source);package=bound(hap)
 actual=revalidate_window(args.font_report,package['sha256'],source)
 proof=json.loads(args.binary_proof.read_text());require(proof['identity']=='0.22.25' and proof['hap']==package,'Wrong actual resource binary proof')
 for name in ('hap','provenance','unstripped_source_build_output','disassembly'):check_bound(proof[name])
 require(proof['provenance']==bound(hap.with_suffix('.provenance.json')),'Wrong source provenance sidecar')
 library=check_bound(proof['unstripped_source_build_output']);require(library.is_relative_to(hap.parent/'stage'),'Not this actual source-built DSO')
 command=['/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-objdump','--disassemble','--start-address='+str(proof['function_start']),'--stop-address='+str(proof['function_end']),str(library)]
 fresh=subprocess.run(command,capture_output=True,text=True,check=True).stdout
 binary=validate_disassembly(check_bound(proof['disassembly']).read_text(),fresh,proof['function_start'],proof['function_end'])
 log=args.python_log.read_text();matched=re.search(r'Ran (\d+) tests in ',log);count=unittest.defaultTestLoader.discover(str(ROOT/'scripts/tests'),pattern='test_*.py').countTestCases()
 require(matched and log.rstrip().endswith('OK') and int(matched[1])==count,'Full current Python suite did not pass')
 replay=json.loads(args.replay_report.read_text());require(replay['passed'] and [row['files'] for row in replay['replayed']]==[5,2526,2528] and all(row['all_original_bytes_equal'] for row in replay['replayed']),'Exact fixture24/25 byte replay missing')
 for row in replay['recipes']:check_bound(row)
 host=ROOT/'out/gx-application-evaluation/font-raster-flush25-host-ctest-attempt1.log'
 require('100% tests passed, 0 tests failed out of 2' in host.read_text(),'Resampler and unchanged wx-source method reference CTests missing')
 host_observation=ROOT/'out/gx-application-evaluation/font-raster-flush25-host-observation-attempt1.log'
 require('PASS 1994 isolated synthetic' in host_observation.read_text() and 'PASS 6410 synthetic RGB cases / 657354 exact bytes' in host_observation.read_text(),'Exact host-only resampler observation missing')
 for name in ('resource_font_resample.cpp','resource_font_resample.h'):
  require((source/'native'/name).read_bytes()==(ROOT/'experiments/gx-font/native'/name).read_bytes(),'Host resampler differs from actual25 source')
 glyph_proof=ROOT/'out/gx-application-evaluation/font-raster-flush25-glyph-flush-proof.json';glyph=json.loads(glyph_proof.read_text())
 require(glyph['passed'] and glyph['source25_macro_in_actual_compile'] and glyph['strict_double_resampler_in_actual_compile'] and glyph['dso']==proof['unstripped_source_build_output'] and glyph['provenance']==proof['provenance'],'Actual raster-glyph binary fix proof differs')
 for key in ('dso','provenance','disassembly'):check_bound(glyph[key])
 glyph_command=glyph['command'];require(len(glyph_command)==6 and glyph_command[:3]==[command[0],'--disassemble','--demangle'] and glyph_command[-1]==str(library),'Wrong glyph proof actual binary command')
 require(all(flag.startswith(prefix) and flag[len(prefix):].isdigit() for flag,prefix in zip(glyph_command[3:5],['--start-address=','--stop-address='])),'Invalid glyph disassembly extent')
 glyph_fresh=subprocess.run(glyph_command,capture_output=True,text=True,check=True).stdout
 require(instruction_words(glyph_fresh)==instruction_words(check_bound(glyph['disassembly']).read_text()) and re.search(r'\b(?:b|bl)\s+.*<(?:gl4es_glFlush|glFlush)>',glyph_fresh),'Actual compiled raster glyph does not branch to flush')
 now=datetime.now(ZoneInfo('Asia/Shanghai'));directory=ROOT/'reports/emulator'/('font-raster-flush-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);directory.mkdir()
 names=('gx-resource-font-status','gx-vector-font-status','gx-font-status','gx-application-platform-status','gx-persistent-runtime-status','gx-texture-load-status','gx-texture-load-route-status')
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
  masks+=row['resource_mask_observations']+row['resample_mask_observations']
 require(len(masks)==51 and all(m['actual_integer']==[1]*4 for m in masks),'All51 actual driver integer snapshots required')
 history=dict(previous['history']);history['qualified_candidate23_before_resample']=archives
 failed24=ROOT/'reports/emulator/olex-font-resample-window-20261110-051354-68f22583/result.json'
 raw24=json.loads(failed24.read_text());require(raw24['passed'] is False and len(raw24['rounds'])==1 and len([row for row in raw24['rounds'][0]['checks'] if not row['passed']])==2,'Preserve actual24 shader-prefix and four-pixel failures')
 history['failed_candidate24_resample_report']=bound(failed24)
 require(replay['source_patch_count']==21 and replay['exact_patched_files']==29 and replay['reverse_recovered_pristine_original'],'All21 exact patch forward/reverse source replay required')
 record=dict(kind='original-font-resource-resample-limited-qualification',status='in_progress',passed=False,milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,complete_process_fd_metadata_verified=False,
  production_baseline=previous['production_baseline'],current_build_candidate=dict(identity=IDENTITY,source=source.relative_to(ROOT).as_posix(),hap=package,verification=verification,actual_compiled_native_sources=verification['compiled_native_sources'],upstream_cpp_compiled=379,patches=21,explicit_core_exports=367,promotion='Exact25 native/vector/original resource/resampling font windows only; own full UI/platform/startup/mixed not yet qualified'),
  target=dict(target=actual['target'],same_existing_emulator='MateBook Pro',device_identity=actual['device_identity'],last_verified_install_version='0.22.25',last_observed_font_pid=actual['pid'],data_reset=False,agreement_auto_accepted=False),
  current_ui=dict(status='not-yet-qualified-for-candidate25'),platform_image_audit=dict(status='not-yet-qualified-for-candidate25'),startup_audit=dict(status='not-yet-qualified-for-candidate25'),mixed_routes=dict(status='not-yet-qualified-for-candidate25',required_exact_requests_per_font_action=15),
  font_window=dict(status='passed-limited-native-vector-original-resource-window',report=bound(independent),actual_rounds=3,independent_checks=actual['checks'],actual_context_continuations=2,native_point_sizes=[18,27,18],original_vector_description='@18',resource_description='#harmony-synthetic-resource.fnt:nr__18',resource_entries=['nr__18','nr__27','nr__18'],resource_actual_frames=9,resource_all256_metrics_verified=True,resource_style_device_qualification=['nr__'],resample_point_sizes=[7,31,46,31,96,72],resample_actual_frames=18,resample_all256_rgb_cells_exact=True,resample_device_styles=['nr__','nrb_','fib_','ni__'],fixed_width_resource_verified=True,resource_ordinary_entry_scaling_only=False,original_vector_reference_shares_GPU=True,complete_font_support=False,full_wx_runtime_verified=False,alpha_mask_gamma_verified=False,raster_binary=binary),
  mask_observations=dict(actual_integer_queries=51,integer_all_color_channels_enabled=True,raw_boolean_values_preserved=True,SDK_modified=False,GL4ES_modified=False),
  tests=dict(complete_current_python_suite_verified=True,python_cases=count,python_log=bound(args.python_log),source_replay=bound(args.replay_report),host_models_packaged=False,host_resampler_ctest=bound(host),host_resampler_observation=bound(host_observation),host_assertions=1994,unchanged_wx_source_cases=6410,unchanged_wx_source_exact_bytes=657354,host_is_device_evidence=False),history=history,
  remaining=['25自己的212诊断UI/全部UI PID资源/固定12启动/三路混合，不能继承21','完整资源样式/字号与异常恢复、字体族/原对话框/DPI/广泛字符；完整wx运行时不在本次资格内','完整原UI/菜单/命令/文档/资源/Python/插件及P1～P8','真机/物理输入/正式签名发行'],
  evidence=[bound(args.font_report),bound(args.binary_proof),bound(independent),bound(args.python_log),bound(args.replay_report),bound(glyph_proof),bound(host),bound(host_observation),bound(directory/'actual-raster-disassembly.log')]+list(archives.values()),updated_at=now.isoformat(timespec='seconds'))
 for name in ('record-font-raster-flush-progress.py','font_raster_flush_progress_validation.py','font_raster_flush_window_validation.py','olex_font_resample_window_validation.py','resource_font_resample_reference.py','record-olex-font-resource-progress.py','olex_font_resource_progress_validation.py','olex_font_resource_window_validation.py','olex_font_resource_fixtures.py','native_font_progress_validation.py','vector_font_state_window_validation.py','original_vector_font_window_validation.py','native_font_raster_window_validation.py','native_font_window_validation.py'):
  saved=directory/name;saved.write_bytes((ROOT/'scripts'/name).read_bytes());record['evidence'].append(bound(saved))
 for path in [directory/'result.json',ROOT/'reports/gx-resource-font-status.json']+paths:path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(status='in_progress',version='0.22.25',font_rounds=3,independent_checks=actual['checks'],source_compiled_units=verification['compiled_native_sources'],report_directory=str(directory),full_port_complete=False),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
