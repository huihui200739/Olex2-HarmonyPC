#!/usr/bin/env python3
"""Record bounded persistent-GX progress without promoting full-UI qualification."""
import argparse,hashlib,json,re,shlex,shutil,subprocess,uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from persistent_gx_window_validation import validate
from texture_1d_window_validation import validate as validate_texture
from texture_context_window_validation import validate as validate_context
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
require(identity['versionName']=='0.22.14' and identity['versionCode']==221400,'Only the exact current candidate13 is accepted')
require(app['passed'] is True and app['target']==a.target and app['identity']==identity and app['hap_sha256']==sha(hap),'Window evidence and HAP differ')
verification=verify(hap,hap.with_suffix('.provenance.json'),source)
require(verification==app['build_verification'],'Captured and current source-built package differ')
proof=json.loads(hap.with_suffix('.provenance.json').read_text());compiled=proof['native']['compiled_sources'];deps=proof['native']['dependencies']
require(len(compiled)==603 and sum(r['path'].startswith('upstream/') and r['path'].endswith('.cpp') for r in compiled)==379,'Actual source inventory changed')
require(not any('/tests/' in r['path'] or 'tests/model' in r['path'] for r in compiled),'Host tests entered the HAP')
require(any(r['path']=='project/upstream/glib/glscene.h' for r in deps),'Original scene header is not in actual dependencies')
require(len(json.loads((source/'source.lock.json').read_text())['patches'])==20,'Expected 20 fixed source patches')
require(len((source/'cmake/original_renderer.exports').read_text().split('global:',1)[1].split('local:',1)[0].split(';'))-1==367,'Explicit symbol inventory changed')
current=ROOT/'reports/gx-application-platform-status.json';archive=ROOT/'reports/gx-application-platform-before-texture-context-status.json'
if not archive.exists():
 prior=json.loads(current.read_text());require(prior['current_build_candidate']['identity']['versionName']=='0.22.13' and prior['current_ui']['status']=='passed-limited-206-check-diagnostic-ui','Archive only the diagnostic-UI-qualified candidate13 record');shutil.copy2(current,archive)
