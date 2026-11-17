#!/usr/bin/env python3
"""Add explicit ABI names from an OHOS probe and the built CPython ELF.

No guessed C++ names, wildcards, replacement GL exports or precompiled package inputs.
The source candidate and evidence output directory must not be frozen build inputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from build_provenance_png import elf_symbols_and_needed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--python-library', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sdk = Path('/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')
    probe = output / 'application-python-abi.cpp'
    probe.write_text('#include "application_runtime.h"\nvoid probe(olex2::harmony::ApplicationRuntime* app,const std::string& text) { app->InitializePython(text); app->GetPythonStatus(); app->RunPython(text); }\n')
    obj = output / 'application-python-abi.o'
    command = [str(sdk/'llvm/bin/clang++'), '--target=aarch64-linux-ohos',
               '--gcc-toolchain='+str(sdk/'llvm'), '--sysroot='+str(sdk/'sysroot'),
               '-std=c++17', '-D_UNICODE', '-D_PYTHON']
    for folder in ('native', 'vendor/cpython/Include', 'native/python-build', 'upstream/sdl',
                   'upstream/repository', 'upstream/xlib', 'upstream/xlib/absorpc', 'upstream/glib'):
        command.append('-I'+str(source/folder))
    command += ['-c', str(probe), '-o', str(obj)]
    subprocess.run(command, check=True)
    undefined = subprocess.check_output([str(sdk/'llvm/bin/llvm-nm'), '--undefined-only', '--format=posix', str(obj)], text=True)
    (output/'undefined-symbols.txt').write_text(undefined)
    methods = sorted(line.split()[0] for line in undefined.splitlines() if 'ApplicationRuntime' in line and any(name in line for name in ('InitializePython', 'GetPythonStatus', 'RunPython')))
    if len(methods) != 3 or len(set(methods)) != 3:
        raise ValueError('Probe did not produce exactly the three actual application ABI symbols')
    library = args.python_library.resolve(strict=True)
    symbols, needed = elf_symbols_and_needed(library.read_bytes())
    python = sorted(name for name in symbols if name.startswith(('Py', '_Py')))
    if len(python) != 1710 or any(not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', name) for name in python):
        raise ValueError('Unexpected fixed CPython 3.11.18 C API export inventory')
    script = source/'cmake/original_renderer.exports'
    original = script.read_text()
    existing = {line.strip()[:-1] for line in original.splitlines() if re.fullmatch(r'\s*[A-Za-z_][A-Za-z_0-9]*;\s*', line)}
    additions = sorted((set(methods)|set(python))-existing)
    if ' local: *;' not in original or len(additions) != 1713:
        raise ValueError('Unexpected original ABI list or candidate has already been changed')
    script.write_text(original.replace(' local: *;', ''.join('  '+name+';\n' for name in additions)+' local: *;', 1))
    result = {'python_library': str(library), 'python_library_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
              'python_c_api_exports': python, 'application_abi_exports': methods,
              'source_probe_command': command, 'shared_library_needed_for_reference_only': needed,
              'explicit_additions': len(additions), 'precompiled_python_library_packaged': False,
              'native_plugin_abi_verified': False, 'full_port_complete': False}
    (output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    (source/'native/python-build/explicit-export-inputs.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Added 1710 explicit CPython C API symbols and 3 measured application ABI symbols')


if __name__ == '__main__':
    main()
