#!/usr/bin/env python3
"""Bind genuine CPU metadata output and actual host build inputs, not GPU proof."""
import argparse,hashlib,json,shlex
from pathlib import Path
from persistent_gx_catalog_reference import parse_catalog_log,HOST_ASSERTIONS
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--log',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build,source,log=map(lambda p:p.resolve(),(a.build,a.source,a.log))
def bound(p):return dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
reference=parse_catalog_log(log.read_text());commands=json.loads((build/'compile_commands.json').read_text());files=[]
for command in commands:
 path=Path(command['file']).resolve()
 if not (path.is_relative_to(source) or path==ROOT/'experiments/gx-persistent-runtime/host/catalog_host.cpp'):raise ValueError('Host catalog compiled an unexpected source')
 files.append(dict(**bound(path),command=command['command']))
if len(files)!=382 or sum('/upstream/' in x['path'] and x['path'].endswith('.cpp') for x in files)!=379:raise ValueError('Genuine source compilation inventory changed')
ctest=ROOT/'out/gx-application-evaluation/persistent-gx-catalog-host-ctest.log'
if '100% tests passed, 0 tests failed out of 1' not in ctest.read_text():raise ValueError('Genuine host CTest did not pass')
result=dict(passed=True,scope='Genuine empty TGXApp registration and original metadata log bytes on host only; no loaded data, GL context or font execution',root_registry=dict(macros=193,functions=46),recursive_catalog=dict(macros=196,functions=52),assertions=HOST_ASSERTIONS,reference=reference,raw_log=bound(log),source=str(source.relative_to(ROOT)),compiled_sources=files,
 build_evidence=[bound(build/name) for name in ('compile_commands.json','build.ninja','.ninja_deps','genuine_gx_catalog_host','libgx_catalog_graphics.a','libgx_catalog_core.a','libgx_catalog_sdl.a')]+[bound(ctest),bound(ROOT/'experiments/gx-persistent-runtime/host/CMakeLists.txt')],full_port_complete=False,functional_completion_percent=None)
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(dict(passed=True,root_registry=result['root_registry'],recursive_catalog=result['recursive_catalog'],assertions=HOST_ASSERTIONS,actual_host_compiled_sources=382,output=str(a.output)),ensure_ascii=False,indent=2))
