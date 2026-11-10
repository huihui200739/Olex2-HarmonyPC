#!/usr/bin/env python3
"""Prepare a new isolated candidate with fixed source-built exact PNG RGB decoding."""
import argparse,hashlib,json,shutil
from pathlib import Path
from vendor_sources import verify_vendor
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
base=a.base.resolve();out=a.output.resolve();evaluation=ROOT/'out/gx-application-evaluation';png=ROOT/'experiments/gx-png-image'
if not base.is_relative_to(evaluation) or not out.is_relative_to(evaluation) or out.exists():raise ValueError('Use a new isolated evaluation mirror')
lock=json.loads((base/'source.lock.json').read_text())
if len(lock['patches'])!=18:raise ValueError('Expected retained 18-patch packed-pixel candidate')
verify_vendor(base);verify_vendor(png)
shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('reports','out','__pycache__','.hvigor','oh_modules')])
for name in ('png_texture_decoder.h','png_texture_decoder.c','png_texture_image.h','png_texture_image.cpp','png_gx_texture_app.h'):
 shutil.copy2(png/'native'/name,out/'native'/name)
shutil.copy2(png/'native/gx_png_texture_lifecycle.cpp',out/'native/gx_texture_lifecycle.cpp')
# Retain the actual current provenance tools used by the outer build recipe.
for name in ('build_provenance_png.py','build_provenance.py','vendor_sources.py'):
 shutil.copy2(ROOT/'scripts'/name,out/'scripts'/name)
shutil.copy2(ROOT/'scripts/build_provenance_png.py',out/'scripts/build_provenance_graphics.py')
for name in ('Olex2PinnedPng.cmake','FindZLIB.cmake'):shutil.copy2(png/'cmake'/name,out/'cmake'/name)
vendor=json.loads((out/'vendor.lock.json').read_text());extra=json.loads((png/'vendor.lock.json').read_text())
for pkg in extra['packages']:shutil.copytree(png/pkg['directory'],out/pkg['directory'])
vendor['packages'].extend(extra['packages']);vendor['scope']+='; isolated exact PNG RGB candidate adds fixed unmodified libpng/zlib'
(out/'vendor.lock.json').write_text(json.dumps(vendor,ensure_ascii=False,indent=2)+'\n')
cmake=out/'app/entry/src/main/cpp/CMakeLists.txt';s=cmake.read_text();s+='''
# Candidate-only exact PNG sample decoder. ImageKit capability probes remain
# separate and unchanged; no test writer, synthetic mock GL or prebuilt decoder.
include("${OLEX2_ROOT}/cmake/Olex2PinnedPng.cmake")
add_olex2_pinned_png("${OLEX2_ROOT}")
target_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/png_texture_decoder.c" "${OLEX2_ROOT}/native/png_texture_image.cpp")
target_link_libraries(olex2core_graphics PRIVATE png_static)
''';cmake.write_text(s)
identity=out/'app/AppScope/app.json5';s=identity.read_text();assert '0.22.8' in s and '220800' in s;identity.write_text(s.replace('0.22.8','0.22.9').replace('220800','220900'))
lock['evaluation_scope']='isolated source-built candidate: exact unassociated PNG RGB virtual seam plus retained packed 2D pixel transfer; not original GPU texture upload, persistent runtime, UI replacement or full-port qualification'
(out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
raw=out/'app/entry/src/main/resources/rawfile'
for package,license in [('libpng','LICENSE'),('zlib','LICENSE')]:shutil.copy2(out/'vendor'/package/license,raw/(package+'-LICENSE.txt'))
notice=raw/'THIRD-PARTY-NOTICES.txt'
with notice.open('a') as f:
 f.write('\nISOLATED CANDIDATE PNG DECODER\n')
 for pkg in extra['packages']:f.write(pkg['name']+' '+pkg['tag']+' / '+pkg['repository']+' commit '+pkg['revision']+'; unmodified source; see '+pkg['name']+'-LICENSE.txt.\n')
print(json.dumps(dict(source=str(out),identity='0.22.9',patches=18,vendor=verify_vendor(out),production_unchanged=True,hap_built=False,original_texture_gpu_executed=False),ensure_ascii=False,indent=2))
