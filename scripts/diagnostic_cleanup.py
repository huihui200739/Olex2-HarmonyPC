"""Exact, separately authorized removal of backed-up synthetic diagnostics only.

No reset, uninstall, recursive removal, wildcard removal, or local deletion.
Every remote unlink is bound to the approved pathname and freshly checked SHA.
"""
from __future__ import annotations
import json
from pathlib import PurePosixPath
import re
import shlex
from diagnostic_backup_audit import BUNDLE, DIRECTORY, REMOTE_FILES, SHA256, safe_name

TARGET = '127.0.0.1:5555'
USER_REQUEST = '清理这些已备份的模拟器旧测试副本，来腾出空间继续安装测试吧，完成百分之多少了'


def validate_authorization(plan, original_sha, authorization, audit):
    if (authorization.get('authorized') is not True
            or authorization.get('user_request') != USER_REQUEST
            or authorization.get('plan_sha256') != original_sha
            or authorization.get('target') != TARGET
            or authorization.get('bundle_name') != BUNDLE
            or authorization.get('retain_local_backups') is not True):
        raise ValueError('Missing exact explicit cleanup authorization')
    if plan.get('target') != TARGET or plan.get('bundle_name') != BUNDLE:
        raise ValueError('Different cleanup target')
    for key in ('local_backups_must_be_retained', 'failed_diagnostics_must_be_retained',
                'current25_diagnostics_must_be_retained'):
        if plan.get(key) is not True:
            raise ValueError('Required preservation boundary missing')
    proposed = []
    for row in audit['directories']:
        validate_row(row)
        proposed.append({key: row[key] for key in
                         ('directory', 'remote', 'bytes', 'receipt', 'receipt_sha256')})
        proposed[-1]['file_count'] = len(row['files'])
    if proposed != plan.get('candidates'):
        raise ValueError('Current complete candidate manifest differs from authorization')
    for key, audit_key in (('directory_count','directory_count'),('file_count','file_count'),('payload_bytes','payload_bytes')):
        if plan.get(key) != audit.get(audit_key):
            raise ValueError('Authorized cleanup totals differ')
    if len({row['directory'] for row in audit['directories']}) != len(audit['directories']):
        raise ValueError('Duplicate cleanup directory')
    return True


def validate_row(row):
    name = row.get('directory')
    match = DIRECTORY.fullmatch(name) if isinstance(name, str) else None
    if not match or row.get('remote') != f'{REMOTE_FILES}/{name}' or row.get('pid') != int(match.group(2)):
        raise ValueError('Unexpected cleanup directory')
    files = row.get('files')
    if not isinstance(files, dict) or not files:
        raise ValueError('Missing exact cleanup files')
    for filename, info in files.items():
        safe_name(filename)
        if not isinstance(info.get('sha256'), str) or not SHA256.fullmatch(info['sha256']):
            raise ValueError('Invalid cleanup digest')
    if row.get('file_count',len(files)) != len(files) or row.get('bytes') != sum(v['bytes'] for v in files.values()):
        raise ValueError('Cleanup row counts differ')


def deletion_script(row):
    """One directory, only listed regular files, then listed empty parents."""
    validate_row(row)
    q = shlex.quote
    paths = {f"{row['remote']}/{name}": info['sha256'] for name,info in row['files'].items()}
    directories = {row['remote']}
    for path in paths:
        parent = PurePosixPath(path).parent
        while str(parent) != row['remote']:
            directories.add(str(parent)); parent = parent.parent
    lines = ['set -eu', f'active=$(pidof {BUNDLE} || true)',
             f'case " $active " in *" {row["pid"]} "*) exit 71 ;; esac',
             f'[ -d {q(REMOTE_FILES)} ] && [ ! -L {q(REMOTE_FILES)} ]']
    for directory in sorted(directories):
        lines.append(f'[ -d {q(directory)} ] && [ ! -L {q(directory)} ]')
    lines.append('set -- ' + ' '.join(q(item) for path,digest in sorted(paths.items()) for item in (digest,path)))
    lines += ['while [ "$#" -gt 0 ]; do', '  expected=$1; file=$2; shift 2',
              '  [ -f "$file" ] && [ ! -L "$file" ]',
              '  actual=$(sha256sum "$file")', '  [ "$actual" = "$expected  $file" ]',
              '  rm "$file"', '  printf "__DELETED__%s\\n" "$file"', 'done']
    for directory in sorted(directories, key=lambda p:(-len(PurePosixPath(p).parts),p)):
        lines.append(f'rmdir {q(directory)}')
    lines += [f'printf "__DIRECTORY_REMOVED__%s\\n" {q(row["remote"])}',
              'printf "__CLEANUP_STATUS__0\\n"', '']
    return '\n'.join(lines)