prior=json.loads(archive.read_text())
now=datetime.now(ZoneInfo('Asia/Shanghai'));audit=ROOT/'reports/emulator'/('texture-context-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);audit.mkdir(parents=True)
device=shell(PC_IDENTITY_QUERY);(audit/'device-info.log').write_text(device);device_identity=validate_pc_emulator_identity(device)
installed=shell('bm dump -n '+shlex.quote(identity['bundleName']));(audit/'installed-bundle.log').write_text(installed);installed=json.loads(installed[installed.index('{'):])
for record in (installed,installed['applicationInfo']):require(record['versionName']==identity['versionName'] and record['versionCode']==identity['versionCode'],'Actual installed package differs')
live_pid=shell('pidof '+shlex.quote(identity['bundleName'])).strip();require(live_pid==app['rounds'][-1]['original']['pid'],'The final observed process is no longer live')
previous=None;previous_folder=None;rounds=[];all_raw=[]
expected_labels=['fresh','restart-1','restart-2','restart-3','repeat-2','repeat-3']
require([r['label'] for r in app['rounds']]==expected_labels,'Expected the fixed six-round protocol')
for index,item in enumerate(app['rounds']):
 label=item['label'];prefix='persistent-'+label;folder=report/'runtime'/(prefix+'-original');before=report/'runtime'/(prefix+'-before');after=report/'runtime'/(prefix+'-after')
 if index<4:previous=None;previous_folder=None
 actual=validate(folder,before,after,identity,previous,create_previews=False);texture=validate_texture(folder)
 # Stored reports are JSON: normalize integer map keys/tuples by complete,
 # finite JSON serialization; never exclude fields to hide a real mismatch.
 actual=json.loads(json.dumps(actual,ensure_ascii=False,allow_nan=False));texture=json.loads(json.dumps(texture,ensure_ascii=False,allow_nan=False))
 require(actual==item['original'] and texture==item['texture_1d'] and actual['passed'] and texture['passed'],'Raw window/state evidence changed or failed: '+label)
 require(len(actual['original']['checks'])==126 and len(actual['checks'])==10 and len(texture['checks'])==64,'Bounded gate inventory changed')
 migration=validate_context(folder,previous_folder)
 require(migration==item['texture_context'] and migration['passed'],'Retained texture raw evidence changed or failed: '+label)
 require(migration['actual_context_migration_verified'] is (index>=4),'Cross-context proof must exist only for the two same-process continuations')
 previous_folder=folder

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
 rounds.append(dict(label=label,pid=actual['pid'],run=actual['run'],original_window_checks=126,persistent_lifetime_checks=10,texture_1d_checks=64,retained_texture_checks=len(migration['checks']),actual_context_migration_verified=migration['actual_context_migration_verified'],texture_observation=migration['observation'],addresses=actual['addresses']))
 previous=actual
require(len({r['pid'] for r in rounds[:4]})==4 and [r['run'] for r in rounds]==[1,1,1,1,2,3] and len({r['pid'] for r in rounds[3:]})==1,'Independent-process and repeated-live-owner protocol differs')
logs={name:ROOT/'out/gx-application-evaluation'/name for name in (
 'all-python-tests-texture-context14-final.log','texture-context-host-ctest.log',
 'texture-context-host-observation.json','texture-context-verifier-tests.log',
 'texture-context-generator-replay.log','texture-context14b-patch-replay.log',
 'production-consistency-persistent-gx-ui-final.log')}
require('Ran 630 tests' in logs['all-python-tests-texture-context14-final.log'].read_text() and logs['all-python-tests-texture-context14-final.log'].read_text().rstrip().endswith('OK'),'Complete 630-test Python regression did not pass')
require('100% tests passed, 0 tests failed out of 1' in logs['texture-context-host-ctest.log'].read_text(),'Original-manager mocked regression did not pass')
host=json.loads(logs['texture-context-host-observation.json'].read_text());require(host['passed'] is True and host['checks']==843 and host['real_gpu_executed'] is False,'Expected 830 retained plus 13 detached-lifetime host assertions')
require('Ran 24 tests' in logs['texture-context-verifier-tests.log'].read_text() and logs['texture-context-verifier-tests.log'].read_text().rstrip().endswith('OK'),'Isolated verifier cases did not pass')
require('PASS exact texture-context candidate replay; 8 files; source13 unchanged' in logs['texture-context-generator-replay.log'].read_text(),'Candidate generator replay differs')
replay=json.loads(logs['texture-context14b-patch-replay.log'].read_text());require(replay['passed'] is True and replay['checks']==116,'Exact 20-patch forward/reverse replay did not pass')
standard=json.loads((ROOT/'reports/adaptation-consistency.json').read_text());require(standard['passed'] is True and len(standard['checks'])==329,'Standard summary gates changed or failed')
inputs=json.loads((source/'texture-context-inputs.json').read_text())
for name,row in inputs['files'].items():require(sha(source/name)==row['sha256'] and (source/name).stat().st_size==row['bytes'],'Candidate native input changed: '+name)
require(sha(ROOT/'scripts/build_provenance_graphics.py')=='3997e70b6582424db3a15446b107e11be599966483ba22c36fb0f4dceed2af7d','Frozen standard provenance engine changed')
stage=dict(kind='original-renderer-retained-texture-context-limited-window-qualification',status='in_progress',passed=False,milestone_complete=False,limited_window_qualification_passed=True,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
 production_baseline=prior['production_baseline'],
 current_build_candidate=dict(identity=identity,source=str(source.relative_to(ROOT)),hap=bound(hap),provenance=bound(hap.with_suffix('.provenance.json')),verification=verification,source_patches=20,upstream_cpp=379,canonical_exports=367,promotion='development-only; new real-context texture subset qualified; complete diagnostic UI/startup qualification still belongs to 0.22.13'),
 target=dict(target=a.target,same_existing_emulator='MateBook Pro',device_identity=device_identity,last_verified_install_version=identity['versionName'],current_pid=int(live_pid),data_reset=False,agreement_auto_accepted=False),
 persistent_gx=dict(implemented=True,unique_long_lived_owner_verified=True,owner='PlatformApp : PngTextureGXApp : TGXApp',window_rounds=rounds,total_original_window_checks=756,total_lifetime_checks=60,total_1d_checks=384,total_retained_texture_checks=sum(r['retained_texture_checks'] for r in rounds),actual_persistent_texture_context_migration_verified=True,actual_consecutive_context_transitions=2,persistent_texture_objects=4,fonts_implemented=False,default_renderer_replaced=False,scope='Original renderer owns four synthetic level-0 RGB/RGBA 1D texture objects across separately created/destroyed real window contexts. Nearest filters reapplied; wrapping explicitly set on each draw. Not original LoadTextures, 2D context migration, fonts or exceptional recovery.'),
 current_ui=dict(status='not-run-for-0.22.14',full_diagnostic_ui_qualified=False,previous_version='0.22.13',previous_qualification=prior['current_ui']),
 startup_audit=dict(status='fixed-12-not-run-for-0.22.14',four_window_startups_verified=True,previous_version='0.22.13',previous_qualification=prior['startup_audit']),
 platform_image_audit=dict(status='all-current-UI-PIDs-not-collected-for-0.22.14',previous_version='0.22.13',archive=bound(archive)),
 tests=dict(python_cases=630,retained_texture_mock_ctest_cases=1,retained_texture_mock_assertions=843,prior_texture_mock_assertions_preserved=830,isolated_context_verifier_cases=24,host_mock_is_not_gpu_evidence=True,host_models_packaged=False,standard_summary_consistency_checks=329),
 history=dict(previous_complete_diagnostic_candidate=prior['current_build_candidate'],before_current_candidate=bound(archive),old_failures_preserved=True,unexecuted_initial_source14='out/gx-application-evaluation/source14'),
 remaining=['当前0.22.14完整诊断UI、全部UI PID平台资源检查和固定12次实际启动','原TGXApp::LoadTextures真实GPU上传/替换/过滤及真实日志增量','二维纹理实际跨context迁移、一维mipmap/其他采样格式、异常资源恢复与清理','真实字体后端、完整原UI/资源/命令/Python/插件','P1～P8完整交付、真机与物理输入、签名和发行'],updated_at=now.isoformat(timespec='seconds'))
files=[ROOT/'scripts'/name for name in ('prepare-texture-context-candidate.py','texture_context_window_validation.py','run-texture-context-window-smoke.py','record-texture-context-progress.py','tests/test_texture_context_window_validation.py')]+[p for p in (ROOT/'experiments/gx-texture-context').rglob('*') if p.is_file()]
stage['evidence']=[bound(report/'result.json'),bound(audit/'installed-bundle.log'),bound(audit/'device-info.log'),bound(archive),bound(source/'texture-context-inputs.json')]+[bound(path) for path in logs.values()]+[bound(path) for path in files]
(audit/'artifact-bindings.json').write_text(json.dumps(all_raw,indent=2)+'\n');stage['artifact_bindings']=bound(audit/'artifact-bindings.json')
(audit/'result.json').write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n');current.write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n');(ROOT/'reports/gx-persistent-runtime-status.json').write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n');(ROOT/'reports/gx-texture-context-status.json').write_text(json.dumps(stage,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(status=stage['status'],version=identity['versionName'],actual_context_transitions=2,full_port_complete=False,report_directory=str(audit)),ensure_ascii=False,indent=2))
