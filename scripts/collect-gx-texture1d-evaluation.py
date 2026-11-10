#!/usr/bin/env python3
"""Collect the 1D-extended candidate from exactly its full UI run's PIDs."""
import argparse,hashlib,json,shlex,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance import verify as verify_build
from build_provenance_png import verify as verify_png_build
from renderer_probe_identity import package_identity
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
from texture_1d_application_validation import validate_application
from gx_application_validation import validate_lifecycle,validate_image_observations,validate_texture_hook,inspect_owned_resource_observations,validate_no_decoder_control
from png_texture_hook_validation import validate_png_texture_hook
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',required=True);p.add_argument('--application-root',type=Path,required=True)
p.add_argument('--app-report',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True)
p.add_argument('--texture-hook',action='store_true')
p.add_argument('--owned-memory-resource-proof',action='store_true')
p.add_argument('--png-rgb-backend',action='store_true')
p.add_argument('--require-post-decode-control',action='store_true')
p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
a=p.parse_args();
if not (a.texture_hook and a.owned_memory_resource_proof and a.png_rgb_backend and a.require_post_decode_control):raise ValueError('The 1D candidate requires all retained PNG/resource controls')
resource_proof='owned-memory' if a.owned_memory_resource_proof else 'full-metadata';token=uuid.uuid4().hex[:8]
report=ROOT/'reports/emulator'/('gx-texture1d-platform-app-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+token);report.mkdir(parents=True)
result=dict(texture_backend='libpng-unassociated-rgb' if a.png_rgb_backend else 'imagekit-opaque',target=a.target,scope='1D-extended diagnostic UI and startup platform artifacts; not persistent GX owner or complete port',functional_completion_percent=None,full_port_complete=False,physical_pc_verified=False,installed_target_code_directly_hashed=False,processes=[],checks=[],resource_proof=resource_proof,post_decode_control_required=a.require_post_decode_control)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(value,message):
 if not value:raise ValueError(message)
def cmd(*args):
 d=subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=90)
 output=d.stdout+d.stderr
 require(d.returncode==0 and not any(line.lstrip().startswith('[Fail]') for line in output.splitlines()),output)
 return output
def shell(command):return cmd('shell',command)
def pids_in(value):
 found=set()
 if isinstance(value,dict):
  for key,item in value.items():
   if (key=='pid' or key.endswith('_pid')) and type(item) in (str,int) and str(item).isdigit() and int(item)>0:found.add(int(item))
   if isinstance(item,(dict,list)):found.update(pids_in(item))
 elif isinstance(value,list):
  for item in value:found.update(pids_in(item))
 return found
