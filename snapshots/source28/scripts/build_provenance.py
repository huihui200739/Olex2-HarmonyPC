#!/usr/bin/env python3
"""Bind a private diagnostic build snapshot, actual native inputs and HAP bytes.

This is local build traceability, not a signature, a reproducible-bytes claim,
proof of upstream GUI integration, or qualification of the complete port.
"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import shlex
import subprocess
import sys
import zipfile
from zoneinfo import ZoneInfo

SCHEMA = 1
EMBEDDED = 'resources/rawfile/OLEX2-BUILD-INPUTS.json'
SOURCE_DIRS = ('app', 'cmake', 'native', 'upstream')
EXCLUDED_DIRS = frozenset(('.git', '.hvigor', 'oh_modules', 'build', '.cxx', '__pycache__'))
RECIPES = ('scripts/build-app.sh', 'scripts/build_provenance.py', 'scripts/check-source.py', 'source.lock.json')
TOOL_ROLES = frozenset(('clang++', 'ld.lld', 'llvm-strip', 'cmake', 'ninja', 'node',
                        'python', 'hvigor', 'ohpm', 'hvigor-package', 'hvigor-ohos-package'))
GENERATED_INPUT = 'app/entry/src/main/resources/rawfile/OLEX2-BUILD-INPUTS.json'
LIMITS = [
    'Diagnostic subset only; original full GUI, renderer, Python and plugins are not certified.',
    'Unsigned debug traceability, not authenticated release provenance or a digital signature.',
    'Records the actual local build; does not assert independently reproducible HAP bytes.',
    'Native dependency evidence is compiler/Ninja input tracing, not ArkTS transitive dependency tracing.',
    'Tool hashes and SDK package metadata identify selected local tools, not an audit of the entire SDK.',
    'Emulator, physical-PC and runtime test evidence are separate and must bind the final HAP hash.',
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n').encode()


def record(path, label):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), f'Not a regular input: {label}')
    data = path.read_bytes()
    return {'path': label, 'bytes': len(data), 'sha256': digest(data)}


def relative_file(root, label):
    require(isinstance(label, str) and label and '\\' not in label, 'Invalid relative path')
    parts = PurePosixPath(label).parts
    require(not label.startswith('/') and all(p not in ('', '.', '..') for p in parts)
            and PurePosixPath(label).as_posix() == label, f'Invalid relative path: {label}')
    path = root / label
    require(path.resolve().is_relative_to(root.resolve()), f'Input outside project: {label}')
    # Refuse even internal symlinks; the capture must hash the copied regular file.
    require(not any(p.is_symlink() for p in (path, *path.parents) if p != root.parent),
            f'Symlink input: {label}')
    return path


def snapshot_files(root):
    output = []
    for directory in SOURCE_DIRS:
        base = root / directory
        require(base.is_dir(), f'Missing snapshot directory: {directory}')
        for path in sorted(base.rglob('*')):
            parts = path.relative_to(root).parts
            if any(p in EXCLUDED_DIRS for p in parts) or parts[-1] == 'local.properties':
                continue
            if path.as_posix() == (root / GENERATED_INPUT).as_posix():
                continue
            require(not path.is_symlink(), f'Symlink in snapshot: {path.relative_to(root)}')
            if path.is_file():
                label = path.relative_to(root).as_posix()
                output.append(record(path, label))
    return sorted(output, key=lambda item: item['path'])


def check_records(records):
    require(isinstance(records, list) and records, 'Missing input records')
    labels = []
    for item in records:
        require(isinstance(item, dict), 'Invalid file record')
        label = item.get('path')
        require(isinstance(label, str) and label and '\\' not in label
                and not label.startswith('/') and PurePosixPath(label).parts
                and '..' not in PurePosixPath(label).parts
                and str(PurePosixPath(label)) == label, 'Invalid recorded path')
        require(isinstance(item.get('bytes'), int) and item['bytes'] >= 0, 'Invalid file length')
        value = item.get('sha256', '')
        require(isinstance(value, str) and len(value) == 64
                and all(c in '0123456789abcdef' for c in value), 'Invalid SHA-256')
        labels.append(label)
    require(labels == sorted(set(labels)), 'Input records must be sorted and unique')


def capture(stage, workspace, tools, sdk, destination):
    stage, workspace = stage.resolve(), workspace.resolve()
    files = snapshot_files(stage)
    # A dependency resolver may create a lock absent from the workspace. No
    # other source may appear or change between copying and capture.
    for item in files:
        original = workspace / item['path']
        if original.is_file():
            require(record(original, item['path']) == item, f'Snapshot differs from source: {item["path"]}')
            item['origin'] = 'workspace'
        else:
            require(item['path'].startswith('app/') and original.name == 'oh-package-lock.json5',
                    f'Unexpected generated source: {item["path"]}')
            item['origin'] = 'resolved-local-dependency-lock'
    lock = json.loads((workspace / 'source.lock.json').read_text())
    metadata_paths = sorted(set(RECIPES) | {p['file'] for p in lock['patches']})
    metadata = [record(relative_file(workspace, p), p) for p in metadata_paths]
    for patch in lock['patches']:
        require(record(workspace / patch['file'], patch['file'])['sha256'] == patch['sha256'],
                'Patch differs from source lock')
    require(record(stage / 'upstream/LICENCE.txt', 'licence')['sha256'] == lock['license_sha256'],
            'Upstream licence differs from source lock')
    identity = json.loads((stage / 'app/AppScope/app.json5').read_text())['app']
    tool_records = []
    require(len(tools) == len({name for name, _ in tools}), 'Duplicate tool role')
    require({name for name, _ in tools} == TOOL_ROLES, 'Missing or unexpected build tool roles')
    for role, path in sorted(tools):
        # SDK launchers can be links. Hash their resolved bytes explicitly.
        actual = path.resolve(strict=True)
        tool_records.append(dict(role=role, filename=actual.name,
                                 **{k: v for k, v in record(actual, actual.name).items() if k != 'path'}))
    sdk_info = json.loads((sdk / 'oh-uni-package.json').read_text())
    manifest = dict(schema_version=SCHEMA, kind='olex2-diagnostic-build-inputs',
                    identity={k: identity[k] for k in ('bundleName', 'versionName', 'versionCode')},
                    full_port_complete=False, upstream_commit=lock['commit'],
                    files=files, snapshot_sha256=digest(canonical(files)), metadata=metadata,
                    tools=tool_records, sdk=sdk_info, limits=LIMITS)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(canonical(manifest))
    return manifest


def normalize_path(path, stage, sdk):
    path = path.resolve(strict=True)
    if path.is_relative_to(stage):
        return 'project/' + path.relative_to(stage).as_posix()
    require(path.is_relative_to(sdk), f'Native input outside snapshot/SDK: {path}')
    return 'native-sdk/' + path.relative_to(sdk).as_posix()


def native_evidence(stage, sdk, ninja, files):
    databases = list((stage / 'app/entry/.cxx').rglob('compile_commands.json'))
    require(len(databases) == 1, f'Expected one native compilation database, got {len(databases)}')
    database = databases[0]
    commands = json.loads(database.read_text())
    require(isinstance(commands, list) and commands, 'Empty native compilation database')
    file_map = {item['path']: item for item in files}
    compiled = []
    for command in commands:
        directory = Path(command['directory'])
        require(directory.resolve() == database.parent.resolve(), 'Unexpected compile command directory')
        source = Path(command['file'])
        if not source.is_absolute():
            source = directory / source
        label = normalize_path(source, stage, sdk)
        require(label.startswith('project/'), 'Native source not from staged project')
        item = record(source, label.removeprefix('project/'))
        require(all(item[k] == file_map.get(item['path'], {}).get(k) for k in item),
                'Compiled native source absent or changed since capture')
        arguments = command.get('arguments') or shlex.split(command['command'])
        require(arguments and Path(arguments[0]).resolve() == (sdk / 'llvm/bin/clang++').resolve(),
                'Unexpected native compiler')
        command_text = shlex.join(arguments).replace(str(stage), '$PROJECT').replace(str(sdk), '$NATIVE_SDK')
        compiled.append(dict(**item, command=command_text))
    compiled.sort(key=lambda item: item['path'])
    require(len(compiled) == len({item['path'] for item in compiled}), 'Duplicate compiled native source')
    # deps reports the actual header closure retained by Ninja after compilation.
    result = subprocess.run([str(ninja), '-C', str(database.parent), '-t', 'deps'],
                            capture_output=True, text=True, check=True)
    dependencies = {}
    objects = 0
    for line in result.stdout.splitlines():
        if not line:
            continue
        if not line.startswith(' '):
            require('#deps ' in line and '(VALID)' in line, 'Missing or stale native dependency record')
            objects += 1
        else:
            path = Path(line.strip())
            if not path.is_absolute():
                path = database.parent / path
            label = normalize_path(path, stage, sdk)
            dependencies[label] = record(path.resolve(), label)
    require(objects == len(commands), 'Native dependency records do not cover all compiled objects')
    require(all('project/' + item['path'] in dependencies for item in compiled),
            'A compiled source is missing from native dependencies')
    for label, item in dependencies.items():
        if label.startswith('project/'):
            original = file_map.get(label.removeprefix('project/'))
            require(original is not None and all(item[k] == original[k] for k in ('bytes', 'sha256')),
                    'Native project dependency absent or changed since capture')
    metadata = [record(path, path.relative_to(stage).as_posix())
                for path in sorted(database.parent.iterdir())
                if path.name in ('compile_commands.json', 'CMakeCache.txt', 'build.ninja', '.ninja_log', '.ninja_deps')]
    return dict(compiled_sources=compiled, dependencies=[dependencies[p] for p in sorted(dependencies)],
                object_count=objects, build_metadata=metadata)


def zip_records(z):
    names = z.namelist()
    require(len(names) == len(set(names)), 'Duplicate HAP member names')
    for name in names:
        require(not name.startswith('/') and '\\' not in name and '..' not in PurePosixPath(name).parts,
                'Invalid HAP member name')
    return [dict(path=name, bytes=len(data), sha256=digest(data))
            for name in sorted(names) if not name.endswith('/') for data in [z.read(name)]]


def finalize(stage, sdk, ninja, hap, inputs, output):
    stage, sdk = stage.resolve(), sdk.resolve()
    manifest = json.loads(inputs.read_text())
    require(manifest['schema_version'] == SCHEMA, 'Unsupported input manifest schema')
    actual = snapshot_files(stage)
    expected = [{k: item[k] for k in ('path', 'bytes', 'sha256')} for item in manifest['files']]
    require(actual == expected, 'Source snapshot changed during build')
    evidence = native_evidence(stage, sdk, ninja, manifest['files'])
    with zipfile.ZipFile(hap) as z:
        require(z.read(EMBEDDED) == inputs.read_bytes(), 'Packaged input manifest does not match build capture')
        members = zip_records(z)
        identity = json.loads(z.read('module.json'))['app']
        require(all(identity[k] == v for k, v in manifest['identity'].items()), 'Packaged identity mismatch')
    packaged = {item['path']: item for item in members}
    matches = []
    native = packaged['libs/arm64-v8a/libolex2native.so']
    for path in (stage / 'app/entry').rglob('libolex2native.so'):
        if path.is_file() and digest(path.read_bytes()) == native['sha256']:
            matches.append(path.relative_to(stage).as_posix())
    require(matches, 'Packaged native library not found in this fresh build')
    data = dict(schema_version=SCHEMA, kind='olex2-diagnostic-build-result',
                created_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),
                full_port_complete=False, input_manifest_sha256=digest(inputs.read_bytes()),
                hap=record(hap, hap.name), members=members, native=evidence,
                packaged_native_build_matches=sorted(matches), limits=LIMITS)
    output.write_bytes(canonical(data))
    return data


def verify(hap, provenance, workspace=None):
    sidecar = json.loads(provenance.read_text())
    if sidecar.get('schema_version') == 2:
        from build_provenance_graphics import verify as verify_graphics
        return verify_graphics(hap, provenance, workspace)
    require(sidecar.get('schema_version') == SCHEMA
            and sidecar.get('kind') == 'olex2-diagnostic-build-result', 'Unsupported build result')
    require(sidecar.get('full_port_complete') is False, 'Incorrect completion claim')
    require(record(hap, hap.name) == sidecar['hap'], 'HAP bytes differ from build result')
    with zipfile.ZipFile(hap) as z:
        manifest_bytes = z.read(EMBEDDED)
        manifest = json.loads(manifest_bytes)
        require(manifest.get('schema_version') == SCHEMA
                and manifest.get('kind') == 'olex2-diagnostic-build-inputs', 'Unsupported input manifest')
        require(manifest.get('full_port_complete') is False, 'Incorrect input completion claim')
        require(digest(manifest_bytes) == sidecar['input_manifest_sha256'], 'Input manifest digest mismatch')
        require(zip_records(z) == sidecar['members'], 'Packaged members differ from build result')
        identity = json.loads(z.read('module.json'))['app']
        require(all(identity[k] == v for k, v in manifest['identity'].items()), 'HAP identity mismatch')
    check_records(manifest['files'])
    check_records(manifest['metadata'])
    for item in manifest['files']:
        require(item.get('origin') in ('workspace', 'resolved-local-dependency-lock'),
                'Unknown snapshot input origin')
    tools = manifest['tools']
    require(isinstance(tools, list) and [item['role'] for item in tools] == sorted(TOOL_ROLES),
            'Missing or unexpected build tool evidence')
    check_records([dict(path=item['role'], bytes=item['bytes'], sha256=item['sha256']) for item in tools])
    require(set(manifest['identity']) == {'bundleName', 'versionName', 'versionCode'},
            'Incomplete app identity')
    require(digest(canonical(manifest['files'])) == manifest['snapshot_sha256'], 'Source snapshot digest mismatch')
    files = {item['path']: item for item in manifest['files']}
    native = sidecar['native']
    check_records(native['compiled_sources'])
    check_records(native['dependencies'])
    require(native['object_count'] == len(native['compiled_sources']) > 0, 'Native object count mismatch')
    dependencies = {item['path']: item for item in native['dependencies']}
    for item in native['compiled_sources']:
        require(item['path'] in files and all(item[k] == files[item['path']][k] for k in ('bytes', 'sha256')),
                'Compiled native source not bound to snapshot')
        dependency = dependencies.get('project/' + item['path'])
        require(dependency and all(item[k] == dependency[k] for k in ('bytes', 'sha256')),
                'Compiled native source not bound to dependency evidence')
    for label, item in dependencies.items():
        if label.startswith('project/'):
            original = files.get(label.removeprefix('project/'))
            require(original and all(item[k] == original[k] for k in ('bytes', 'sha256')),
                    'Project header not bound to snapshot')
        else:
            require(label.startswith('native-sdk/'), 'Unexpected native dependency namespace')
    require(sidecar.get('packaged_native_build_matches'), 'Missing native build match evidence')
    if workspace is not None:
        workspace = workspace.resolve()
        for item in manifest['files'] + manifest['metadata']:
            if item.get('origin', 'workspace') == 'workspace':
                current = record(relative_file(workspace, item['path']), item['path'])
                require(all(current[k] == item[k] for k in current), f'Workspace changed: {item["path"]}')
        for current in snapshot_files(workspace):
            original = files.get(current['path'])
            require(original and all(current[k] == original[k] for k in current),
                    f'Workspace added or changed source: {current["path"]}')
        lock = json.loads((workspace / 'source.lock.json').read_text())
        require(lock['commit'] == manifest['upstream_commit'], 'Upstream commit mismatch')
    return dict(passed=True, hap_sha256=sidecar['hap']['sha256'],
                input_manifest_sha256=sidecar['input_manifest_sha256'],
                snapshot_sha256=manifest['snapshot_sha256'], staged_files=len(files),
                compiled_native_sources=len(native['compiled_sources']),
                native_dependency_files=len(dependencies), tools=len(manifest['tools']),
                sdk=manifest['sdk'], full_port_complete=False,
                workspace_matches=workspace is not None, limits=LIMITS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    cap = sub.add_parser('capture')
    cap.add_argument('--stage', type=Path, required=True)
    cap.add_argument('--workspace', type=Path, required=True)
    cap.add_argument('--sdk', type=Path, required=True)
    cap.add_argument('--tool', action='append', default=[], metavar='ROLE=PATH')
    cap.add_argument('--output', type=Path, required=True)
    end = sub.add_parser('finalize')
    for name in ('stage', 'sdk', 'ninja', 'hap', 'inputs', 'output'):
        end.add_argument('--' + name, type=Path, required=True)
    check = sub.add_parser('verify')
    check.add_argument('--hap', type=Path, required=True)
    check.add_argument('--provenance', type=Path, required=True)
    check.add_argument('--workspace', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'capture':
            tools = []
            for value in args.tool:
                role, path = value.split('=', 1)
                tools.append((role, Path(path)))
            result = capture(args.stage, args.workspace, tools, args.sdk, args.output)
            print(f'Captured {len(result["files"])} staged files; {result["snapshot_sha256"]}')
        elif args.action == 'finalize':
            result = finalize(args.stage, args.sdk, args.ninja, args.hap, args.inputs, args.output)
            print(f'Bound {len(result["native"]["compiled_sources"])} native sources to {result["hap"]["sha256"]}')
        else:
            print(json.dumps(verify(args.hap, args.provenance, args.workspace), ensure_ascii=False, indent=2))
    except (ValueError, KeyError, OSError, TypeError, zipfile.BadZipFile, subprocess.SubprocessError) as error:
        print(f'Build provenance failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
