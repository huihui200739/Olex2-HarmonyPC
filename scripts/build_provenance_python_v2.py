#!/usr/bin/env python3
"""Source-built CPython application candidate, preserving all PNG provenance gates.

The only ABI extension is the explicit CPython inventory and measured application
methods. This is not validation of native wheels, plugins or complete application use.
"""
import json
from pathlib import Path
import re
import tarfile
import build_provenance_png as base

base.RECIPES += (
    'scripts/build_provenance_python.py', 'scripts/build-app-candidate-python.sh',
    'scripts/build_provenance_python_v2.py', 'scripts/build-app-candidate-python-v2.sh',
    'scripts/generate-python-runtime32-exports.py',
    'scripts/build-python-ohos-v6.sh', 'scripts/package-python-runtime-v1.py',
    'scripts/prepare-python-runtime32-v2.py', 'scripts/prepare-python-runtime32-v3.py',
)
base.LIMITS += [
    'CPython is rebuilt from captured C sources in the original core DSO; no prebuilt libpython is packaged.',
    'Explicit Python C API exports do not certify OHOS native plugins or GNU/Linux wheels.',
    'Application Python scripts are trusted, synchronous and not sandboxed; full plugins/GUI/document integration remain incomplete.',
]


def validate_graph(libraries, exports_text):
    require = base.require
    require({i['path'] for i in libraries} == base.LIBRARIES and len(libraries) == len(base.LIBRARIES), 'Unexpected native library graph')
    core = next(i for i in libraries if i['path'].endswith('libolex2core_graphics.so'))
    parent = next(i for i in libraries if i['path'].endswith('libolex2native.so'))
    runtime = next(i for i in libraries if i['path'].endswith('libc++_shared.so'))
    require(core['origin'] == parent['origin'] == 'compiled' and runtime['origin'] == 'native-sdk', 'Invalid native origin')
    allowed = sorted(line.strip()[:-1] for line in exports_text.splitlines()
                     if re.fullmatch(r'\s*(?:_Z|olex2_|Py|_Py)[A-Za-z_0-9]*;\s*', line))
    require(bool(allowed) and len(allowed) == len(set(allowed)) and core['exports'] == allowed
            and all(not name.startswith(('gl', 'egl')) for name in core['exports']), 'Graphics/Python exports differ from explicit host ABI')
    require(len([name for name in allowed if name.startswith(('Py', '_Py'))]) == 1710, 'Fixed CPython C API inventory differs')
    require('libolex2core_graphics.so' in parent['needed'] and 'libolex2native.so' not in core['needed'], 'Invalid single-core DSO dependency graph')
    require(not any(name.startswith('libpython') for library in libraries if library['origin'] == 'compiled' for name in library['needed']), 'Precompiled Python DSO dependency is not permitted')


_original_verify = base.verify


def verify(hap, provenance, workspace=None):
    result = _original_verify(hap, provenance, workspace)
    sidecar = json.loads(provenance.read_text())
    snapshot = provenance.parent/'build-evidence/source-snapshot.tar.gz'
    with tarfile.open(snapshot, 'r:gz') as archive:
        source_map = json.load(archive.extractfile('native/python-build/source-build-map.json'))
        exports = json.load(archive.extractfile('native/python-build/explicit-export-inputs.json'))
    expected = {item['source']: item['source_sha256'] for item in source_map['compiled_sources']}
    compiled = {item['path']: item for item in sidecar['native']['compiled_sources']}
    base.require(len(expected) == len(source_map['compiled_sources']) == 221, 'Fixed Python source map differs')
    for path, sha in expected.items():
        base.require(path in compiled and compiled[path]['sha256'] == sha, 'Python C source missing or changed in actual build: '+path)
    base.require({p for p in compiled if p.startswith('vendor/cpython/')} == {p for p in expected if p.startswith('vendor/cpython/')}, 'Python compilation inventory differs from fixed source map')
    for path in ('upstream/repository/pyext.cpp', 'native/python_runtime.cpp'):
        base.require(path in compiled, 'Original application bridge not compiled: '+path)
    core = next(item for item in sidecar['native_libraries'] if item['path'].endswith('libolex2core_graphics.so'))
    actual_python = [name for name in core['exports'] if name.startswith(('Py', '_Py'))]
    base.require(actual_python == exports['python_c_api_exports'], 'Packaged Python C API differs from measured source-build reference')
    base.require(all(name in core['exports'] for name in exports['application_abi_exports']), 'Measured application Python ABI not exported')
    result.update(python_compiled_sources=221, original_python_bridge_compiled=True,
                  python_c_api_explicit_exports=len(actual_python), application_python_execution_verified=False)
    return result


base.validate_graph = validate_graph
base.verify = verify

if __name__ == '__main__':
    raise SystemExit(base.main())
