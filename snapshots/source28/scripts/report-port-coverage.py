#!/usr/bin/env python3
"""Count tracked upstream C++ translation units included in the local OHOS build.

This is build inclusion only, never feature completion or full-port readiness.
"""
from datetime import datetime
import json
from pathlib import Path
import subprocess
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'upstream'
tracked = subprocess.check_output(['git', '-C', str(UPSTREAM), 'ls-files', '-z', '*.cpp'])
files = {name.decode() for name in tracked.split(b'\0') if name}
# The production application now compiles additional original graphics sources
# in its unique runtime DSO. Count the source-verified package, not a different
# command-line target's smaller compilation database.
from build_provenance import verify
proof_path=ROOT/'out/app/entry-default-unsigned.provenance.json'
verify(ROOT/'out/app/entry-default-unsigned.hap',proof_path,ROOT)
provenance=json.loads(proof_path.read_text())
selected={item['path'].removeprefix('upstream/') for item in provenance['native']['compiled_sources']
          if item['path'].startswith('upstream/') and item['path'].removeprefix('upstream/') in files}
if not files or not selected:
    raise SystemExit('Missing tracked sources or configured OHOS compilation entries')
report = {
    'checked_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),
    'metric': 'upstream_cpp_translation_unit_build_inclusion_only',
    'upstream_tracked_cpp_count': len(files), 'selected_upstream_cpp_count': len(selected),
    'selected_upstream_cpp_percent': round(len(selected) * 100 / len(files), 2),
    'functional_completion_percent': None,
    'functional_completion_measured': False,
    'denominator': 'All tracked *.cpp files at the pinned upstream checkout, including auxiliary/test programs',
    'numerator': 'Distinct tracked upstream *.cpp in the source-verified native libraries of the actual diagnostic HAP',
    'limits': [
        'Files are not weighted by effort or importance; inclusion is not full behavior coverage',
        'Extracted helper headers and new platform code are not upstream cpp translation units',
        'Does not measure complete GUI, renderer, Python, plugins, workflows or release readiness',
        'Not test pass percentage, code line coverage or a schedule estimate',
    ],
    'selected_sources': sorted(selected),
    'evidence': 'out/app/entry-default-unsigned.provenance.json',
    'upstream_commit': subprocess.check_output(['git', '-C', str(UPSTREAM), 'rev-parse', 'HEAD'], text=True).strip(),
    'full_port_complete': False, 'physical_pc_verified': False,
}
(ROOT / 'reports/port-coverage.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(f"Upstream C++ build inclusion: {len(selected)}/{len(files)} = {report['selected_upstream_cpp_percent']}%")
print('Complete functional progress is unmeasured; do not present this as full-port completion.')
