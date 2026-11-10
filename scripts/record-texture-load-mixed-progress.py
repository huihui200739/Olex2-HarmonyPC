#!/usr/bin/env python3
"""Re-read candidate16 alternating-route raw evidence; never promote full UI gates."""
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
from layout_validation import attributes,rectangle,controls_unclipped
from renderer_probe_identity import installed_identity,package_identity
from texture_load_route_validation import (IDENTITY,action_plan,expected_counters,validate_texture_load,
                                           validate_original_window,validate_lifetime_mixed)
from texture_1d_window_validation import validate as validate_1d
from texture_context_window_validation import validate as validate_retained_1d
ROOT=Path(__file__).resolve().parents[1]
def require(value,message):
    if not value:raise ValueError(message)
def load(path):return json.loads(path.read_text())
def canonical(value):return json.loads(json.dumps(value,ensure_ascii=False,allow_nan=False))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def bound(path):
    path=path.resolve();require(path.is_relative_to(ROOT),'Evidence outside project')
    return dict(path=str(path.relative_to(ROOT)),bytes=path.stat().st_size,sha256=sha(path))
def controls(layout):
    old,_=controls_unclipped(layout);nodes=list(attributes(layout))
    def one(name):
        matches=[n for n in nodes if n.get('id')==name]
        return matches[0] if len(matches)==1 else {}
    panel=rectangle(one('olex2-viewport-panel').get('bounds'));button=one('olex2-original-texture-load-probe')
    box=rectangle(button.get('bounds'));orig=rectangle(button.get('origBounds'))
    return bool(old and panel and box and orig==box and button.get('visible')=='true' and button.get('enabled')=='true' and
                button.get('type')=='Button' and panel[0]<=box[0]<box[2]<=panel[2] and panel[1]<=box[1]<box[3]<=panel[3])
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('report','source','hap'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--target',required=True);p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
    require(a.target=='127.0.0.1:5555','Only the existing explicitly selected emulator accepted')
    report,source,hap=a.report.resolve(),a.source.resolve(),a.hap.resolve()
    require(report.is_relative_to(ROOT/'reports/emulator') and source.is_relative_to(ROOT/'out/gx-application-evaluation'),'Evidence/source outside project')
    require(package_identity(hap)==IDENTITY,'Only exact candidate16 accepted')
    build=verify(hap,hap.with_suffix('.provenance.json'),source);native=load(hap.with_suffix('.provenance.json'))
    require(build['compiled_native_sources']==604 and build['native_libraries']==3,'Build source/library scope differs')
    core=next(x for x in native['native_libraries'] if x['path'].endswith('/libolex2core_graphics.so'))
    require(len(core['exports'])==367 and not any(re.fullmatch(r'(?:gl|egl)[A-Z].*',s) for s in core['exports']),'Explicit core ABI scope differs')
    require(len(load(source/'source.lock.json')['patches'])==20,'Twenty original patches not retained')
    cpp_count=len({s['path'] for s in native['native']['compiled_sources'] if s['path'].startswith('upstream/') and s['path'].endswith('.cpp')})
    require(cpp_count==379,'Upstream compile inventory differs')
    inputs=load(source/'texture-load-route-inputs.json')
    for name,e in inputs['files'].items():
        path=source/name;require(sha(path)==e['sha256'] and path.stat().st_size==e['bytes'],'Frozen candidate16 input changed: '+name)
    observed=load(report/'result.json')
    require(observed['passed'] is True and observed['identity']==IDENTITY and observed['target']==a.target and observed['hap_sha256']==sha(hap),'Mixed actual run identity or outcome differs')
    require([x['first_route'] for x in observed['orders']]==['load','original'],'Both exact cold-start orders required')
    evidence=[bound(report/'result.json'),bound(hap),bound(hap.with_suffix('.provenance.json')),bound(source/'texture-load-route-inputs.json')]
    qualifications=[];pids=set();totals=dict(original_window_checks=0,lifetime_checks=0,temporary_1d_checks=0,retained_1d_checks=0,load_checks=0,mixed_checks=0,original_actions=0,load_actions=0,original_1d_continuations=0,loaded_2d_continuations=0)
    for order in observed['orders']:
        first=order['first_route'];plan=expected_counters(action_plan(first))
        require(order['passed'] is True and order['plan']==plan and len(order['actions'])==6,'Planned six-action protocol differs')
        previous_load=None;previous_original=None;previous_lifetime=None;owners=None;pid=None
        for expected,action in zip(plan,order['actions']):
            require(all(action[k]==v for k,v in expected.items()) and action['passed'] is True and all(c['passed'] is True for c in action['checks']),'An action is missing, changed or failed')
            route,number,lease=expected['route'],expected['run'],expected['global_lease']
            folder=(ROOT/action['folder']).resolve();require(folder.is_relative_to(report/'runtime'),'Native artifact folder outside this actual run')
            require(load(folder/'independent-mixed-validation.json')==action,'Saved per-action validation differs')
            filename=('shared-original-texture-load-run-' if route=='load' else 'shared-original-integrated-run-')+str(number)+'.json'
            observation=load(folder/filename);legacy=observation['original']['legacy']
            current_pid=observation['original']['pid']
            if pid is None:pid=current_pid;pids.add(pid)
            require(current_pid==pid and observation['run']==observation['original']['run']==legacy['run']==number,'Actual PID or route-local ordinal differs')
            proof=legacy if route=='load' else legacy['persistent_gx']
            addresses={k:proof[k] for k in ('owner_address','renderer_address','scene_address')}
            addresses['manager_address']=legacy['texture_load']['manager_address'] if route=='load' else legacy['persistent_textures']['manager_address']
            if owners is None:owners=addresses
            require(addresses==owners,'Both routes did not share the same original owners/manager')
            require(all(type(proof[k]) is int for k in ('binding_before','detachment_before','binding_after','detachment_after')) and
                    proof['binding_before']==proof['detachment_before']==lease-1 and proof['binding_after']==proof['detachment_after']==lease,'Total physical context lease count not preserved')
            label=first+'-first-'+str(lease)+'-'+route;snapshots={}
            for when in ('before','after'):
                capture=report/'runtime'/(label+'-'+when);snapshot=load(capture/'native-verification.json');snapshots[when]=snapshot
                require((capture/'pid.txt').read_text().strip()==str(pid) and snapshot in parse_app_reports((capture/'process-scoped-report.log').read_text()),'UI record not corroborated by the actual explicit process')
                require(snapshot['installedPackage']==installed_identity(IDENTITY) and load(capture/'validation.json')['passed'] is True,'Actual UI runtime identity/checks differ')
                require(controls(load(capture/'layout.json')),'Existing or new route control is clipped')
                for name in ('native-verification.json','process-scoped-report.log','pid.txt','validation.json','layout.json'):evidence.append(bound(capture/name))
            require(snapshots['before']['applicationRuntime']==snapshots['after']['applicationRuntime'] and snapshots['before']['workspaceState']==snapshots['after']['workspaceState'],'Runtime/saved workspace changed')
            q=dict(first_route=first,route=route,run=number,global_lease=lease,pid=pid,addresses=addresses,report=bound(folder/'independent-mixed-validation.json'))
            if route=='load':
                qualified=canonical(validate_texture_load(folder,previous_load));require(qualified['passed'] and qualified==action['texture_load'],'Fresh original load/GPU/log verification differs')
                for when,snapshot in snapshots.items():
                    log=legacy['texture_load']['log_'+when]
                    require(all(snapshot['originalLog'][k]==log[k] for k in ('posted','dropped','truncated','captureFailures','clearCount','clearedEntries','textSha256')) and snapshot['originalLog']['entryCount']==len(log['entries']),'UI original log differs from actual loader notifications')
                totals['load_checks']+=len(qualified['checks']);totals['load_actions']+=1
                totals['loaded_2d_continuations']+=qualified['actual_context_migration_verified'];previous_load=folder
                q.update(checks=len(qualified['checks']),actual_context_restoration=qualified['actual_context_migration_verified'],log_before=qualified['observation']['log_before'],log_after=qualified['observation']['log_after'])
            else:
                original=canonical(validate_original_window(folder,report/'runtime'/(label+'-before'),report/'runtime'/(label+'-after'),IDENTITY,create_previews=False))
                lifetime,original_addresses=validate_lifetime_mixed(proof,str(pid),number,[snapshots['before'],snapshots['after']],lease,previous_lifetime)
                lifetime=canonical(lifetime)
                old_1d=canonical(validate_1d(folder));retained=canonical(validate_retained_1d(folder,previous_original))
                require(original['passed'] and len(original['checks'])==126 and original==action['original'],'All original 126 gates not preserved')
                require(len(lifetime)==10 and all(c['passed'] is True for c in lifetime) and lifetime==action['lifetime'],'All ten lifetime gates not preserved with global lease ordinal')
                require(old_1d['passed'] and len(old_1d['checks'])==64 and old_1d==action['texture_1d'],'All original 64 1D gates not preserved')
                require(retained['passed'] and retained==action['retained_1d'],'Fresh persistent 1D restoration proof differs')
                previous_lifetime=dict(passed=True,pid=str(pid),run=number,addresses=original_addresses);previous_original=folder
                totals['original_window_checks']+=126;totals['lifetime_checks']+=10;totals['temporary_1d_checks']+=64;totals['retained_1d_checks']+=len(retained['checks']);totals['original_actions']+=1
                totals['original_1d_continuations']+=retained['actual_context_migration_verified']
                q.update(original_window_checks=126,lifetime_checks=10,temporary_1d_checks=64,retained_1d_checks=len(retained['checks']),actual_context_restoration=retained['actual_context_migration_verified'])
            totals['mixed_checks']+=len(action['checks']);qualifications.append(q)
            evidence.extend([bound(folder/'received-artifacts.json'),bound(folder/'independent-mixed-validation.json')])
        require(pid==order['pid'],'Cold-order actual PID differs')
    require(len(pids)==2 and len(qualifications)==12 and totals['original_1d_continuations']==totals['loaded_2d_continuations']==4,'Actual per-route continuation/PID scope differs')
    logs=ROOT/'out/gx-application-evaluation'
    python_log=logs/'all-python-tests-texture-load16-mixed-final.log';text=python_log.read_text()
    require('Ran 706 tests' in text and text.rstrip().endswith('OK'),'Complete current 706 Python tests did not pass')
    isolated=logs/'texture-load16-route-verifier-tests.log';text=isolated.read_text();require('Ran 15 tests' in text and text.rstrip().endswith('OK'),'15 isolated route verifier cases did not pass')
    modeled=logs/'texture-load-route-host-ctest.log';text=modeled.read_text();require('62 assertions; no GL or device execution' in text and '100% tests passed, 0 tests failed out of 1' in text,'Isolated compiled route counter model did not pass')
    def shell(command):
        marker='MIXED_RECORD_EXIT_'+uuid.uuid4().hex
        run=subprocess.run([a.hdc,'-t',a.target,'shell','( '+command+' ); code=$?; printf "\\n'+marker+'=%s\\n" "$code"'],capture_output=True,text=True,timeout=80)
        text=run.stdout+run.stderr;match=re.search(marker+r'=(\d+)',text);require(run.returncode==0 and match and match[1]=='0',text)
        return text[:match.start()].rstrip()
    installed=shell('bm dump -n '+IDENTITY['bundleName']);metadata=json.loads(installed[installed.index('{'):])
    for node in (metadata,metadata['applicationInfo']):require(node['versionName']==IDENTITY['versionName'] and node['versionCode']==IDENTITY['versionCode'],'Actual installed candidate changed')
    pid=shell('pidof '+IDENTITY['bundleName']).strip();require(pid==str(qualifications[-1]['pid']),'Last mixed-route process no longer current')
    archive=ROOT/'reports/gx-application-platform-before-texture-load-route-status.json';current=ROOT/'reports/gx-application-platform-status.json'
    if not archive.exists():
        old=load(current);require(old['current_build_candidate']['identity']['versionName']=='0.22.15' and old['current_ui']['controller_exit_code']==1 and old['current_ui']['checks_recorded']==81,'Preserve candidate15 actual UI failure')
        shutil.copy2(current,archive)
    old=load(archive);now=datetime.now(ZoneInfo('Asia/Shanghai'));record_dir=ROOT/'reports/emulator'/('texture-load-mixed-revalidation-'+now.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);record_dir.mkdir()
    (record_dir/'installed-bundle.log').write_text(installed)
    evidence.extend([bound(archive),bound(python_log),bound(isolated),bound(modeled),bound(record_dir/'installed-bundle.log')])
    for name in ('texture_load_route_validation.py','run-texture-load-mixed-smoke.py','record-texture-load-mixed-progress.py','prepare-texture-load-route-candidate.py','prepare-texture-load-route-ui-tools.py','texture_load_route_application_validation.py'):
        evidence.append(bound(ROOT/'scripts'/name))
    record=dict(kind='original-graphics-and-texture-load-mixed-route-limited-actual-window-qualification',status='in_progress',passed=False,milestone_complete=False,
        full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,installed_target_code_directly_hashed=False,
        production_baseline=old['production_baseline'],current_build_candidate=dict(identity=IDENTITY,source=str(source.relative_to(ROOT)),hap=bound(hap),verification=build,
        actual_compiled_native_sources=604,upstream_cpp_compiled=379,patches=20,explicit_core_exports=367,promotion='limited actual mixed-route qualification; own full212/platform/12-start gates pending'),
        target=dict(old['target'],last_verified_install_version='0.22.16',current_pid=int(pid)),
        current_ui=dict(status='not-run-full-212-regression-for-0.22.16',full_original_ui_implemented=False),
        platform_image_audit=dict(status='not-collected-for-full-candidate16-UI-PID-set'),startup_audit=dict(status='fixed-12-start-audit-not-run-for-0.22.16'),
        persistent_gx=dict(implemented=True,owner='PlatformApp : PngTextureGXApp : TGXApp',unique_long_lived_owner_verified=True,default_renderer_replaced=False,fonts_implemented=False),
        mixed_routes=dict(limited_actual_window_verified=True,report=bound(report/'result.json'),first_orders=['load','original'],actual_processes=sorted(pids),actual_context_leases=12,
            route_local_runs_each=[1,2,3],same_original_owner_renderer_scene_manager_for_both_routes=True,actual_original_log_notifications=24,totals=totals,actions=qualifications,
            scope='Two cold-start orders with alternation. Four per-route 1D and four per-route 2D restoration comparisons; these are not a count of all physical context changes.'),
        tests=dict(python_cases=706,isolated_mixed_route_verifier_cases=15,isolated_route_counter_ctest_cases=1,isolated_route_counter_assertions=62,host_models_packaged=False),
        history=dict(candidate15_UI_failure=bound(archive),candidate14_full_diagnostic=old['history']['previous_candidate14_full_diagnostic'],old_failures_preserved=True),
        remaining=['当前16自身完整212诊断UI、全部当前UI进程资源与固定12启动','15布局读取超时原因仍未归因，不能据16有限测试声称已修复',
                   '字体、完整原UI/菜单/命令/文档/资源/Python/插件','完整纹理格式/异常恢复、P1～P8、真机物理输入和签名发行'],evidence=evidence,updated_at=now.isoformat(timespec='seconds'))
    data=json.dumps(record,ensure_ascii=False,indent=2)+'\n'
    for path in (record_dir/'result.json',current,ROOT/'reports/gx-persistent-runtime-status.json',ROOT/'reports/gx-texture-load-status.json',ROOT/'reports/gx-texture-load-route-status.json'):path.write_text(data)
    print(json.dumps(dict(status='in_progress',version='0.22.16',cold_orders=2,actual_actions=12,actual_context_leases=12,
        original_1d_restoration_comparisons=4,loaded_2d_restoration_comparisons=4,full_port_complete=False,report_directory=str(record_dir)),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
