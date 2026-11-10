"""Read-only audit of already-received synthetic graphics diagnostics.

This module never deletes, installs, restarts, or modifies device content.  Its
output is evidence for a later, separately authorized storage decision, not a
cleanup authorization or application qualification.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex

BUNDLE = 'com.example.olex2harmonypc'
REMOTE_FILES = f'/data/app/el2/100/base/{BUNDLE}/haps/entry/files'
DIRECTORY = re.compile(r'original-(font|renderer|texture-load)-pid-([1-9][0-9]*)-run-([1-9][0-9]*)\Z')
SHA256 = re.compile(r'[0-9a-f]{64}\Z')


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def safe_name(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_./-]+', value):
        raise ValueError('Unexpected diagnostic filename')
    path = PurePosixPath(value)
    if path.is_absolute() or any(p in ('', '.', '..') for p in value.split('/')):
        raise ValueError('Noncanonical diagnostic filename')
    return value


def owned_file(root: Path, path: Path) -> Path:
    """Reject traversal and symlinks in every project-relative component."""
    root = root.resolve()
    path = Path(path)
    if not path.is_absolute():
        path = root / path
    try:
        relative = path.relative_to(root)
    except ValueError as error:
        raise ValueError('Evidence outside this project') from error
    current = root
    for part in relative.parts:
        if part in ('.', '..'):
            raise ValueError('Noncanonical evidence path')
        current = current / part
        if current.is_symlink():
            raise ValueError('Symlink in evidence path')
    if not current.is_file():
        raise ValueError('Missing regular evidence file')
    return current



def create_owned_directory(root: Path, path: Path) -> Path:
    """Create a new local evidence directory, never following project symlinks."""
    root = root.resolve()
    path = Path(path)
    if not path.is_absolute():
        path = root / path
    try:
        relative = path.relative_to(root)
    except ValueError as error:
        raise ValueError('Output outside this project') from error
    if not relative.parts or any(part in ('.', '..') for part in relative.parts):
        raise ValueError('Noncanonical output path')
    current = root
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            raise ValueError('Symlink in output path')
    path.mkdir(parents=True, exist_ok=False)
    return path


def receipt_directory(root: Path, report: Path, receipt: Path) -> dict:
    receipt = owned_file(root, receipt)
    value = json.loads(receipt.read_text())
    name = value.get('artifact_subdirectory', '')
    match = DIRECTORY.fullmatch(name) if isinstance(name, str) else None
    if not match:
        raise ValueError('Not a synthetic graphics diagnostic directory')
    route, pid, run = match.groups()
    if str(value.get('pid')) != pid or type(value.get('run')) is not int or value['run'] != int(run):
        raise ValueError('Receipt PID/run differs from its directory')
    identity = value.get('identity')
    if identity is not None and (not isinstance(identity, dict) or identity.get('bundleName') != BUNDLE):
        raise ValueError('Receipt belongs to a different application')
    integrated = {'renderer': 'integrated', 'font': 'font', 'texture-load': 'texture-load'}[route]
    sidecar = f'shared-original-{integrated}-run-{run}.json'
    files = value.get('files')
    if not isinstance(files, dict) or sidecar not in files:
        raise ValueError('Missing original receipt observation')
    verified = {}
    for filename, meta in files.items():
        safe_name(filename)
        if not isinstance(meta, dict):
            raise ValueError('Invalid received-file metadata')
        size, digest = meta.get('bytes'), meta.get('sha256')
        if type(size) is not int or size < 0 or not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise ValueError('Invalid size/hash in original receipt')
        if meta.get('target_sha256') != digest:
            raise ValueError('Original target/host receipt hashes differ')
        local = owned_file(root, receipt.parent / filename)
        if local.stat().st_size != size or sha256(local) != digest:
            raise ValueError('Local backup changed since receipt')
        if filename != sidecar:
            verified[filename] = dict(bytes=size, sha256=digest, backup=str(local.relative_to(root)))
    observation = json.loads((receipt.parent / sidecar).read_text())
    original = observation.get('original', {})
    if (observation.get('run') != int(run) or str(original.get('pid')) != pid
            or original.get('run') != int(run) or original.get('artifact_subdirectory') != name):
        raise ValueError('Original observation does not bind receipt PID/run/directory')
    if not verified:
        raise ValueError('No diagnostic files backed up')
    return dict(directory=name, remote=f'{REMOTE_FILES}/{name}', pid=int(pid), run=int(run),
                qualification_report=str(report.relative_to(root)),
                qualification_report_sha256=sha256(report),
                receipt=str(receipt.relative_to(root)), receipt_sha256=sha256(receipt),
                files=verified, bytes=sum(row['bytes'] for row in verified.values()))


def audit_local_backups(root: Path, reports: list[Path]) -> dict:
    root = root.resolve()
    if not reports:
        raise ValueError('Explicit successful reports required')
    directories = {}
    sources = []
    for item in reports:
        report = owned_file(root, item)
        relative = report.relative_to(root).as_posix()
        if not (relative.startswith('reports/emulator/') or relative.startswith('out/gx-application-evaluation/')):
            raise ValueError('Not an owned graphics evidence report')
        if report.name != 'result.json' or json.loads(report.read_text()).get('passed') is not True:
            raise ValueError('Only explicitly successful reports may be proposed')
        runtime = report.parent / 'runtime'
        if runtime.is_symlink() or any(path.is_symlink() for path in runtime.rglob('*')):
            raise ValueError('Symlink in qualified diagnostic runtime tree')
        receipts = sorted(runtime.rglob('received-artifacts.json'))
        if not receipts:
            raise ValueError('Successful report has no received diagnostic directories')
        for receipt in receipts:
            # Do not inherit qualification from an unrelated nested result.
            for parent in receipt.parents:
                if parent == report.parent:
                    break
                if (parent / 'result.json').is_file():
                    raise ValueError('Nested result requires explicit separate qualification')
            row = receipt_directory(root, report, receipt)
            name = row['directory']
            if name in directories:
                prior = directories[name]
                if {k:(v['bytes'],v['sha256']) for k,v in prior['files'].items()} != {
                        k:(v['bytes'],v['sha256']) for k,v in row['files'].items()}:
                    raise ValueError('Conflicting backups for the same remote directory')
            else:
                directories[name] = row
        sources.append(dict(path=relative, sha256=sha256(report)))
    rows = [directories[name] for name in sorted(directories)]
    return dict(scope='Read-only old synthetic graphics diagnostic backup audit; not cleanup or app qualification',
                local_backups_verified=True, device_observed=False,
                deletion_authorized=False, deleted_files=0, files_modified=False,
                reports=sources, directories=rows, directory_count=len(rows),
                file_count=sum(len(row['files']) for row in rows),
                payload_bytes=sum(row['bytes'] for row in rows),
                full_port_complete=False, functional_completion_percent=None)


def readonly_device_query(audit: dict) -> str:
    paths = []
    for row in audit['directories']:
        name = row['directory']
        if not DIRECTORY.fullmatch(name) or row['remote'] != f'{REMOTE_FILES}/{name}':
            raise ValueError('Unexpected proposed device directory')
        paths.append(row['remote'])
    if not paths:
        raise ValueError('Empty device audit')
    return '\n'.join([
        'set -- ' + ' '.join(shlex.quote(path) for path in paths),
        "printf '__AUDIT_PID__\\n'", f'pidof {BUNDLE} || true',
        "printf '__AUDIT_DF__\\n'", f'df -k {REMOTE_FILES}',
        "printf '__AUDIT_LINKS__\\n'", 'find "$@" -type l -print', 'links_status=$?',
        "printf '__AUDIT_HASHES__\\n'", 'find "$@" -type f -exec sha256sum {} +', 'hashes_status=$?',
        "printf '__AUDIT_STATUS__%s,%s\\n' \"$links_status\" \"$hashes_status\"", ''])


def audit_device_inventory(audit: dict, text: str) -> dict:
    markers = ['__AUDIT_PID__', '__AUDIT_DF__', '__AUDIT_LINKS__', '__AUDIT_HASHES__']
    lines = text.replace('\r\n', '\n').splitlines()
    positions = []
    for marker in markers:
        if lines.count(marker) != 1:
            raise ValueError('Incomplete or duplicate device inventory marker')
        positions.append(lines.index(marker))
    if positions != sorted(positions) or positions[0] != 0:
        raise ValueError('Unexpected device inventory order')
    if not lines or lines[-1] != '__AUDIT_STATUS__0,0':
        raise ValueError('Device inventory command did not complete successfully')
    pid_text = ' '.join(lines[positions[0]+1:positions[1]])
    if pid_text and not re.fullmatch(r'[0-9]+(?: [0-9]+)*', pid_text):
        raise ValueError('Unexpected current process observation')
    active = {int(pid) for pid in pid_text.split()}
    if any(row['pid'] in active for row in audit['directories']):
        raise ValueError('Proposed diagnostic directory belongs to the active process')
    disk = lines[positions[1]+1:positions[2]]
    if len(disk) < 2 or any(not line.strip() for line in disk):
        raise ValueError('Missing device disk observation')
    if lines[positions[2]+1:positions[3]]:
        raise ValueError('Device diagnostic tree contains symlinks or listing errors')
    expected = {}
    for row in audit['directories']:
        if not DIRECTORY.fullmatch(row['directory']) or row['remote'] != f"{REMOTE_FILES}/{row['directory']}":
            raise ValueError('Unexpected device directory')
        for name, info in row['files'].items():
            expected[f"{row['remote']}/{safe_name(name)}"] = info['sha256']
    observed = {}
    for line in lines[positions[3]+1:-1]:
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        if not match:
            raise ValueError('Malformed device file digest or command failure')
        digest, path = match.groups()
        if path in observed:
            raise ValueError('Duplicate remote file digest')
        observed[path] = digest
    if observed.keys() != expected.keys():
        missing, extra = expected.keys()-observed.keys(), observed.keys()-expected.keys()
        raise ValueError(f'Device directory file set differs: {len(missing)} missing, {len(extra)} extra')
    if any(observed[path] != digest for path,digest in expected.items()):
        raise ValueError('Current device bytes differ from verified local backup')
    return dict(device_observed=True, exact_device_file_sets_verified=True,
                current_device_hashes_equal_local_backups=True, current_processes=sorted(active),
                disk_observation=disk, directory_count=len(audit['directories']),
                file_count=len(expected), payload_bytes=audit['payload_bytes'],
                deletion_authorized=False, deleted_files=0, files_modified=False)
