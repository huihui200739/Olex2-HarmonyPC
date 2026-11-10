#!/usr/bin/env python3
"""Inspect SDK linking compatibility; never create a graphics context."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', type=Path, default=Path(os.environ.get(
    'OHOS_NATIVE_HOME', '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')))
args = parser.parse_args()
sdk = args.sdk.resolve()
cmake = Path(os.environ.get('CMAKE_BIN', str(sdk / 'build-tools/cmake/bin/cmake')))
lib = sdk / 'sysroot/usr/lib/aarch64-linux-ohos/libGLv4.so'
raw = subprocess.check_output([str(sdk / 'llvm/bin/llvm-nm'), '-D', str(lib)], text=True)
exports = {line.split()[-1].split('@')[0] for line in raw.splitlines() if line.strip()}
legacy = ['glMatrixMode', 'glLoadIdentity', 'glNewList', 'glBegin',
          'glVertex3d', 'glEnd', 'glEndList']
core = ['glGetString', 'glClear', 'glCreateShader', 'glGenBuffers']
reports = ROOT / 'reports'
reports.mkdir(exist_ok=True)
(reports / 'sdk-glv4-exports.txt').write_text(raw)

probe_results = {}
for target, log_name in [('olex2_gl_core_probe', 'core-gl-link.log'),
                         ('olex2_gl_link_probe', 'legacy-gl-link.log')]:
    result = subprocess.run([str(cmake), '--build', str(ROOT / 'out/ohos-arm64'),
                             '--target', target], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (reports / log_name).write_text(result.stdout)
    probe_results[target] = result.returncode

report = {
    'scope': 'SDK arm64 link compatibility only; no device/rendering test',
    'sdk_native_version': json.loads((sdk / 'oh-uni-package.json').read_text())['version'],
    'sdk_api': json.loads((sdk / 'oh-uni-package.json').read_text())['apiVersion'],
    'architecture': 'arm64-v8a',
    'library': str(lib),
    'legacy_gl_required': legacy,
    'legacy_gl_missing': [name for name in legacy if name not in exports],
    'core_gl_exported': [name for name in core if name in exports],
    'glu_header_present': (sdk / 'sysroot/usr/include/GL/glu.h').exists(),
    'glu_libraries_present': sorted(p.name for p in lib.parent.glob('*GLU*')),
    'link_exit_codes': probe_results,
    'runtime_rendering_verified': False,
}
report['upstream_renderer_links_unmodified'] = probe_results['olex2_gl_link_probe'] == 0
(reports / 'graphics-compatibility.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
# Missing legacy APIs is an assessment finding, not a failure to run the check.
raise SystemExit(0 if probe_results['olex2_gl_core_probe'] == 0 else 1)
