#!/usr/bin/env python3
"""Run source-built libpng CPU checks on one explicitly selected PC emulator."""
import argparse,hashlib,json,re,shlex,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
from png_texture_validation import validate_png_observations
from vendor_sources import verify_vendor
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',required=True);p.add_argument('--build',required=True,type=Path)
p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
sdk=Path('/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')
token=uuid.uuid4().hex[:8];report=ROOT/'reports/emulator'/('png-texture-cpu-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+token);report.mkdir(parents=True)
remote='/data/local/tmp/olex2-png-texture-'+token
result=dict(target=a.target,scope='ordinary synthetic PNG CPU decoding by fixed source-built libpng/zlib; not ImageKit, GPU upload, HAP integration, full original UI or complete port qualification',imagekit_executed=False,real_gpu_execution=False,full_port_complete=False,functional_completion_percent=None,checks=[],runs=[])
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def cmd(*args):
 d=subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=90);output=d.stdout+d.stderr
 if d.returncode or any(line.lstrip().startswith('[Fail]') for line in output.splitlines()):raise RuntimeError(output)
 return output
def shell(text):
 marker='PNG_SHELL_EXIT_'+uuid.uuid4().hex
 output=cmd('shell','( '+text+' ); c=$?; printf "\\n'+marker+'=%s\\n" "$c"')
 codes=re.findall(r'^'+re.escape(marker)+r'=(\d+)\r?$',output,re.M)
 if codes!=['0']:raise ValueError('Target PNG command failed: '+output)
 return output[:output.index(marker)].rstrip()
try:
 binary=a.build.resolve()/'olex2_png_texture_test';compiles=a.build.resolve()/'compile_commands.json'
 if not binary.is_file() or not compiles.is_file():raise ValueError('A built PNG test and real compile commands are required')
 device=shell(PC_IDENTITY_QUERY);(report/'device-info.log').write_text(device);result['device_identity']=validate_pc_emulator_identity(device)
 result['vendor']=verify_vendor(ROOT/'experiments/gx-png-image')
 elf=subprocess.run([str(sdk/'llvm/bin/llvm-readelf'),'-h','-d',str(binary)],capture_output=True,text=True,check=True).stdout
 (report/'binary-elf.log').write_text(elf)
 if 'AArch64' not in elf or re.search(r'NEEDED.*\[(?:libpng[^\]]*|libz\.so[^\]]*|libimage_source[^\]]*)\]',elf):raise ValueError('Expected AArch64 with source-built static PNG dependencies')
 shutil.copytree(ROOT/'experiments/gx-png-image',report/'source/png')
 source=report/'source/gx-application/native';source.mkdir(parents=True)
 for name in ('texture_image.h','texture_image.cpp','image_resource_audit.h','image_resource_audit_inl.h'):shutil.copy2(ROOT/'experiments/gx-application/native'/name,source/name)
 for name in ('run-png-texture-smoke.py','png_texture_validation.py','gx_application_validation.py','vendor_sources.py','gx_target_identity.py'):shutil.copy2(ROOT/'scripts'/name,report/name)
 shutil.copy2(compiles,report/'compile_commands.json')
 links=report/'link-commands';links.mkdir()
 for i,path in enumerate(sorted(a.build.resolve().rglob('link.txt'))):shutil.copy2(path,links/f'{i}-{path.parent.parent.name}-link.txt')
 source_files=[dict(path=f.relative_to(report/'source').as_posix(),bytes=f.stat().st_size,sha256=sha(f)) for f in sorted((report/'source').rglob('*')) if f.is_file()]
 (report/'source-manifest.json').write_text(json.dumps(source_files,ensure_ascii=False,indent=2)+'\n')
 result['source_manifest_sha256']=sha(report/'source-manifest.json')
 shell('mkdir '+shlex.quote(remote))
 for source in (binary,sdk/'llvm/lib/aarch64-linux-ohos/libc++_shared.so'):
  retained=report/source.name;shutil.copy2(source,retained);expected=sha(retained)
  (report/(source.name+'-send.log')).write_text(cmd('file','send',retained,remote+'/'+source.name))
  actual=shell('sha256sum '+shlex.quote(remote+'/'+source.name)).split()[0]
  result['checks'].append(dict(name=source.name,sha256=expected,target_sha256=actual,passed=expected==actual))
  if expected!=actual:raise ValueError('Target executable/library bytes differ')
 shell('chmod 700 '+shlex.quote(remote+'/'+binary.name))
 for number in range(1,4):
  artifact=remote+'/run-'+str(number)
  output=shell('cd '+shlex.quote(remote)+' && LD_LIBRARY_PATH=. ./'+binary.name+' '+shlex.quote(artifact))
  (report/f'run-{number}.log').write_text(output)
  payload=json.loads(next(line for line in output.splitlines() if line.startswith('{')))
  remote_json=artifact+'/png-observations.json';local=report/f'run-{number}-png-observations.json'
  expected=shell('sha256sum '+shlex.quote(remote_json)).split()[0]
  (report/f'run-{number}-receive.log').write_text(cmd('file','recv',remote_json,local))
  if sha(local)!=expected:raise ValueError('Target PNG observation bytes differ')
  raw=json.loads(local.read_text());observed=validate_png_observations(raw)
  for key in payload:
   if key not in raw or type(raw[key]) is not type(payload[key]) or raw[key]!=payload[key]:raise ValueError('PNG stdout and actual artifact differ: '+key)
  result['runs'].append(dict(run=number,passed=True,observed=observed,observation_sha256=expected,target_sha256=expected))
 result['passed']=True
except Exception as error:result.update(passed=False,error=type(error).__name__+': '+str(error))
finally:
 result.update(target_artifact_directory=remote,target_artifacts_retained=True,finished_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'))
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),runs=len(result['runs']),error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