try:
 device=shell(PC_IDENTITY_QUERY);(report/'device-info.log').write_text(device)
 result['device_identity']=validate_pc_emulator_identity(device)
 app_root=a.application_root.resolve();app_path=a.app_report.resolve()
 require(app_root.is_relative_to(ROOT) and app_path.is_relative_to(app_root/'reports/app'),'Application evidence outside this project')
 app=json.loads(app_path.read_text());require(app_path.parent==(app_root/app['report_directory']).resolve(),'Application report location differs')
 hap=app_path.parent/Path(app['build_provenance']['retained_hap']).name
 proof=app_path.parent/'build-provenance.json';identity=package_identity(hap)
 verification=(verify_png_build if a.png_rgb_backend else verify_build)(hap,proof)
 require(verification['hap_sha256']==app['hap']['sha256'],'Runtime HAP bytes differ')
 result['application']=dict(path=str(app_path),sha256=digest(app_path),hap_sha256=digest(hap),identity=identity,build_verification=verification,
                             ui=validate_application(app,app_root,identity))
 shutil.copy2(app_path,report/'application-result.json')
 fixtures=json.loads(a.fixtures.read_text())['fixtures'];shutil.copy2(a.fixtures,report/'synthetic-fixtures.json')
 installed=shell('bm dump -n '+shlex.quote(identity['bundleName']));(report/'installed-bundle.log').write_text(installed)
 installed_data=json.loads(installed[installed.index('{'):])
 for node in (installed_data,installed_data['applicationInfo']):
  require(node['versionName']==identity['versionName'] and node['versionCode']==identity['versionCode'],'Installed candidate identity changed')
 require(installed_data['name']==identity['bundleName'] if 'name' in installed_data else installed_data['applicationInfo']['bundleName']==identity['bundleName'],'Installed bundle differs')
 pids=sorted(pids_in(app));require(pids,'No application PIDs in the actual UI evidence')
 current=shell('pidof '+shlex.quote(identity['bundleName'])).strip();require(current.isdigit() and int(current) in pids,'Current process is not one observed in this UI run')
 result['current_pid']=int(current)
 for pid in pids:
  remote='/data/app/el2/100/base/'+identity['bundleName']+'/haps/entry/files/gx-platform-evaluation-pid-'+str(pid)
  target=report/('pid-'+str(pid));target.mkdir()
  names=['result.json']+[f['file'] for f in fixtures]+(['合成颜色-2.png'] if a.texture_hook else [])
  manifest={}
  for name in names:
   expected=shell('sha256sum '+shlex.quote(remote+'/'+name)).split()[0]
   path=target/name;(target/(name+'-receive.log')).write_text(cmd('file','recv',remote+'/'+name,path))
   require(digest(path)==expected,'Target artifact bytes differ: '+name)
   manifest[name]=dict(sha256=expected,target_sha256=expected,bytes=path.stat().st_size)
  for fixture in fixtures:require(manifest[fixture['file']]['sha256']==fixture['sha256'],'Synthetic PNG bytes differ from separate reference')
  if a.texture_hook:require(manifest['合成颜色-2.png']['sha256']==manifest['synthetic-rgb-2.png']['sha256'],'Unicode fixture differs from its RGB reference')
  observation=json.loads((target/'result.json').read_text());require(type(observation.get('pid')) is int and observation['pid']==pid and observation.get('execution_completed') is True,'Native observation is not from the expected process')
  item=dict(pid=pid,files=manifest,lifecycle=validate_lifecycle(observation['lifecycle']),image_capabilities=validate_image_observations(observation['images'],fixtures,require_resources=a.owned_memory_resource_proof,resource_proof=resource_proof))
  if a.owned_memory_resource_proof:
   item['owned_resources']=inspect_owned_resource_observations(observation,proof=resource_proof,texture_backend='libpng-unassociated-rgb' if a.png_rgb_backend else 'imagekit-opaque')
   item['no_decoder_control']=validate_no_decoder_control(observation.get('no_decoder_control'),'before-first-image-api' if a.require_post_decode_control else None)
  if a.require_post_decode_control or 'post_decode_control' in observation:item['post_decode_control']=validate_no_decoder_control(observation.get('post_decode_control'),'after-capability-probes')
  if a.texture_hook:item['texture_hook']=(validate_png_texture_hook if a.png_rgb_backend else validate_texture_hook)(observation['texture_hook'],fixtures,require_resources=a.owned_memory_resource_proof,resource_proof=resource_proof)
  result['processes'].append(item)
 result['checks']=[dict(name='HAP build, all original 194 and six additional 1D UI gates revalidated',passed=True),dict(name='installed bundle identity and explicit PID set match',passed=True),dict(name='all received synthetic artifacts match actual target SHA256',passed=True),dict(name='original empty graphics application identities/resources and image bytes independently checked',passed=True)]
 result['complete_descriptor_metadata_scan_passed']=all(row.get('owned_resources',{}).get('complete_descriptor_metadata_scan_passed') is True for row in result['processes'])
 result['passed']=True
except Exception as e:result.update(passed=False,error=type(e).__name__+': '+str(e))
finally:
 for name in ['collect-gx-texture1d-evaluation.py','texture_1d_application_validation.py','texture_1d_window_validation.py','gx_application_validation.py','gx_target_identity.py']:shutil.copy2(ROOT/'scripts'/name,report/name)
 (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),processes=len(result['processes']),error=result.get('error')),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
