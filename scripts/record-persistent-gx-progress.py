#!/usr/bin/env python3
"""Record bounded persistent-GX progress without promoting full-UI qualification."""
import argparse,hashlib,json,re,shlex,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from persistent_gx_window_validation import validate
from texture_1d_window_validation import validate as validate_texture
from renderer_probe_identity import package_identity
from app_report_log import parse_app_reports
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--window-report',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--hap',type=Path,required=True);p.add_argument('--target',required=True);p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
def require(value,message):
 if not value:raise ValueError(message)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bound(p):
 p=p.resolve();return dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p))
def shell(command):
 marker='PERSISTENT_GX_EXIT_'+uuid.uuid4().hex
 r=subprocess.run([a.hdc,'-t',a.target,'shell','( '+command+' ); status=$?; printf "\\n'+marker+'=%s\\n" "$status"'],capture_output=True,text=True,timeout=90)
 text=r.stdout+r.stderr;match=re.search(re.escape(marker)+r'=(\d+)',text)
 require(r.returncode==0 and match and match[1]=='0',text)
 return text[:match.start()].rstrip()
source=a.source.resolve();hap=a.hap.resolve();report=a.window_report.resolve();app=json.loads((report/'result.json').read_text());identity=package_identity(hap)
require(identity['versionName']=='0.22.13' and identity['versionCode']==221300,'Only the exact current candidate13 is accepted')
require(app['passed'] is True and app['target']==a.target and app['identity']==identity and app['hap_sha256']==sha(hap),'Window evidence and HAP differ')
verification=verify(hap,hap.with_suffix('.provenance.json'),source)
require(verification==app['build_verification'],'Captured and current source-built package differ')
proof=json.loads(hap.with_suffix('.provenance.json').read_text());compiled=proof['native']['compiled_sources'];deps=proof['native']['dependencies']
require(len(compiled)==602 and sum(r['path'].startswith('upstream/') and r['path'].endswith('.cpp') for r in compiled)==379,'Actual source inventory changed')
require(not any('/tests/' in r['path'] or 'tests/model' in r['path'] for r in compiled),'Host tests entered the HAP')
require(any(r['path']=='project/upstream/glib/glscene.h' for r in deps),'Original scene header is not in actual dependencies')
require(len(json.loads((source/'source.lock.json').read_text())['patches'])==19,'Expected 19 fixed source patches')
require(len((source/'cmake/original_renderer.exports').read_text().split('global:',1)[1].split('local:',1)[0].split(';'))-1==367,'Explicit symbol inventory changed')
current=ROOT/'reports/gx-application-platform-status.json';archive=ROOT/'reports/gx-application-platform-before-persistent-runtime-status.json'
if not archive.exists():
 prior=json.loads(current.read_text());require(prior['current_build_candidate']['identity']['versionName']=='0.22.11','Archive only the full-UI-qualified candidate11 record');shutil.copy2(current,archive)
