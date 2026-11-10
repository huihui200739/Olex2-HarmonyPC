#!/usr/bin/env python3
"""Verify the frozen backup and optionally restore missing local metadata/source.

No SDK changes, installation, device operations or overwrites of existing files.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def verify_snapshot():
    manifest = json.loads((ROOT / 'snapshots/source28.manifest.json').read_text())
    snapshot = ROOT / 'snapshots/source28'
    records = manifest['records']
    actual = sorted(str(p.relative_to(snapshot)) for p in snapshot.rglob('*') if p.is_file())
    expected = sorted(r['path'] for r in records)
    if actual != expected or len(actual) != manifest['files']:
        raise RuntimeError('Snapshot file set differs from the manifest')
    for r in records:
        p = snapshot / r['path']
        if p.is_symlink() or p.stat().st_size != r['bytes']:
            raise RuntimeError('Unexpected file type/size: ' + r['path'])
        if hashlib.sha256(p.read_bytes()).hexdigest() != r['sha256']:
            raise RuntimeError('Snapshot digest mismatch: ' + r['path'])
    print(f"Verified exact bytes for {len(records)} snapshot files")
    return manifest


def restore_source(manifest):
    target = ROOT / 'out/gx-application-evaluation/source28'
    if target.exists():
        raise RuntimeError('Source destination already exists; refusing to overwrite it')
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / 'snapshots/source28', target)
    # A checkout may normalize executable permissions on non-executable files.
    for r in manifest['records']:
        (target / r['path']).chmod(r['mode'])
    print('Restored missing source28 directory without modifying the snapshot')


def restore_upstream():
    target = ROOT / 'upstream/.git'
    if target.exists():
        raise RuntimeError('Upstream metadata already exists; refusing to replace it')
    lock = json.loads((ROOT / 'source.lock.json').read_text())
    bundle = ROOT / 'backup/upstream-history.bundle'
    out = ROOT / 'out'
    out.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='restore-upstream-', dir=out) as d:
        clone = Path(d) / 'clone'
        subprocess.run(['git', 'clone', '--no-checkout', str(bundle), str(clone)], check=True)
        subprocess.run(['git', '-C', str(clone), 'reset', '--mixed', lock['commit']],
                       check=True, stdout=subprocess.DEVNULL)
        head = subprocess.check_output(['git', '-C', str(clone), 'rev-parse', 'HEAD'], text=True).strip()
        if head != lock['commit']:
            raise RuntimeError('Bundle does not restore the pinned commit')
        # Only Git metadata is moved. Existing upstream source bytes stay untouched.
        shutil.move(str(clone / '.git'), str(target))
    print('Restored pinned upstream Git metadata; source files were not overwritten')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--restore-source28', action='store_true')
    parser.add_argument('--restore-upstream-git', action='store_true')
    args = parser.parse_args()
    manifest = verify_snapshot()
    if args.restore_source28:
        restore_source(manifest)
    if args.restore_upstream_git:
        restore_upstream()


if __name__ == '__main__':
    main()
