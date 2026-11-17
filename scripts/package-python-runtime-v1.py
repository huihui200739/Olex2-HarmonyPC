#!/usr/bin/env python3
"""Package the actual OHOS Python build; no app/plugin completion claim."""
import argparse, hashlib, json, os
from pathlib import Path
import shutil, subprocess, zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'out/python-runtime-evaluation'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def record(path, label): return dict(path=label, bytes=path.stat().st_size, sha256=sha(path))

def package(build):
    build = build.resolve()
    result = json.loads((build / 'build-result.json').read_text())
    assert result['built'] and result['source_unmodified_after_build']
    info = json.loads((build / 'source-and-toolchain.json').read_text())
    for item in result['artifacts']:
        assert record(build / item['path'], item['path']) == item
    out = BASE / 'runtime-bundles' / build.name
    out.mkdir(parents=True, exist_ok=False)
    shutil.copy2(__file__, out / 'package-recipe.py')
    # This generates target sysconfig data with the matching host interpreter;
    # it does not execute an ARM64 binary on macOS or rebuild the interpreter.
    env = os.environ.copy()
    env['OLEX2_PYTHON_NATIVE_SDK'] = '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native'
    env['OLEX2_PYTHON_COMMAND_LOG'] = str(out / 'packaging-compiler-commands.jsonl')
    with (out / 'sysconfig-generation.log').open('wb') as log:
        subprocess.run(['make', 'pybuilddir.txt'], cwd=build/'build', env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    lib = build / 'source/Python-3.11.18/Lib'
    sysconfig_directory = build/'build'/(build/'build/pybuilddir.txt').read_text().strip()
    sysconfig_files = sorted(sysconfig_directory.glob('_sysconfigdata_*.py'))
    assert len(sysconfig_files) == 1, sysconfig_files
    runtime = out/'runtime'
    (runtime/'bin').mkdir(parents=True)
    (runtime/'lib/python3.11').mkdir(parents=True)
    shutil.copy2(build/'build/python.ohos', runtime/'bin/python3.11')
    shutil.copy2(build/'build/libpython3.11.so.1.0', runtime/'lib/libpython3.11.so.1.0')
    shutil.copy2(build/'source/Python-3.11.18/LICENSE', runtime/'CPYTHON-LICENSE.txt')
    archive = runtime/'lib/python311.zip'
    members = []
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        files = [(p, p.relative_to(lib).as_posix(), 'official-cpython-Lib') for p in sorted(lib.rglob('*'))
                 if p.is_file() and not p.is_symlink() and not {'test','tests','__pycache__','ensurepip'} & set(p.relative_to(lib).parts)
                 and p.suffix not in ('.pyc','.pyo')]
        files += [(p, p.name, 'generated-target-sysconfig') for p in sysconfig_files]
        names = [name for _,name,_ in files]
        assert len(names) == len(set(names))
        for path,name,origin in sorted(files, key=lambda item:item[1]):
            zi = zipfile.ZipInfo(name, date_time=(2000,1,1,0,0,0))
            zi.create_system = 3
            zi.external_attr = 0o100644 << 16
            zi.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(zi,path.read_bytes())
            members.append(dict(**record(path,name),origin=origin))
    config = (build/'build/Modules/config.c').read_text()
    for name in ['math','_json','_struct','zlib','_decimal','pyexpat','unicodedata','_codecs_cn','_socket']:
        assert 'PyInit_'+name in config, name
    output = dict(schema_version=1, target='aarch64-linux-ohos', version='3.11.18',
                  source_archive_sha256=info['archive_sha256'], build=str(build.relative_to(ROOT)),
                  generated_sysconfig=record(sysconfig_files[0], str(sysconfig_files[0].relative_to(build))),
                  stdlib=record(archive,'runtime/lib/python311.zip'), stdlib_members=members,
                  files=[record(p,str(p.relative_to(out))) for p in sorted(runtime.rglob('*')) if p.is_file()],
                  excluded_library_directories=['test','tests','__pycache__','ensurepip'],
                  known_limits=['Not integrated into the Olex2 HAP yet.',
                                'SSL, libffi/ctypes, sqlite, bz2/lzma and GUI extension dependencies are not supplied by this build.',
                                'Legacy CPython3.11 GNU-style extension tag does not imply GNU/Linux wheel or native plugin ABI compatibility.',
                                'SDK gettext and CAN header combinations are unavailable; normal TCP/IP socket code remains included.',
                                'An isolated interpreter configuration is not a security sandbox.'],
                  target_runtime_executed=False, application_integration_complete=False, full_port_complete=False)
    (out/'package-result.json').write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(output=str(out),files=len(output['files']),stdlib_files=len(members),stdlib_bytes=archive.stat().st_size),indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True)
    package(parser.parse_args().build)
