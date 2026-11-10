#!/usr/bin/env python3
"""Observe own encoded-byte lifetime and fake-handle cleanup on one target.

The fake handle cases do not execute ImageKit. Real ImageKit cleanup evidence
comes separately from the normally launched application, never this CLI.
"""
import argparse,hashlib,json,re,shlex,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from gx_application_validation import validate_image_resource_audit
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',required=True);p.add_argument('--binary',type=Path,required=True)
p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
sdk=Path('/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')
token=uuid.uuid4().hex[:8];report=ROOT/'reports/emulator'/('image-memory-scope-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+token);report.mkdir(parents=True)
remote='/data/local/tmp/olex2-image-memory-'+token
result=dict(target=a.target,scope='own file-to-memory input lifetime and explicitly fake handle cleanup only',imagekit_executed=False,full_port_complete=False,functional_completion_percent=None,checks=[],runs=[])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cmd(*args):
 d=subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=90)
 output=d.stdout+d.stderr
 if d.returncode or any(line.lstrip().startswith('[Fail]') for line in output.splitlines()):raise RuntimeError(output)
 return output
def shell(text):return cmd('shell',text)
try:
 shell('mkdir '+shlex.quote(remote))
 for source in [a.binary,sdk/'llvm/lib/aarch64-linux-ohos/libc++_shared.so']:
  retained=report/source.name;shutil.copy2(source,retained);expected=sha(retained)
  (report/(source.name+'-send.log')).write_text(cmd('file','send',retained,remote+'/'+source.name))
  actual=shell('sha256sum '+shlex.quote(remote+'/'+source.name)).split()[0]
  result['checks'].append(dict(name=source.name,sha256=expected,target_sha256=actual,passed=expected==actual))
  if expected!=actual:raise ValueError('Target executable/library differs')
 shell('chmod 700 '+shlex.quote(remote+'/'+a.binary.name))
 for number in range(1,4):
  marker='OWNED_RESOURCE_EXIT_'+token+'_'+str(number);artifact=remote+'/run-'+str(number)
  output=shell('( cd '+shlex.quote(remote)+' && LD_LIBRARY_PATH=. ./'+a.binary.name+' '+shlex.quote(artifact)+' ); c=$?; printf "\\n'+marker+'=%s\\n" "$c"')
  (report/f'run-{number}.log').write_text(output)
  codes=re.findall(r'^'+re.escape(marker)+r'=(\d+)\r?$',output,re.M)
  if codes!=['0']:raise ValueError('Owned resource CPU tests failed')
  payload=json.loads(next(line for line in output.splitlines() if line.startswith('{')))
  if payload.get('passed') is not True or payload.get('fake_handles_are_sdk_execution') is not False or type(payload.get('checks')) is not int or payload['checks']!=11:
   raise ValueError('Unexpected CPU test result')
  remote_json=artifact+'/actual-owned-memory-audit.json';local=report/f'run-{number}-actual-owned-memory-audit.json'
  actual=shell('sha256sum '+shlex.quote(remote_json)).split()[0]
  (report/f'run-{number}-receive.log').write_text(cmd('file','recv',remote_json,local))
  if sha(local)!=actual:raise ValueError('Owned descriptor audit bytes differ')
  audit=json.loads(local.read_text());f=audit.get('file',{});handles=audit.get('handles',[])
  if not (audit.get('owned_scope_balanced') is True and audit.get('input_kind')=='owned-memory-buffer' and
          audit.get('sdk_received_file_descriptor') is False and audit.get('buffer_released_after_handles') is True and
          type(audit.get('encoded_bytes')) is int and audit['encoded_bytes']>0 and
          f.get('opened') is True and f.get('identity_known') is True and f.get('close_attempted') is True and
          type(f.get('close_code')) is int and f['close_code']==0 and type(f.get('close_errno')) is int and f['close_errno']==0 and
          len(handles)==1 and handles[0].get('name')=='source' and handles[0].get('created') is True and
          handles[0].get('release_attempted') is True and type(handles[0].get('release_code')) is int and handles[0]['release_code']==0):
   raise ValueError('Owned-memory/fake-handle scope was misrepresented')
  if payload.get('full_metadata_scan_passed') is not audit.get('balanced'):raise ValueError('Full metadata scan result was misrepresented')
  result['runs'].append(dict(run=number,passed=True,checks=payload['checks'],audit_sha256=actual,target_sha256=actual,actual_owned_memory_audit=audit,full_metadata_scan_passed=audit["balanced"]))
 result['passed']=True
except Exception as e:result.update(passed=False,error=str(e))
finally:
 result['target_artifact_directory']=remote;result['target_artifacts_retained']=True
 shutil.copytree(ROOT/'experiments/gx-application',report/'source')
 shutil.copy2(Path(__file__),report/'runner.py')
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),runs=len(result['runs']),error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
