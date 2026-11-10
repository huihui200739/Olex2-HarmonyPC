#!/usr/bin/env python3
"""Execute only the exact user-approved old diagnostic cleanup manifest."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo
from diagnostic_backup_audit import (audit_local_backups, audit_device_inventory, create_owned_directory,
                                    owned_file, readonly_device_query, sha256)
from diagnostic_cleanup import (BUNDLE, TARGET, validate_authorization, deletion_script, parse_deletion,
                                preservation_script, parse_preservation, app_file_commands, APP_FILES)
from gx_target_identity import PC_IDENTITY_QUERY, validate_pc_emulator_identity

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan',type=Path,required=True)
    parser.add_argument('--authorization',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--target',required=True)
    parser.add_argument('--application-debug-files',action='store_true',help='Use standard HDC -b for this debug app only; never change system permissions or use root')
    parser.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
    args=parser.parse_args()
    if args.target != TARGET: parser.error('Only the original explicitly selected emulator')
    output=create_owned_directory(ROOT,args.output)
    result=dict(passed=False,scope='Authorized exact old synthetic diagnostic cleanup, not port qualification',
                target=TARGET,deletion_authorized=False,deleted_files=0,removed_directories=0,
                full_port_complete=False,functional_completion_percent=None,started_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat())
    for name in ('cleanup-backed-up-diagnostics.py','diagnostic_cleanup.py','diagnostic_backup_audit.py','gx_target_identity.py'):
        (output/name).write_bytes((ROOT/'scripts'/name).read_bytes())
    def save(): (output/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    def shell(script,label,check=True):
        (output/(label+'.sh')).write_text(script)
        command=[args.hdc,'-t',TARGET,'shell',script]
        run=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (output/(label+'.log')).write_bytes(run.stdout)
        (output/(label+'.command.json')).write_text(json.dumps(command,ensure_ascii=False,indent=2)+'\n')
        text=run.stdout.decode('utf-8',errors='strict')
        if check and (run.returncode or '[Fail]' in text):raise RuntimeError('Device observation failed: '+label)
        return run.returncode,text
    def app_command(command,label):
        argv=[args.hdc,'-t',TARGET,'shell','-b',BUNDLE,*command]
        run=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (output/(label+'.command.json')).write_text(json.dumps(argv,ensure_ascii=False,indent=2)+'\n')
        (output/(label+'.log')).write_bytes(run.stdout)
        return run.returncode,run.stdout.decode('utf-8',errors='strict')
    try:
        plan_path=owned_file(ROOT,args.plan); auth_path=owned_file(ROOT,args.authorization)
        plan=json.loads(plan_path.read_text());authorization=json.loads(auth_path.read_text())
        (output/'original-plan.json').write_bytes(plan_path.read_bytes())
        (output/'authorization.json').write_bytes(auth_path.read_bytes())
        for key in ('backup_audit','device_audit','exclusion_audit'):
            meta=plan[key];path=owned_file(ROOT,Path(meta['path']))
            if path.stat().st_size!=meta['bytes'] or sha256(path)!=meta['sha256']:
                raise ValueError('Original audit changed: '+key)
        original=json.loads(owned_file(ROOT,Path(plan['backup_audit']['path'])).read_text())
        reports=[Path(row['path']) for row in original['reports']]
        audit=audit_local_backups(ROOT,reports)
        if audit!=original:raise ValueError('Original complete local backup audit differs')
        validate_authorization(plan,sha256(plan_path),authorization,audit)
        result.update(deletion_authorized=True,plan_sha256=sha256(plan_path),
                      approved_directories=audit['directory_count'],approved_files=audit['file_count'],approved_payload_bytes=audit['payload_bytes'])
        (output/'local-before.json').write_text(json.dumps(audit,indent=2)+'\n');save()
        result['device_identity']=validate_pc_emulator_identity(shell(PC_IDENTITY_QUERY,'device-identity')[1])
        raw=shell(readonly_device_query(audit),'device-before')[1]
        checked=audit_device_inventory(audit,raw)
        result['disk_before']=checked['disk_observation']
        (output/'device-before.json').write_text(json.dumps(checked,indent=2)+'\n')
        before=parse_preservation(shell(preservation_script(audit),'protected-before')[1])
        result['protected_readable_file_count']=len(before['readable_files'])
        result['protected_runtime_contents_hashed']=False
        result['protected_runtime_policy']='Never traversed or modified; public directory metadata only'
        (output/'protected-before.json').write_text(json.dumps(before,indent=2)+'\n');save()
        for index,row in enumerate(audit['directories'],1):
            single=dict(audit,directories=[row],directory_count=1,file_count=len(row['files']),payload_bytes=row['bytes'])
            audit_device_inventory(single,shell(readonly_device_query(single),f'{index:03}-before')[1])
            if args.application_debug_files:
                result['file_access_mode']='Standard HDC shell -b for own installed debug bundle; no root or permission modification'
                _,physical=shell(f"stat -c '%i:%s:%a:%Y:%Z' {row['remote']}",f'{index:03}-physical-stat')
                code,scoped=app_command(['stat','-c','%i:%s:%a:%Y:%Z',APP_FILES+'/'+row['directory']],f'{index:03}-debug-stat')
                if code or physical.strip()!=scoped.strip():raise ValueError('App-scoped directory does not match approved physical directory metadata')
                remove,empty_dirs=app_file_commands(row)
                code,text=app_command(remove,f'{index:03}-debug-remove-files')
                query=f'set -eu\nprintf "__REMAINING__\\n"\nfind {row["remote"]} -type f -o -type l\nprintf "__REMAINING_DONE__\\n"\n'
                _,remaining=shell(query,f'{index:03}-remaining-files')
                lines=remaining.replace('\r\n','\n').splitlines()
                if len(lines)<2 or lines[0]!='__REMAINING__' or lines[-1]!='__REMAINING_DONE__':raise ValueError('Incomplete post-removal inventory')
                expected={row['remote']+'/'+name for name in row['files']}
                if set(lines[1:-1])-expected:raise ValueError('Unexpected new diagnostic file; stop cleanup')
                result['deleted_files']+=len(expected-set(lines[1:-1]));save()
                if code or text.strip() or lines[1:-1]:raise ValueError('App-scoped file removal incomplete: '+text[:500])
                code,text=app_command(empty_dirs,f'{index:03}-debug-remove-empty-directories')
                if code or text.strip():raise ValueError('Empty directory removal incomplete: '+text[:500])
                _,absent=shell(f'set -eu\n[ ! -e {row["remote"]} ] && [ ! -L {row["remote"]} ]\nprintf "__DIRECTORY_ABSENT__\\n"\n',f'{index:03}-absent')
                if absent.strip()!='__DIRECTORY_ABSENT__':raise ValueError('Directory removal not confirmed')
            else:
                code,text=shell(deletion_script(row),f'{index:03}-remove',check=False)
                result['deleted_files']+=len([line for line in text.splitlines() if line.startswith('__DELETED__')]);save()
                parse_deletion(row,text,code)
            result['removed_directories']+=1;save()
            print(f'Removed {index}/{len(audit["directories"])} approved diagnostic directories',flush=True)
        after=parse_preservation(shell(preservation_script(audit),'protected-after')[1])
        (output/'protected-after.json').write_text(json.dumps(after,indent=2)+'\n')
        result['protected_files_unchanged']=before==after
        if before!=after:raise ValueError('Nonselected files changed; inspect preserved evidence before any installation')
        script='set -eu\n'+'\n'.join(f'[ ! -e {row["remote"]} ] && [ ! -L {row["remote"]} ]' for row in audit['directories'])+'\nprintf "__ABSENT__81\\n"\ndf -k '+audit['directories'][0]['remote'].rsplit('/',1)[0]+'\n'
        result['post_cleanup_absence_and_disk']=shell(script,'absence-and-disk-after')[1].splitlines()
        after_local=audit_local_backups(ROOT,reports)
        result['local_backups_unchanged']=after_local==audit
        if after_local!=audit:raise ValueError('Local backup revalidation differs')
        result['passed']=(result['deleted_files']==audit['file_count'] and result['removed_directories']==audit['directory_count'])
    except Exception as error:result['error']=f'{type(error).__name__}: {error}'
    finally:
        result['finished_at']=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat();save()
    print(json.dumps(result,ensure_ascii=False,indent=2))
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
