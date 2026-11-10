#!/usr/bin/env python3
"""Audit explicit old successful diagnostics; never delete anything."""
import argparse
import json
from pathlib import Path
import subprocess

from diagnostic_backup_audit import (audit_local_backups, readonly_device_query,
                                    audit_device_inventory, create_owned_directory)

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, action='append', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--observe-device', action='store_true')
    parser.add_argument('--target')
    parser.add_argument('--hdc', default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
    args = parser.parse_args()
    if args.observe_device and args.target != '127.0.0.1:5555':
        parser.error('Read-only observation requires explicit --target 127.0.0.1:5555')
    output = create_owned_directory(ROOT, args.output)
    result = dict(passed=False, scope='Read-only storage recovery preparation',
                  device_observed=False, deletion_authorized=False, deleted_files=0,
                  files_modified=False, full_port_complete=False, functional_completion_percent=None)
    try:
        audit = audit_local_backups(ROOT, args.report)
        (output / 'local-backup-audit.json').write_text(json.dumps(audit, indent=2)+'\n')
        query = readonly_device_query(audit)
        (output / 'readonly-device-query.sh').write_text(query)
        result.update(local_directory_count=audit['directory_count'], local_file_count=audit['file_count'],
                      local_payload_bytes=audit['payload_bytes'])
        if args.observe_device:
            command = [args.hdc, '-t', args.target, 'shell', query]
            (output / 'device-command.json').write_text(json.dumps(command, indent=2)+'\n')
            observed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (output / 'device-inventory.log').write_bytes(observed.stdout)
            if observed.returncode:
                raise RuntimeError(f'Read-only HDC inventory exited {observed.returncode}')
            verified = audit_device_inventory(audit, observed.stdout.decode('utf-8', errors='strict'))
            (output / 'device-backup-audit.json').write_text(json.dumps(verified, indent=2)+'\n')
            result.update(device_observed=True, device_verification=verified)
        result['passed'] = True
    except Exception as error:
        result['error'] = f'{type(error).__name__}: {error}'
    finally:
        for name in ('audit-diagnostic-backups.py', 'diagnostic_backup_audit.py'):
            (output / name).write_bytes((ROOT / 'scripts' / name).read_bytes())
        (output / 'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
