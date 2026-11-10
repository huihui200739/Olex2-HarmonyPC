#!/usr/bin/env python3
"""Exact25 native/vector fonts, original drawing and loading in both orders; no reset."""
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
import uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance_png import verify
from renderer_probe_identity import package_identity
from gx_target_identity import PC_IDENTITY_QUERY,validate_pc_emulator_identity
from layout_validation import attributes,rectangle,controls_unclipped
from prepare_texture_load_support import capture_profile
from font_raster_flush_mixed_validation import (IDENTITY,RECIPE_FILES,action_plan,expected_counters,validate_texture_load,
                                           validate_original_window,validate_lifetime_mixed,validate_font,resource_stages_complete)
from native_font_progress_validation import visible as visible_controls
from texture_1d_window_validation import validate as validate_1d
from texture_context_window_validation import validate as validate_retained_1d
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target',required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--hap',type=Path,required=True)
    p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
    if a.target!='127.0.0.1:5555':raise ValueError('Use only the same existing emulator')
    source,hap=a.source.resolve(),a.hap.resolve()
    if package_identity(hap)!=IDENTITY:raise ValueError('Only exact native/vector candidate25 accepted')
    report=ROOT/'reports/emulator'/('font-raster-flush-mixed-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]);report.mkdir(parents=True)
    result=dict(passed=False,target=a.target,identity=IDENTITY,hap_sha256=hashlib.sha256(hap.read_bytes()).hexdigest(),orders=[],
        full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,full_ui_qualification=False,installed_target_code_directly_hashed=False)
    bundle=IDENTITY['bundleName'];remote='/data/app/el2/100/base/'+bundle+'/haps/entry/files'
    env=dict(os.environ,PYTHONPATH=str(ROOT/'scripts'))
    def hdc(*args):
        command=[a.hdc,'-t',a.target,*map(str,args)]
        run=subprocess.run(command,capture_output=True,text=True,timeout=80);text=run.stdout+run.stderr
        with (report/'hdc-commands.log').open('a') as f:f.write(' '.join(shlex.quote(s) for s in command)+'\n'+text+'\n')
        if run.returncode or any(s.lstrip().startswith('[Fail]') for s in text.splitlines()):raise RuntimeError(text)
        return text
    def shell(command):return hdc('shell',command)
    def invoke(script,*args):
        command=[sys.executable,str(script),*map(str,args)]
        run=subprocess.run(command,capture_output=True,text=True,timeout=180,env=env)
        with (report/'commands.log').open('a') as f:f.write(' '.join(shlex.quote(s) for s in command)+'\n'+run.stdout+run.stderr+'\n')
        if run.returncode:raise RuntimeError(str(script)+': '+run.stdout+run.stderr)
    def start():
        shell('aa force-stop '+bundle);deadline=time.monotonic()+15
        while shell('pidof '+bundle).strip():
            if time.monotonic()>deadline:raise RuntimeError('Previous evaluation process did not stop')
            time.sleep(.2)
        shell('aa start -a EntryAbility -b '+bundle)
    def receive(route,number,folder):
        folder.mkdir(parents=True,exist_ok=False)
        pid=shell('pidof '+bundle).strip()
        if not pid.isdigit():raise ValueError('Missing actual evaluation process')
        filename={'load':'shared-original-texture-load-run-','original':'shared-original-integrated-run-','font':'shared-original-font-run-'}[route]+str(number)+'.json'
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            try:
                observation=json.loads(shell('cat '+shlex.quote(remote+'/'+filename)))
                if observation.get('run')==number and str(observation.get('original',{}).get('pid'))==pid:break
            except (ValueError,AttributeError):pass
            time.sleep(.2)
        else:raise RuntimeError('No actual current route/process/run result')
        artifact={'load':'original-texture-load-pid-','original':'original-renderer-pid-','font':'original-font-pid-'}[route]+pid+'-run-'+str(number)
        if observation['original'].get('artifact_subdirectory')!=artifact:raise ValueError('Unexpected route artifact identity')
        root=remote+'/'+artifact;names=shell('find '+shlex.quote(root)+' -type f').splitlines()+[remote+'/'+filename]
        files={}
        for path in names:
            if path==remote+'/'+filename:name=filename
            elif path.startswith(root+'/'):name=path[len(root)+1:]
            else:raise ValueError('Artifact outside this exact process/route/run')
            if not re.fullmatch(r'[A-Za-z0-9_./-]+',name) or '..' in Path(name).parts or Path(name).is_absolute():raise ValueError('Unexpected artifact name')
            expected=shell('sha256sum '+shlex.quote(path)).split()[0]
            local=folder/name;local.parent.mkdir(parents=True,exist_ok=True)
            if local.exists():raise FileExistsError(local)
            hdc('file','recv',path,local);raw=local.read_bytes();digest=hashlib.sha256(raw).hexdigest()
            if digest!=expected:raise ValueError('Target/local artifact hash differs')
            files[name]=dict(bytes=len(raw),sha256=digest,target_sha256=expected)
        if shell('pidof '+bundle).strip()!=pid:raise ValueError('Process changed during transfer')
        (folder/'received-artifacts.json').write_text(json.dumps(dict(pid=pid if route=='original' else int(pid),run=number,identity=IDENTITY,
            hap_sha256=result['hap_sha256'],artifact_subdirectory=artifact,files=files),indent=2)+'\n')
        return observation
    try:
        device=shell(PC_IDENTITY_QUERY);(report/'device-info.log').write_text(device)
        result['device_identity']=validate_pc_emulator_identity(device)
        result['build_verification']=verify(hap,hap.with_suffix('.provenance.json'),source)
        (report/'capture-profile.py').write_text(capture_profile(ROOT))
        hdc('install',hap)
        installed=shell('bm dump -n '+bundle);(report/'installed-bundle.log').write_text(installed)
        metadata=json.loads(installed[installed.index('{'):])
        for node in (metadata,metadata['applicationInfo']):
            if node['versionName']!=IDENTITY['versionName'] or node['versionCode']!=IDENTITY['versionCode']:raise ValueError('Installed identity differs')
        pids=set()
        for first in ('original','load'):
            start();order=dict(first_route=first,plan=expected_counters(action_plan(first)),actions=[]);result['orders'].append(order)
            previous_load=None;previous_original=None;previous_original_lifetime=None;previous_font=None;owners=None;manager=None;pid=None
            for step in order['plan']:
                route,number,lease=step['route'],step['run'],step['global_lease'];label=first+'-first-'+str(lease)+'-'+route
                before=report/'runtime'/(label+'-before');after=report/'runtime'/(label+'-after');folder=report/'runtime'/(label+'-native')
                common=['--target',a.target,'--hdc',a.hdc,'--hap',hap,'--report',report]
                invoke(report/'capture-profile.py',*common,'--label',label+'-before')
                before_state=json.loads((before/'native-verification.json').read_text())
                layout=json.loads((before/'layout.json').read_text());visible,layout_details=visible_controls(layout)
                if not visible:raise ValueError('Original or texture-load control is clipped or unavailable')
                identifier={'load':'olex2-original-texture-load-probe','original':'olex2-original-renderer-probe','font':'olex2-original-font-probe'}[route]
                invoke(ROOT/'scripts/original_renderer_click.py','--target',a.target,'--hdc',a.hdc,'--layout',before/'layout.json','--id',identifier)
                observation=receive(route,number,folder)
                invoke(report/'capture-profile.py',*common,'--label',label+'-after')
                after_state=json.loads((after/'native-verification.json').read_text());legacy=observation['original']['legacy']
                action=dict(**step,folder=str(folder.relative_to(ROOT)),layout=layout_details,checks=[])
                order['actions'].append(action)
                def check(name,value,detail=None):action['checks'].append(dict(name=name,passed=bool(value),detail=detail))
                current_pid=int(observation['original']['pid'])
                if pid is None:pid=current_pid;pids.add(pid)
                check('same actual process throughout this planned order',current_pid==pid)
                proof=legacy['persistent_gx'] if route=='original' else legacy
                addresses={k:proof[k] for k in ('owner_address','renderer_address','scene_address')}
                if route!='font':
                    current_manager=legacy['texture_load']['manager_address'] if route=='load' else legacy['persistent_textures']['manager_address']
                    if manager is None:manager=current_manager
                    check('same persistent manager across original drawing and loading',type(current_manager) is int and current_manager>0 and current_manager==manager)
                if owners is None:owners=addresses
                check('same unique owner renderer and scene across all three entries',addresses==owners)
                check('global lease counts exactly follow all three planned routes',type(lease) is int and
                    proof['binding_before']==proof['detachment_before']==lease-1 and proof['binding_after']==proof['detachment_after']==lease)
                check('actual route-local generation in wrapper and original core',observation['run']==observation['original']['run']==legacy['run']==number)
                if route!='load':
                    check('exact cumulative planned font requests at font/drawing boundary',type(proof.get('font_requests')) is int and proof['font_requests']==step['expected_font_requests'])
                else:
                    action['direct_load_font_counter_available']=False
                check('saved workspace and initialized runtime unchanged',before_state['workspaceState']==after_state['workspaceState'] and before_state['applicationRuntime']==after_state['applicationRuntime'])
                after_visible,after_details=visible_controls(json.loads((after/'layout.json').read_text()))
                check('all three actual controls remain fully visible after window return',after_visible,after_details)
                if route=='load':
                    qualified=validate_texture_load(folder,previous_load);action['texture_load']=qualified
                    check('all original load replace log GPU and per-route restoration gates',qualified['passed'])
                    for when,state in (('before',before_state),('after',after_state)):
                        log=legacy['texture_load']['log_'+when]
                        check('UI preserves genuine loader log '+when,all(state['originalLog'][k]==log[k] for k in
                            ('posted','dropped','truncated','captureFailures','clearCount','clearedEntries','textSha256')) and state['originalLog']['entryCount']==len(log['entries']))
                    previous_load=folder
                elif route=='font':
                    font=validate_font(folder,lease,previous_font,source,step['expected_font_requests']);action['font']=font
                    check('all original font glyph scale raster and restoration gates preserved',font['passed'])
                    check('all original vector pixels metrics scaling and actual integer masks preserved',font['passed'] and len(font['vector_cases'])==4 and len(font['mask_observations'])==8 and all(m['actual_integer']==[1]*4 for m in font['mask_observations']))
                    check('all original resource and six resampling stages pixels metrics and masks preserved',resource_stages_complete(font))
                    previous_font=folder
                else:
                    original=validate_original_window(folder,before,after,IDENTITY,create_previews=False)
                    lifetime,actual_addresses=validate_lifetime_mixed(proof,str(pid),number,[before_state,after_state],lease,step['expected_font_requests'],previous_original_lifetime)
                    old_1d=validate_1d(folder);retained=validate_retained_1d(folder,previous_original)
                    action.update(original=original,lifetime=lifetime,texture_1d=old_1d,retained_1d=retained)
                    check('all original 126 pixel matrix driver and host gates preserved',original['passed'] and len(original['checks'])==126)
                    check('all ten lifetime gates with explicit global lease ordinal',len(lifetime)==10 and all(c['passed'] for c in lifetime))
                    check('all original 64 temporary 1D gates preserved',old_1d['passed'] and len(old_1d['checks'])==64)
                    check('all original persistent 1D color and restoration gates preserved',retained['passed'])
                    previous_original_lifetime=dict(passed=all(c['passed'] for c in lifetime) and original['passed'],pid=str(pid),run=number,addresses=actual_addresses)
                    previous_original=folder
                action['passed']=all(c['passed'] for c in action['checks'])
                (folder/'independent-mixed-validation.json').write_text(json.dumps(action,indent=2)+'\n')
                print(label,'PASS' if action['passed'] else 'FAIL',flush=True)
                if not action['passed']:raise ValueError('Mixed-route actual window validation failed: '+label)
            order['passed']=len(order['actions'])==9 and all(x['passed'] for x in order['actions']) and order['actions'][-1]['route']=='font' and order['actions'][-1]['font']['passed'];order['pid']=pid
            order['font_counter_closure']='Final font action observes all forty-five planned requests; each font/drawing boundary exact; load route has no direct font-count telemetry' 
        if len(pids)!=2 or not all(o['passed'] for o in result['orders']):raise ValueError('Both cold-start order protocols did not complete')
        result['passed']=True
    except Exception as error:result['error']=type(error).__name__+': '+str(error)
    finally:
        for name in RECIPE_FILES:
            shutil.copy2(ROOT/'scripts'/name,report/name)
        (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(passed=result['passed'],report_directory=str(report),orders=len(result['orders']),actions=sum(len(x['actions']) for x in result['orders']),error=result.get('error')),ensure_ascii=False,indent=2))
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