def parse_deletion(row, text, returncode):
    paths = {f"{row['remote']}/{name}" for name in row['files']}
    lines = text.replace('\r\n','\n').splitlines()
    observed = [line.removeprefix('__DELETED__') for line in lines if line.startswith('__DELETED__')]
    if len(observed) != len(set(observed)) or set(observed) - paths:
        raise ValueError('Unexpected or duplicate deletion receipt')
    expected = [f'__DELETED__{path}' for path in sorted(paths)] + [
        f'__DIRECTORY_REMOVED__{row["remote"]}', '__CLEANUP_STATUS__0']
    if returncode or lines != expected:
        raise ValueError(f'Incomplete exact directory removal ({len(observed)}/{len(paths)} recorded)')
    return observed


def preservation_script(audit):
    for row in audit['directories']: validate_row(row)
    # This app-private runtime is inaccessible to HDC and is never traversed.
    # Record public directory metadata, not a claim about protected contents.
    opaque = REMOTE_FILES + '/olex2-runtime-v1'
    prune = ' -o '.join('-path '+shlex.quote(path) for path in [row['remote'] for row in audit['directories']]+[opaque])
    return '\n'.join(['set -eu', 'printf "__OPAQUE_RUNTIME_METADATA__\\n"',
                      f"stat -c '%i:%s:%a:%Y:%Z %n' {shlex.quote(opaque)}",
                      'printf "__PROTECTED_FILE_HASHES__\\n"', f'find {shlex.quote(REMOTE_FILES)} \\( {prune} \\) -prune -o -type f -exec sha256sum {{}} +',
                      'printf "__PRESERVATION_STATUS__0\\n"', ''])


def parse_preservation(text):
    lines = text.replace('\r\n','\n').splitlines()
    if not lines or lines[-1] != '__PRESERVATION_STATUS__0':
        raise ValueError('Incomplete protected file observation')
    if len(lines)<4 or lines[0]!='__OPAQUE_RUNTIME_METADATA__' or lines[2]!='__PROTECTED_FILE_HASHES__':
        raise ValueError('Missing opaque runtime metadata boundary')
    if not re.fullmatch(r'[0-9]+:[0-9]+:[0-7]+:[0-9]+:[0-9]+ '+re.escape(REMOTE_FILES+'/olex2-runtime-v1'),lines[1]):
        raise ValueError('Invalid public runtime directory metadata')
    observed = {}
    for line in lines[3:-1]:
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)',line)
        if not match or not match.group(2).startswith(REMOTE_FILES+'/') or match.group(2) in observed:
            raise ValueError('Invalid protected file digest')
        digest,path = match.groups(); observed[path]=digest
    return dict(readable_files=observed,opaque_runtime_directory_metadata=lines[1],protected_runtime_contents_hashed=False)

# Standard HDC debug-bundle file access, not root/smode/chmod or a general shell.
APP_FILES = './data/storage/el2/base/haps/entry/files'


def app_file_commands(row):
    validate_row(row)
    base = APP_FILES+'/'+row['directory']
    files = [base+'/'+name for name in sorted(row['files'])]
    directories={base}
    for name in row['files']:
        parts=PurePosixPath(name).parts[:-1]
        for index in range(1,len(parts)+1):directories.add(base+'/'+'/'.join(parts[:index]))
    parents=sorted(directories,key=lambda path:(-len(PurePosixPath(path).parts),path))
    return (['rm',*files],['rmdir',*parents])
