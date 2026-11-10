#!/usr/bin/env python3
"""Bounded real-window LoadTextures validation; not the full diagnostic UI suite."""
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
from app_report_log import parse_app_reports
from build_provenance_png import verify
from gx_target_identity import PC_IDENTITY_QUERY, validate_pc_emulator_identity
from renderer_probe_identity import package_identity, installed_identity
from texture_load_window_validation import IDENTITY, validate
from prepare_texture_load_support import capture_profile

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', required=True)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--hap', type=Path, required=True)
    p.add_argument('--hdc', default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
    a = p.parse_args()
    if a.target != '127.0.0.1:5555':
        raise ValueError('Only the existing explicitly selected emulator is permitted')
    hap, source = a.hap.resolve(), a.source.resolve()
    identity = package_identity(hap)
    if identity != IDENTITY:
        raise ValueError('Only exact candidate15 accepted')
    report = ROOT/'reports/emulator'/('texture-load-window-' + datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    report.mkdir(parents=True)
    result = dict(passed=False, target=a.target, identity=identity, hap_sha256=hashlib.sha256(hap.read_bytes()).hexdigest(),
                  rounds=[], full_ui_qualification=False, full_port_complete=False, functional_completion_percent=None,
                  physical_pc_verified=False, installed_target_code_directly_hashed=False)
    bundle = identity['bundleName']
    remote = '/data/app/el2/100/base/' + bundle + '/haps/entry/files'
    env = dict(os.environ, PYTHONPATH=str(ROOT/'scripts'))
    def hdc(*args):
        command = [a.hdc, '-t', a.target, *map(str, args)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=80)
        text = run.stdout + run.stderr
        with (report/'hdc-commands.log').open('a') as f:
            f.write(' '.join(shlex.quote(x) for x in command) + '\n' + text + '\n')
        if run.returncode or any(s.lstrip().startswith('[Fail]') for s in text.splitlines()):
            raise RuntimeError(text)
        return text
    def shell(command):
        return hdc('shell', command)
    def invoke(script, *args):
        command = [sys.executable, str(script), *map(str, args)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=180, env=env)
        with (report/'commands.log').open('a') as f:
            f.write(' '.join(shlex.quote(x) for x in command) + '\n' + run.stdout + run.stderr + '\n')
        if run.returncode:
            raise RuntimeError(str(script) + ': ' + run.stdout + run.stderr)
    def start():
        shell('aa force-stop ' + shlex.quote(bundle))
        deadline = time.monotonic() + 15
        while shell('pidof ' + shlex.quote(bundle)).strip():
            if time.monotonic() >= deadline:
                raise RuntimeError('Existing evaluation process did not stop')
            time.sleep(.2)
        shell('aa start -a EntryAbility -b ' + shlex.quote(bundle))
    def receive(run_number, folder):
        folder.mkdir(parents=True, exist_ok=False)
        pid = shell('pidof ' + shlex.quote(bundle)).strip()
        if not pid.isdigit():
            raise ValueError('No current evaluation process')
        filename = f'shared-original-texture-load-run-{run_number}.json'
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            try:
                native = json.loads(shell('cat ' + shlex.quote(remote+'/'+filename)))
                if native.get('run') == run_number and str(native.get('original', {}).get('pid')) == pid:
                    break
            except (ValueError, AttributeError):
                pass
            time.sleep(.2)
        else:
            raise RuntimeError('No current process/run texture-load observation')
        artifact = f'original-texture-load-pid-{pid}-run-{run_number}'
        if native['original'].get('artifact_subdirectory') != artifact:
            raise ValueError('Texture-load artifact outside the expected current run')
        root = remote+'/'+artifact
        names = shell('find ' + shlex.quote(root) + ' -type f').splitlines() + [remote+'/'+filename]
        files = {}
        for target_path in names:
            if target_path == remote+'/'+filename:
                name = filename
            elif target_path.startswith(root+'/'):
                name = target_path[len(root)+1:]
            else:
                raise ValueError('Artifact outside current process/run: ' + target_path)
            if not re.fullmatch(r'[A-Za-z0-9_./-]+', name) or '..' in Path(name).parts or Path(name).is_absolute():
                raise ValueError('Unexpected artifact path')
            target_hash = shell('sha256sum ' + shlex.quote(target_path)).split()[0]
            local = folder/name; local.parent.mkdir(parents=True, exist_ok=True)
            if local.exists():
                raise FileExistsError(local)
            hdc('file', 'recv', target_path, local)
            raw = local.read_bytes(); digest = hashlib.sha256(raw).hexdigest()
            if digest != target_hash:
                raise ValueError('Received target artifact differs: ' + name)
            files[name] = dict(bytes=len(raw), sha256=digest, target_sha256=target_hash)
        if shell('pidof ' + shlex.quote(bundle)).strip() != pid:
            raise ValueError('Process changed while receiving evidence')
        (folder/'received-artifacts.json').write_text(json.dumps(dict(pid=int(pid), run=run_number,
            artifact_subdirectory=artifact, identity=identity, hap_sha256=result['hap_sha256'], files=files), indent=2)+'\n')
        return native
    try:
        result['device_identity'] = validate_pc_emulator_identity(shell(PC_IDENTITY_QUERY))
        result['build_verification'] = verify(hap, hap.with_suffix('.provenance.json'), source)
        (report/'capture-profile.py').write_text(capture_profile(ROOT))
        hdc('install', hap)
        dump = shell('bm dump -n ' + shlex.quote(bundle))
        (report/'installed-bundle.log').write_text(dump)
        package = json.loads(dump[dump.index('{'):])
        for node in (package, package['applicationInfo']):
            if node['versionName'] != identity['versionName'] or node['versionCode'] != identity['versionCode']:
                raise ValueError('Actual installed candidate differs')
        previous = None; owners = set()
        for index, label in enumerate(('fresh', 'restart-1', 'restart-2', 'restart-3', 'repeat-2', 'repeat-3')):
            if index < 4:
                start(); previous = None
            run_number = 1 if index < 4 else index-2
            before = report/'runtime'/(label+'-before'); after = report/'runtime'/(label+'-after')
            folder = report/'runtime'/(label+'-texture-load')
            common = ['--target', a.target, '--hdc', a.hdc, '--hap', hap, '--report', report]
            invoke(report/'capture-profile.py', *common, '--label', label+'-before')
            invoke(ROOT/'scripts/original_renderer_click.py', '--target', a.target, '--hdc', a.hdc,
                   '--layout', before/'layout.json', '--id', 'olex2-original-texture-load-probe')
            receive(run_number, folder)
            invoke(report/'capture-profile.py', *common, '--label', label+'-after')
            qualified = validate(folder, previous)
            before_ui = json.loads((before/'native-verification.json').read_text())
            after_ui = json.loads((after/'native-verification.json').read_text())
            native = json.loads((folder/f'shared-original-texture-load-run-{run_number}.json').read_text())['original']['legacy']['texture_load']
            ui_checks = []
            def check(name, value):
                ui_checks.append(dict(name=name, passed=bool(value)))
            check('same initialized runtime and saved workspace', before_ui['applicationRuntime'] == after_ui['applicationRuntime'] and before_ui['workspaceState'] == after_ui['workspaceState'])
            check('actual UI package identity before and after', before_ui['installedPackage'] == after_ui['installedPackage'] == installed_identity(identity))
            for when, ui in (('before', before_ui), ('after', after_ui)):
                live = native['log_'+when]
                check('UI reflects actual original log '+when, all(ui['originalLog'][key] == live[key] for key in
                      ('posted', 'dropped', 'truncated', 'captureFailures', 'clearCount', 'clearedEntries', 'textSha256')) and
                      ui['originalLog']['entryCount'] == len(live['entries']))
            qualified['ui_checks'] = ui_checks
            qualified['passed'] = qualified['passed'] and all(c['passed'] for c in ui_checks)
            (folder/'independent-validation.json').write_text(json.dumps(qualified, indent=2)+'\n')
            result['rounds'].append(dict(label=label, validation=qualified, folder=str(folder.relative_to(ROOT))))
            print(label, 'PASS' if qualified['passed'] else 'FAIL', flush=True)
            if not qualified['passed']:
                raise ValueError('Actual texture-load evidence failed: ' + label)
            owners.add(qualified['observation']['pid'])
            if index >= 4 and not qualified['actual_context_migration_verified']:
                raise ValueError('Missing actual 2D context handoff')
            previous = folder
        if len(owners) != 4:
            raise ValueError('Four independent actual processes required')
        result['passed'] = True
    except Exception as error:
        result['error'] = type(error).__name__ + ': ' + str(error)
    finally:
        for path in (ROOT/'scripts/texture_load_window_validation.py', ROOT/'scripts/run-texture-load-window-smoke.py',
                     ROOT/'scripts/prepare_texture_load_support.py', ROOT/'experiments/gx-texture-load/synthetic-fixtures.json'):
            shutil.copy2(path, report/path.name)
        (report/'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(passed=result['passed'], report_directory=str(report), rounds=len(result['rounds']), error=result.get('error')), ensure_ascii=False, indent=2))
    return 0 if result['passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
