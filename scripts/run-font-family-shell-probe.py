#!/usr/bin/env python3
"""Real installed-font CPU probe in an owned temporary shell directory.

It never installs/restarts the app, touches app data, or certifies the app/GPU.
"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import uuid
import zipfile
from zoneinfo import ZoneInfo
from font_family_reference import description, probe_selection
from gx_target_identity import PC_IDENTITY_QUERY, validate_pc_emulator_identity
from native_font_progress_validation import ROOT, bound, require
from native_font_window_validation import inspect_cells

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target',required=True);p.add_argument('--binary',type=Path,required=True);p.add_argument('--hap',type=Path,required=True)
    p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc');a=p.parse_args()
    require(a.target=='127.0.0.1:5555','Only the same original emulator is allowed')
    identity=datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]
    report=ROOT/'reports/emulator'/('font-family-shell-'+identity);report.mkdir()
    remote='/data/local/tmp/olex2-font-family-'+identity
    result=dict(passed=False,target=a.target,scope='Actual shell-context CPU native font selection only; NOT installed application, original TGlFont, window/GPU or UI qualification',packaged_application_qualified=False,GPU_qualified=False,full_port_complete=False,functional_completion_percent=None,remote_created=False,remote_cleanup_completed=False,files={})
    backed_up=False
    def cmd(*args):
        command=[a.hdc,'-t',a.target,*map(str,args)];run=subprocess.run(command,capture_output=True,text=True,timeout=90);text=run.stdout+run.stderr
        with (report/'commands.log').open('a') as f:f.write(shlex.join(command)+'\n'+text+'\n')
        require(run.returncode==0 and not any(s.lstrip().startswith('[Fail]') for s in text.splitlines()),text)
        return text
    def shell(s):return cmd('shell',s)
    try:
        device=shell(PC_IDENTITY_QUERY);(report/'device-info.log').write_text(device);result['device_identity']=validate_pc_emulator_identity(device)
        (report/'shell-identity.log').write_text(shell('id'))
        (report/'disk-before.log').write_text(shell('df -k /data'))
        require('__CREATED__' in shell('mkdir '+shlex.quote(remote)+' && printf __CREATED__'),'Could not create fresh owned probe directory')
        result['remote_created']=True
        binary=report/'font-family-device-probe';shutil.copyfile(a.binary,binary)
        library=report/'libc++_shared.so'
        with zipfile.ZipFile(a.hap) as archive:library.write_bytes(archive.read('libs/arm64-v8a/libc++_shared.so'))
        result['local_input_binary']=bound(a.binary);result['runtime_library_hap']=bound(a.hap)
        for path in (binary,library):
            cmd('file','send',path,remote+'/'+path.name)
            require(shell('sha256sum '+shlex.quote(remote+'/'+path.name)).split()[0]==bound(path)['sha256'],'Uploaded exact binary/library bytes differ')
        require('__EXECUTABLE__' in shell('chmod 700 '+shlex.quote(remote+'/font-family-device-probe')+' && test -x '+shlex.quote(remote+'/font-family-device-probe')+' && printf __EXECUTABLE__'),'Could not execute owned native test file')
        shell('cd '+shlex.quote(remote)+' && LD_LIBRARY_PATH='+shlex.quote(remote)+' ./font-family-device-probe > stdout.json 2> stderr.log; rc=$?; printf "%s\\n" "$rc" > '+shlex.quote(remote+'/exit-code.txt'))
        names=shell('find '+shlex.quote(remote)+' -type f').splitlines()
        allowed={'font-family-device-probe','libc++_shared.so','stdout.json','stderr.log','exit-code.txt'}|{f'selected-face-{i}.rgb' for i in range(3)}
        for target in names:
            require(target.startswith(remote+'/') and target[len(remote)+1:] in allowed,'Unexpected file in fresh probe directory')
            name=target[len(remote)+1:];expected=shell('sha256sum '+shlex.quote(target)).split()[0];received=report/('received-'+name)
            cmd('file','recv',target,received);row=bound(received)
            require(row['sha256']==expected,'Actual target/received probe bytes differ')
            result['files'][name]=dict(row,target_sha256=expected)
        require({'stdout.json','stderr.log','exit-code.txt','font-family-device-probe','libc++_shared.so'}<=set(result['files']),'Missing original execution evidence')
        backed_up=True
        require((report/'received-exit-code.txt').read_text().strip()=='0','Native shell probe failed: '+(report/'received-stderr.log').read_text())
        observed=json.loads((report/'received-stdout.json').read_text())
        require(observed['execution_context']=='hdc-shell-not-application' and observed['packaged_application_qualified'] is False and observed['GPU_qualified'] is False and observed['execution_completed'] is True,'Shell probe scope differs')
        selected=probe_selection(observed['catalog']);planned=[selected[i] for i in (0,3,4)]
        require(len(observed['cases'])==3 and observed['missing_family_rejected'] is True and observed['missing_style_rejected'] is True and observed['default_fallback'] is False and observed['synthetic_style'] is False,'All three exact selections and explicit failures required')
        summaries=[]
        for index,(case,(family,style)) in enumerate(zip(observed['cases'],planned)):
            require(case['family_index']==family['index'] and case['style_index']==style['index'] and case['description']==description(family['name'],style) and case['file']==f'selected-face-{index}.rgb','Observed exact installed family/style differs')
            require(type(case['actual_face_bold']) is bool and type(case['actual_face_italic']) is bool,'Missing actual face metadata')
            data=(report/('received-'+case['file'])).read_bytes();cells=inspect_cells(data,case['cell_width'],case['cell_height'])
            require(all(cells['masks'][c] for c in b'Ab09'),'Selected face has no visible bounded ASCII glyph ink')
            summaries.append(dict(family=family['name'],style=style['name'],weight_code=style['weight_code'],width_code=style['width_code'],slant_code=style['slant_code'],actual_face_bold=case['actual_face_bold'],actual_face_italic=case['actual_face_italic'],cell_width=case['cell_width'],cell_height=case['cell_height'],raw_rgb_sha256=hashlib.sha256(data).hexdigest(),all256_RGB_cells_received=True))
        result.update(passed=True,cases=summaries,actual_family_count=len(observed['catalog']['families']),actual_style_count=sum(len(f['styles']) for f in observed['catalog']['families']))
    except Exception as error:result['error']=type(error).__name__+': '+str(error)
    finally:
        # Delete only these freshly-created, completely received test files. No
        # paths in application data are examined, deleted or permission-modified.
        if backed_up:
            try:
                targets=[remote+'/'+name for name in result['files']]
                text=shell('rm -f '+' '.join(shlex.quote(p) for p in targets)+' && rmdir '+shlex.quote(remote)+' && printf __REMOVED__')
                require('__REMOVED__' in text,'Owned probe cleanup did not complete');result['remote_cleanup_completed']=True
            except Exception as error:result['cleanup_error']=str(error);result['passed']=False
        (report/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
        for name in ('font_family_reference.py','native_font_window_validation.py','gx_target_identity.py'):(report/name).write_bytes((ROOT/'scripts'/name).read_bytes())
        (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(passed=result['passed'],report_directory=str(report),actual_family_count=result.get('actual_family_count'),error=result.get('error'),packaged_application_qualified=False),ensure_ascii=False,indent=2))
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
