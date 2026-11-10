#!/usr/bin/env python3
"""Collect bounded ordinary cold-start diagnostics, not a full UI release gate."""
import argparse,hashlib,json,shlex,subprocess,time,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance import verify as verify_build
from build_provenance_png import verify as verify_png_build
from renderer_probe_identity import package_identity
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
from gx_application_validation import validate_lifecycle,validate_image_observations,validate_texture_hook,inspect_owned_resource_observations,validate_no_decoder_control
from png_texture_hook_validation import validate_png_texture_hook
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--runs',type=int,default=12)
p.add_argument('--require-resource-audit',action='store_true')
p.add_argument('--owned-memory-resource-proof',action='store_true')
p.add_argument('--png-rgb-backend',action='store_true')
p.add_argument('--require-post-decode-control',action='store_true')
p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
if a.require_resource_audit and a.owned_memory_resource_proof:raise ValueError('Select one explicit resource proof')
require_resources=a.require_resource_audit or a.owned_memory_resource_proof
proof='owned-memory' if a.owned_memory_resource_proof else 'full-metadata'
if not 1<=a.runs<=20:raise ValueError('Use 1 to 20 normal startup observations')
report=ROOT/'reports/emulator'/('gx-startup-audit-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);report.mkdir(parents=True)
result=dict(texture_backend='libpng-unassociated-rgb' if a.png_rgb_backend else 'imagekit-opaque',target=a.target,scope='diagnostic cold starts of the same synthetic evaluation package only; no full UI regression or complete port certification',full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,full_ui_regression_performed=False,owned_resource_audit_required=require_resources,resource_proof=proof,post_decode_control_required=a.require_post_decode_control,runs=[])
def require(value,message):
 if not value:raise ValueError(message)
def cmd(*args):
 d=subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=90)
 output=d.stdout+d.stderr
 require(d.returncode==0 and not any(line.lstrip().startswith('[Fail]') for line in output.splitlines()),output)
 return output
def shell(s):return cmd('shell',s)
def pid():return shell('pidof com.example.olex2harmonypc || true').strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
try:
 identity=package_identity(a.hap);require(identity['bundleName']=='com.example.olex2harmonypc','Only the existing evaluation bundle may be used')
 device=shell(PC_IDENTITY_QUERY);(report/'device-info.log').write_text(device)
 result['device_identity']=validate_pc_emulator_identity(device)
 build_proof=(verify_png_build if a.png_rgb_backend else verify_build)(a.hap,a.hap.with_suffix('.provenance.json'),a.source)
 result.update(identity=identity,hap=dict(path=str(a.hap.resolve()),sha256=sha(a.hap)),build_provenance=build_proof)
 fixtures=json.loads(a.fixtures.read_text())['fixtures']
 (report/'install.log').write_text(cmd('install',a.hap.resolve()))
 dump=shell('bm dump -n com.example.olex2harmonypc');(report/'installed-bundle.log').write_text(dump)
 require('{' in dump,'No installed bundle identity was returned')
 installed=json.loads(dump[dump.index('{'):])
 for row in (installed,installed['applicationInfo']):require(row['versionName']==identity['versionName'] and row['versionCode']==identity['versionCode'],'Installed identity differs')
 seen=set()
 for number in range(1,a.runs+1):
  folder=report/f'run-{number}';folder.mkdir();(folder/'stop.log').write_text(shell('aa force-stop com.example.olex2harmonypc'))
  deadline=time.monotonic()+8
  while pid() and time.monotonic()<deadline:time.sleep(.1)
  require(not pid(),'Previous owned process did not stop')
  (folder/'start.log').write_text(shell('aa start -a EntryAbility -b com.example.olex2harmonypc'))
  deadline=time.monotonic()+15;current='';raw=None
  while time.monotonic()<deadline:
   current=pid()
   if current.isdigit():
    remote='/data/app/el2/100/base/com.example.olex2harmonypc/haps/entry/files/gx-platform-evaluation-pid-'+current+'/result.json'
    try:
     output=shell('cat '+shlex.quote(remote));raw=json.loads(output)
     if raw.get('pid')==int(current) and raw.get('execution_completed') is True:break
    except (ValueError,json.JSONDecodeError):pass
   time.sleep(.1)
  require(raw is not None and raw.get('pid')==int(current) and raw.get('execution_completed') is True,'No complete native observation for current process')
  require(int(current) not in seen,'Repeated process ID in this bounded startup run');seen.add(int(current))
  expected=shell('sha256sum '+shlex.quote(remote)).split()[0];target=folder/'native-result.json';(folder/'receive.log').write_text(cmd('file','recv',remote,target))
  require(sha(target)==expected,'Target/native result bytes differ');raw=json.loads(target.read_text())
  item=dict(run=number,pid=int(current),native_result_sha256=expected,target_sha256=expected)
  if require_resources:
   try:item['owned_resources']=inspect_owned_resource_observations(raw,proof=proof,texture_backend='libpng-unassociated-rgb' if a.png_rgb_backend else 'imagekit-opaque')
   except Exception as e:item['owned_resources']=dict(passed=False,error=str(e))
  try:
   if a.owned_memory_resource_proof:item['no_decoder_control']=validate_no_decoder_control(raw.get('no_decoder_control'),'before-first-image-api' if a.require_post_decode_control else None)
   if a.require_post_decode_control or 'post_decode_control' in raw:item['post_decode_control']=validate_no_decoder_control(raw.get('post_decode_control'),'after-capability-probes')
   if require_resources:require(item['owned_resources']['passed'],'Owned image resources did not validate')
   item['lifecycle']=validate_lifecycle(raw['lifecycle']);item['images']=validate_image_observations(raw['images'],fixtures,require_resources=require_resources,resource_proof=proof)
   hook=raw['texture_hook']
   require(hook.get('execution_completed') is not True or all('fd_identities_before' in c and 'fd_identities_after' in c for c in hook['cycles']),'Old uninstrumented process artifact')
   item['texture_hook']=(validate_png_texture_hook if a.png_rgb_backend else validate_texture_hook)(hook,fixtures,require_resources=require_resources,resource_proof=proof);item['passed']=True
  except Exception as e:
   item.update(passed=False,validation_error=str(e),native_texture_failure={k:v for k,v in raw.get('texture_hook',{}).items() if k not in ('cycles','partial_trace')})
  result['runs'].append(item)
 result['complete_descriptor_metadata_scan_passed']=all(x.get('owned_resources',{}).get('complete_descriptor_metadata_scan_passed') is True for x in result['runs'])
 result['qualification_scope']='process descriptor count equality in original virtual cycles; public image release results and encoded-byte lifetimes; not all SDK internals'
 result['passed']=all(x['passed'] for x in result['runs'])
except Exception as e:result.update(passed=False,error=type(e).__name__+': '+str(e))
finally:
 result['finished_at']=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds')
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),runs=len(result['runs']),failures=[{k:v for k,v in x.items() if k in ('run','pid','validation_error','native_texture_failure')} for x in result['runs'] if not x['passed']],error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
