#!/usr/bin/env python3
"""Revalidate the SAME retained cold starts without replacing failure evidence.

No target is contacted and no new runtime observation is claimed. Original
received SHA values, exact PIDs, full cold-start membership and HAP provenance
are checked before reapplying the unchanged lifecycle/count/color conditions.
"""
import argparse,hashlib,json,shutil,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance import verify as verify_build
from renderer_probe_identity import package_identity
from gx_application_validation import (validate_lifecycle,validate_image_observations,
    validate_texture_hook,inspect_owned_resource_observations,validate_no_decoder_control)
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--input-report',type=Path,required=True);p.add_argument('--target',required=True)
p.add_argument('--source',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True)
p.add_argument('--resource-proof',choices=['full-metadata','owned-memory'],required=True);a=p.parse_args()
report=ROOT/'reports/emulator'/('gx-startup-revalidation-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);report.mkdir(parents=True)
result=dict(kind='offline-revalidation-of-the-same-runtime-observations',target=a.target,resource_proof=a.resource_proof,
    new_runtime_observation_performed=False,full_ui_regression_performed=False,keeps_original_failed_report=True,
    full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,runs=[])
def require(value,message):
 if not value:raise ValueError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
try:
 source=a.input_report.resolve();require(source.is_relative_to(ROOT/'reports/emulator'),'Input must be retained project evidence')
 old=json.loads(source.read_text());require(old['target']==a.target,'Target identity differs')
 original_runs=old['runs'];require(isinstance(original_runs,list) and 1<=len(original_runs)<=20,'Missing original startup membership')
 result['input_report']=dict(path=str(source),sha256=sha(source),passed=old['passed'],runs=len(original_runs))
 hap=Path(old['hap']['path']);require(hap.is_relative_to(ROOT) and sha(hap)==old['hap']['sha256'],'Retained candidate HAP differs')
 build=verify_build(hap,hap.with_suffix('.provenance.json'),a.source)
 require(package_identity(hap)==old['identity'],'Retained HAP identity differs')
 result.update(hap=old['hap'],identity=old['identity'],build_provenance=build)
 fixtures=json.loads(a.fixtures.read_text())['fixtures'];shutil.copy2(a.fixtures,report/'synthetic-fixtures.json')
 pids=set()
 for number,previous in enumerate(original_runs,1):
  require(type(previous.get('run')) is int and previous['run']==number,'Original startup order differs')
  pid=previous['pid'];require(type(pid) is int and pid>0 and pid not in pids,'Original PID membership differs');pids.add(pid)
  native=source.parent/f'run-{number}'/'native-result.json';expected=previous['native_result_sha256']
  require(sha(native)==expected==previous['target_sha256'],'Original received native bytes differ')
  folder=report/f'run-{number}';folder.mkdir();shutil.copy2(native,folder/'native-result.json')
  raw=json.loads(native.read_text());require(raw.get('pid')==pid and type(raw['pid']) is int and raw.get('execution_completed') is True,'Native observation PID/completion differs')
  item=dict(run=number,pid=pid,native_result_sha256=expected,target_sha256=previous['target_sha256'],
            target_sha256_observed_in_original_run=True,original_validation_error=previous.get('validation_error'),original_passed=previous['passed'])
  try:item['owned_resources']=inspect_owned_resource_observations(raw,proof=a.resource_proof)
  except Exception as e:item['owned_resources']=dict(passed=False,error=str(e))
  try:
   require(item['owned_resources']['passed'],'Owned image resource scope did not validate')
   if a.resource_proof=='owned-memory':item['no_decoder_control']=validate_no_decoder_control(raw.get('no_decoder_control'))
   item['lifecycle']=validate_lifecycle(raw['lifecycle'])
   item['images']=validate_image_observations(raw['images'],fixtures,require_resources=True,resource_proof=a.resource_proof)
   item['texture_hook']=validate_texture_hook(raw['texture_hook'],fixtures,require_resources=True,resource_proof=a.resource_proof)
   item['passed']=True
  except Exception as e:
   item.update(passed=False,validation_error=str(e),native_texture_failure={k:v for k,v in raw.get('texture_hook',{}).items() if k not in ('cycles','partial_trace')})
  result['runs'].append(item)
 require(len(result['runs'])==len(original_runs),'Revalidation omitted an original cold start')
 result['passed']=all(row['passed'] for row in result['runs'])
 result['complete_descriptor_metadata_scan_passed']=all(row.get('owned_resources',{}).get('complete_descriptor_metadata_scan_passed') is True for row in result['runs'])
except Exception as e:result.update(passed=False,error=type(e).__name__+': '+str(e))
finally:
 for name in ['revalidate-gx-startup-audit.py','gx_application_validation.py']:shutil.copy2(ROOT/'scripts'/name,report/name)
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),runs=len(result['runs']),passed_runs=sum(x['passed'] for x in result['runs']),failures=[dict(run=x['run'],pid=x['pid'],error=x.get('validation_error')) for x in result['runs'] if not x['passed']],error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