prior=json.loads(archive.read_text())
now=datetime.now(ZoneInfo('Asia/Shanghai'));audit=ROOT/'reports/emulator'/('persistent-gx-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);audit.mkdir(parents=True)
device=shell(PC_IDENTITY_QUERY);(audit/'device-info.log').write_text(device);device_identity=validate_pc_emulator_identity(device)
installed=shell('bm dump -n '+shlex.quote(identity['bundleName']));(audit/'installed-bundle.log').write_text(installed);installed=json.loads(installed[installed.index('{'):])
for record in (installed,installed['applicationInfo']):require(record['versionName']==identity['versionName'] and record['versionCode']==identity['versionCode'],'Actual installed package differs')
live_pid=shell('pidof '+shlex.quote(identity['bundleName'])).strip();require(live_pid==app['rounds'][-1]['original']['pid'],'The final observed process is no longer live')
previous=None;rounds=[];all_raw=[]
expected_labels=['fresh','restart-1','restart-2','restart-3','repeat-2','repeat-3']
require([r['label'] for r in app['rounds']]==expected_labels,'Expected the fixed six-round protocol')
for index,item in enumerate(app['rounds']):
 label=item['label'];prefix='persistent-'+label;folder=report/'runtime'/(prefix+'-original');before=report/'runtime'/(prefix+'-before');after=report/'runtime'/(prefix+'-after')
 if index<4:previous=None
 actual=validate(folder,before,after,identity,previous,create_previews=False);texture=validate_texture(folder)
 # Stored reports are JSON: normalize integer map keys/tuples by complete,
 # finite JSON serialization; never exclude fields to hide a real mismatch.
 actual=json.loads(json.dumps(actual,ensure_ascii=False,allow_nan=False));texture=json.loads(json.dumps(texture,ensure_ascii=False,allow_nan=False))
 require(actual==item['original'] and texture==item['texture_1d'] and actual['passed'] and texture['passed'],'Raw window/state evidence changed or failed: '+label)
 require(len(actual['original']['checks'])==126 and len(actual['checks'])==10 and len(texture['checks'])==64,'Bounded gate inventory changed')
 for capture in (before,after):
  require((capture/'pid.txt').read_text().strip()==actual['pid'],'Snapshot process identity differs')
  snapshot=json.loads((capture/'native-verification.json').read_text());records=parse_app_reports((capture/'process-scoped-report.log').read_text())
  require(snapshot in records,'Snapshot is not corroborated by its live-process report')
 manifest=json.loads((folder/'received-artifacts.json').read_text());remote='/data/app/el2/100/base/'+identity['bundleName']+'/haps/entry/files';paths={}
 for name in manifest['files']:
  require(re.fullmatch('[A-Za-z0-9_.-]+',name) is not None,'Invalid retained artifact name')
  if name==f'shared-original-integrated-run-{actual["run"]}.json':
   # This host summary is overwritten by later processes using run=1. Its
   # original receive-time target SHA remains required by the 126-check gate;
   # do not mislabel it as freshly rehashed on the target after that overwrite.
   require(sha(folder/name)==manifest['files'][name]['target_sha256'],'Captured integrated summary SHA changed')
   all_raw.append(dict(**bound(folder/name),target_sha_rechecked_now=False,target_sha_scope='initial exact process/run receipt; mutable host summary path'))
  else:paths[name]=remote+'/'+manifest['artifact_subdirectory']+'/'+name
 output=shell('sha256sum '+' '.join(shlex.quote(path) for path in paths.values()));(audit/(prefix+'-target-sha256.log')).write_text(output+'\n')
 actual_hashes={line.split(None,1)[1].strip():line.split(None,1)[0] for line in output.splitlines()}
 for name,remote_path in paths.items():
  require(actual_hashes.get(remote_path)==sha(folder/name)==manifest['files'][name]['target_sha256'],'Fresh target/local artifact SHA differs: '+name)
  all_raw.append(dict(**bound(folder/name),target_sha_rechecked_now=True,target_sha_scope='fresh exact immutable process/run directory'))
 rounds.append(dict(label=label,pid=actual['pid'],run=actual['run'],original_window_checks=126,persistent_lifetime_checks=10,texture_1d_checks=64,addresses=actual['addresses']))
 previous=actual
require(len({r['pid'] for r in rounds[:4]})==4 and [r['run'] for r in rounds]==[1,1,1,1,2,3] and len({r['pid'] for r in rounds[3:]})==1,'Independent-process and repeated-live-owner protocol differs')
logs={name:ROOT/'out/gx-application-evaluation'/name for name in ('all-python-tests-persistent-gx.log','production-host-ctest-persistent-gx.log','persistent-gx-host-state-ctest.log','persistent-gx-verifier-tests.log','persistent-gx-generator-replay.log')}
require('Ran 572 tests' in logs['all-python-tests-persistent-gx.log'].read_text() and logs['all-python-tests-persistent-gx.log'].read_text().rstrip().endswith('OK'),'Complete Python suite did not pass')
require('100% tests passed, 0 tests failed out of 18' in logs['production-host-ctest-persistent-gx.log'].read_text(),'Standard host CTest did not pass')
require('100% tests passed, 0 tests failed out of 1' in logs['persistent-gx-host-state-ctest.log'].read_text(),'Modeled scene state CTest did not pass')
require('Ran 14 tests' in logs['persistent-gx-verifier-tests.log'].read_text() and logs['persistent-gx-verifier-tests.log'].read_text().rstrip().endswith('OK'),'Lifecycle verifier tests did not pass')
require('PASS exact persistent GX generator replay; 10 files' in logs['persistent-gx-generator-replay.log'].read_text(),'Candidate generator replay did not pass')
stage=dict(kind='persistent-unique-gx-owner-limited-window-qualification',status='in_progress',passed=False,milestone_complete=False,limited_window_qualification_passed=True,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
 production_baseline=prior['production_baseline'],current_build_candidate=dict(identity=identity,source=str(source.relative_to(ROOT)),hap=bound(hap),provenance=bound(hap.with_suffix('.provenance.json')),verification=verification,source_patches=19,upstream_cpp=379,canonical_exports=367,promotion='development-only; not full-UI-qualified or production'),
 target=dict(target=a.target,same_existing_emulator='MateBook Pro',device_identity=device_identity,last_verified_install_version=identity['versionName'],current_pid=int(live_pid),data_reset=False,agreement_auto_accepted=False),
 persistent_gx=dict(implemented=True,unique_long_lived_owner_verified=True,owner='PlatformApp : PngTextureGXApp : TGXApp',window_rounds=rounds,total_original_window_checks=756,total_lifetime_checks=60,total_1d_checks=384,fonts_implemented=False,actual_persistent_texture_context_migration_verified=False,default_renderer_replaced=False,scope='Empty application, three original synthetic primitives and independent temporary 1D manager only; context leases are transient, original GX/renderer/scene persist.'),
 current_ui=dict(status='not-run-for-0.22.13',full_diagnostic_ui_qualified=False,previous_version='0.22.11',previous_qualification=prior['current_ui']),
 startup_audit=dict(status='fixed-12-not-run-for-0.22.13',four_window_startups_verified=True,previous_version='0.22.11',previous_qualification=prior['startup_audit']),
 platform_image_audit=dict(status='all-current-UI-PIDs-not-collected-for-0.22.13',previous_version='0.22.11',archive=bound(archive)),
 tests=dict(python_cases=572,standard_host_ctest_cases=18,modeled_scene_ctest_cases=1,modeled_scene_assertions=83,lifecycle_verifier_cases=14,host_model_is_not_original_scene_or_gpu_evidence=True,host_models_packaged=False),
 history=dict(previous_complete_diagnostic_candidate=prior['current_build_candidate'],candidate12='out/gx-application-evaluation/source12',candidate12_window_report='reports/emulator/persistent-gx-window-20261109-233552-033ce2de/result.json',candidate12_initial_stale_capture='reports/emulator/persistent-gx-window-20261109-233441-1489ac05/result.json',old_failures_preserved=True),
 remaining=['为193宏/46函数长期GX所有者取得独立宿主目录原字节参考及完整200项界面回归，不沿用TX135/38资格','当前包全部UI PID平台资源审计及固定12次真实启动','原TGXApp::LoadTextures真实GPU上传/替换/过滤及真实日志增量','持久纹理实际跨context迁移、一维mipmap/采样格式和GPU资源异常清理','真实字体后端、完整原UI/资源/命令/Python/插件','P1～P8完整交付、真机与物理输入、签名和发行'],
 updated_at=now.isoformat(timespec='seconds'))
source_files=[ROOT/'scripts'/n for n in ('prepare-persistent-gx-candidate.py','persistent_gx_window_validation.py','run-persistent-gx-window-smoke.py','record-persistent-gx-progress.py','tests/test_persistent_gx_window_validation.py')]+list((ROOT/'experiments/gx-persistent-runtime/native').glob('*'))+[ROOT/'experiments/gx-persistent-runtime/tests/platform_scene_state.cpp',ROOT/'experiments/gx-persistent-runtime/tests/model/glscene.h']
stage['evidence']=[bound(report/'result.json'),bound(audit/'installed-bundle.log'),bound(audit/'device-info.log'),bound(archive)]+[bound(path) for path in logs.values()]+[bound(path) for path in source_files]
(audit/'artifact-bindings.json').write_text(json.dumps(all_raw,indent=2)+'\n');stage['artifact_bindings']=bound(audit/'artifact-bindings.json')
(audit/'result.json').write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n');current.write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n');(ROOT/'reports/gx-persistent-runtime-status.json').write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status=stage['status'],full_port_complete=False,limited_window_qualification_passed=True,version=identity['versionName'],report_directory=str(audit)),ensure_ascii=False,indent=2))
