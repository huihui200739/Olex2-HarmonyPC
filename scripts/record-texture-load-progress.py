#!/usr/bin/env python3
"""Re-read actual source-bound LoadTextures evidence without promoting full-port gates."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from app_report_log import parse_app_reports
from build_provenance_png import verify
from renderer_probe_identity import installed_identity, package_identity
from texture_load_window_validation import IDENTITY, validate

ROOT = Path(__file__).resolve().parents[1]

def require(value, message):
    if not value:
        raise ValueError(message)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load(path):
    return json.loads(path.read_text())

def bound(path):
    path = path.resolve()
    require(path.is_relative_to(ROOT), 'Evidence outside project')
    return dict(path=str(path.relative_to(ROOT)), bytes=path.stat().st_size, sha256=sha(path))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('report','source','hap'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--target',required=True)
    p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
    a=p.parse_args()
    require(a.target=='127.0.0.1:5555','Only the same existing emulator accepted')
    report,source,hap=a.report.resolve(),a.source.resolve(),a.hap.resolve()
    require(report.is_relative_to(ROOT/'reports/emulator') and source.is_relative_to(ROOT/'out/gx-application-evaluation'),'Scope outside current project')
    require(package_identity(hap)==IDENTITY,'Expected exact candidate15')
    build=verify(hap,hap.with_suffix('.provenance.json'),source)
    native=load(hap.with_suffix('.provenance.json'))
    core=next(x for x in native['native_libraries'] if x['path'].endswith('/libolex2core_graphics.so'))
    require(len(core['exports'])==367 and not any(re.fullmatch(r'(?:gl|egl)[A-Z].*',x) for x in core['exports']),'Original core exports changed')
    lock=load(source/'source.lock.json');require(len(lock['patches'])==20,'Original twenty patches not preserved')
    inputs=load(source/'texture-load-inputs.json')
    for name,record in inputs['files'].items():
        path=source/name;require(sha(path)==record['sha256'] and path.stat().st_size==record['bytes'],'Frozen candidate input changed: '+name)
    run=load(report/'result.json');require(run['passed'] is True and run['target']==a.target and run['identity']==IDENTITY and run['hap_sha256']==sha(hap),'Actual run identity/result differs')
    require([r['label'] for r in run['rounds']]==['fresh','restart-1','restart-2','restart-3','repeat-2','repeat-3'],'Bounded six-run protocol differs')
    qualifications=[];previous=None;evidence=[bound(report/'result.json'),bound(hap),bound(hap.with_suffix('.provenance.json')),bound(source/'texture-load-inputs.json')]
    for index,row in enumerate(run['rounds']):
        if index<4:previous=None
        folder=(ROOT/row['folder']).resolve();require(folder.is_relative_to(report/'runtime'),'Window artifacts outside this actual run')
        current=validate(folder,previous)
        require(current['passed'] and current=={k:v for k,v in row['validation'].items() if k!='ui_checks'},'Fresh native/GL/window qualification differs or failed')
        require(index<4 or current['actual_context_migration_verified'],'Missing actual consecutive 2D context migration')
        label=row['label'];pid=current['observation']['pid'];n=current['observation']['run']
        probe=load(folder/f'shared-original-texture-load-run-{n}.json')['original']['legacy']['texture_load']
        snapshots={}
        for when in ('before','after'):
            capture=report/'runtime'/(label+'-'+when)
            status=load(capture/'native-verification.json');snapshots[when]=status
            logs=(capture/'process-scoped-report.log').read_text()
            require((capture/'pid.txt').read_text().strip()==str(pid) and status in parse_app_reports(logs),'UI snapshot not corroborated by the actual explicit process')
            require(status['installedPackage']==installed_identity(IDENTITY) and load(capture/'validation.json')['passed'] is True,'Real initialized UI identity/checks did not pass')
            observed=probe['log_'+when]
            require(all(status['originalLog'][k]==observed[k] for k in ('posted','dropped','truncated','captureFailures','clearCount','clearedEntries','textSha256')) and status['originalLog']['entryCount']==len(observed['entries']),'UI did not reflect the genuine loader log delta')
            for name in ('native-verification.json','process-scoped-report.log','pid.txt','validation.json','layout.json'):
                evidence.append(bound(capture/name))
        require(snapshots['before']['applicationRuntime']==snapshots['after']['applicationRuntime'] and snapshots['before']['workspaceState']==snapshots['after']['workspaceState'],'Initialized runtime/saved workspace changed')
        require(len(row['validation']['ui_checks'])==4 and all(c['passed'] is True for c in row['validation']['ui_checks']),'Four actual per-round UI checks not preserved')
        evidence.extend([bound(folder/'received-artifacts.json'),bound(folder/'independent-validation.json')])
        qualifications.append(dict(label=label,**current['observation'],native_checks=len(current['checks']),ui_checks=4,
                                   actual_context_migration_verified=current['actual_context_migration_verified']))
        previous=folder
    require(len({q['pid'] for q in qualifications})==4,'Four distinct actual processes required')
    logroot=ROOT/'out/gx-application-evaluation'
    python_log=logroot/'all-python-tests-texture-load15-attempt1.log'
    text=python_log.read_text();require('Ran 679 tests' in text and text.rstrip().endswith('OK'),'Complete current 679 Python tests did not pass')
    tests_log=logroot/'texture-load-verifier-tests-attempt1.log'
    text=tests_log.read_text();require('Ran 29 tests' in text and text.rstrip().endswith('OK'),'29 isolated verifier cases did not pass')
    def shell(command):
        marker='TEXTURE_LOAD_EXIT_'+uuid.uuid4().hex
        result=subprocess.run([a.hdc,'-t',a.target,'shell','( '+command+' ); code=$?; printf "\\n'+marker+'=%s\\n" "$code"'],capture_output=True,text=True,timeout=80)
        text=result.stdout+result.stderr;match=re.search(marker+r'=(\d+)',text)
        require(result.returncode==0 and match and match[1]=='0',text)
        return text[:match.start()].rstrip()
    installed=shell('bm dump -n '+IDENTITY['bundleName']);metadata=json.loads(installed[installed.index('{'):])
    for node in (metadata,metadata['applicationInfo']):
        require(node['versionName']==IDENTITY['versionName'] and node['versionCode']==IDENTITY['versionCode'],'Installed candidate changed')
    pid=shell('pidof '+IDENTITY['bundleName']).strip();require(pid==str(qualifications[-1]['pid']),'Last observed process no longer live')
    archive=ROOT/'reports/gx-application-platform-before-texture-load-status.json'
    current_path=ROOT/'reports/gx-application-platform-status.json'
    if not archive.exists():
        old=load(current_path)
        require(old['current_build_candidate']['identity']['versionName']=='0.22.14' and old['current_ui']['qualification']['total_ui_checks']==212,'Expected intact prior candidate14 full diagnostic qualification')
        shutil.copy2(current_path,archive)
    old=load(archive);now=datetime.now(ZoneInfo('Asia/Shanghai'))
    record_dir=ROOT/'reports/emulator'/('texture-load-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);record_dir.mkdir()
    (record_dir/'installed-bundle.log').write_text(installed)
    cpp_count=len({r['path'] for r in native['native']['compiled_sources'] if r['path'].startswith('upstream/') and r['path'].endswith('.cpp')})
    evidence.extend([bound(python_log),bound(tests_log),bound(archive),bound(record_dir/'installed-bundle.log')])
    for name in ('texture_load_window_validation.py','run-texture-load-window-smoke.py','prepare_texture_load_support.py','prepare-texture-load-candidate.py','generate-texture-load-fixtures.py','record-texture-load-progress.py'):
        evidence.append(bound(ROOT/'scripts'/name))
    for name in ('original_texture_load_probe.h','original_texture_load_probe.cpp','original_texture_load_fixtures.h'):
        evidence.append(bound(ROOT/'experiments/gx-texture-load/native'/name))
    evidence.append(bound(ROOT/'experiments/gx-texture-load/synthetic-fixtures.json'))
    record=dict(kind='original-named-UI-texture-load-limited-actual-window-qualification',status='in_progress',passed=False,
        milestone_complete=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,
        installed_target_code_directly_hashed=False,production_baseline=old['production_baseline'],
        current_build_candidate=dict(identity=IDENTITY,source=str(source.relative_to(ROOT)),hap=bound(hap),verification=build,
            actual_compiled_native_sources=build['compiled_native_sources'],upstream_cpp_compiled=cpp_count,patches=20,explicit_core_exports=367,
            promotion='partial actual LoadTextures window qualification; own full diagnostic UI and 12-start gates pending'),
        target=dict(old['target'],last_verified_install_version='0.22.15',current_pid=int(pid)),
        current_ui=dict(status='not-run-full-212-regression-for-0.22.15',new_control_and_runtime_snapshots_verified=True,
                        full_original_ui_implemented=False,prior_candidate14_full_diagnostic=bound(archive)),
        platform_image_audit=dict(status='not-collected-for-full-candidate15-UI-PID-set',prior_candidate14_full_audit_not_inherited=True),
        startup_audit=dict(status='fixed-12-start-audit-not-run-for-0.22.15',prior_candidate14_startup_qualification_not_inherited=True),
        persistent_gx=dict(implemented=True,owner='PlatformApp : PngTextureGXApp : TGXApp',unique_long_lived_owner_verified=True,
                           default_renderer_replaced=False,fonts_implemented=False),
        original_texture_load=dict(implemented=True,limited_actual_window_verified=True,report=bound(report/'result.json'),
            normal_level0_RGB_2d_only=True,load_and_resized_replacement_verified=True,actual_context_transitions=2,
            original_log_notifications=24,all_prior_original_logs_preserved=True,sampler_min_filter_parameter='nearest',
            actual_magnification_pixels='bilinear',wrap_s_and_t='repeat; not explicitly overwritten by Draw',
            last_png_decoder_after_each_loader_call_allocations_balanced=True,all_decoder_or_sdk_resource_audit=False,
            rounds=qualifications,total_native_checks=sum(q['native_checks'] for q in qualifications),
            total_ui_snapshot_checks=sum(q['ui_checks'] for q in qualifications)),
        tests=dict(python_cases=679,isolated_texture_load_verifier_cases=29,synthetic_test_fixtures_are_not_runtime_evidence=True),
        remaining=['当前0.22.15自身完整212诊断UI、全部UI进程资源收集与固定12启动',
                   '原纹理加载错误路径、minification实际GPU行为、其他格式/mipmap和异常资源恢复',
                   '真实字体、完整原UI/菜单/命令/文档/资源、Python与插件','完整P1～P8、真机物理输入、签名及发行'],
        history=dict(previous_candidate14_full_diagnostic=bound(archive),old_failures_preserved=True),
        evidence=evidence,updated_at=now.isoformat(timespec='seconds'))
    require(cpp_count==379 and build['compiled_native_sources']==604 and build['native_libraries']==3,'Actual candidate source/library scope differs')
    data=json.dumps(record,ensure_ascii=False,indent=2)+'\n'
    for path in (record_dir/'result.json',current_path,ROOT/'reports/gx-persistent-runtime-status.json',ROOT/'reports/gx-texture-load-status.json'):
        path.write_text(data)
    print(json.dumps(dict(status='in_progress',version='0.22.15',window_rounds=6,actual_2d_context_transitions=2,
                         real_original_log_notifications=24,full_port_complete=False,report_directory=str(record_dir)),ensure_ascii=False,indent=2))

if __name__=='__main__':
    main()
