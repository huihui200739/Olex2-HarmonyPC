#!/usr/bin/env python3
"""Freeze25: minimal platform-only original raster glyph flush, retaining24 actual failure."""
import argparse,difflib,hashlib,json,shutil
from pathlib import Path
from olex_font_resample_window_validation import validate_source
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(base,out):
 base,out=base.resolve(),out.resolve();validate_source(base)
 app=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if app['versionName']!='0.22.24' or app['versionCode']!=222400:raise ValueError('Requires frozen24')
 if out.exists():raise FileExistsError(out)
 shutil.copytree(base,out,ignore=lambda folder,names:[n for n in names if n in ('out','reports','__pycache__','.hvigor','oh_modules')])
 path='upstream/glib/glfont.cpp';old=(base/path).read_bytes();eol=b'\r\n' if b'\r\n' in old else b'\n'
 marker=eol.join([b'      olx_gl::callList(FontBase + ind);',b'    }',b'  }',b'}',b'//..............................................................................',b'void TGlFont::DrawVectorChar'])
 replacement=eol.join([b'      olx_gl::callList(FontBase + ind);',b'    }',b'  }',b'#ifdef OLEX2_PLATFORM_RASTER_GLYPH_FLUSH',b'  // Retire this bitmap batch before the next glyph can overlap its padding.',b'  // This preserves transparent zero bits with the fixed GL4ES raster backend.',b'  olx_gl::flush();',b'#endif',b'}',b'//..............................................................................',b'void TGlFont::DrawVectorChar'])
 if old.count(marker)!=1:raise ValueError('Changed exact original raster glyph function')
 new=old.replace(marker,replacement,1);(out/path).write_bytes(new)
 patch='patches/0021-flush-platform-font-raster-glyph-batches.patch'
 diff=''.join(difflib.unified_diff(old.decode().splitlines(keepends=True),new.decode().splitlines(keepends=True),fromfile='a/glib/glfont.cpp',tofile='b/glib/glfont.cpp'))
 (out/patch).write_bytes(diff.encode());lock=json.loads((out/'source.lock.json').read_text());lock['patches'].append(dict(file=patch,sha256=sha(out/patch)))
 if any(row['file']=='glib/glfont.cpp' for row in lock['patched_files']):raise ValueError('Unexpected previously patched original font source')
 lock['patched_files'].append(dict(file='glib/glfont.cpp',sha256=sha(out/path)));lock['patched_files'].sort(key=lambda row:row['file'])
 lock['evaluation_scope']='Original resource fonts with source-model high-quality resampling and platform-only per-raster-glyph bitmap flush; candidate25 qualification pending; full port incomplete'
 (out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
 # Keep the20 format/version/table unchanged, but explicitly bind its new
 # upstream wrapper source through this new25 overlay and source patch.
 vector=json.loads((out/'original-vector-font-inputs.json').read_text());vector['original_vector_source']['sha256']=sha(out/path);(out/'original-vector-font-inputs.json').write_text(json.dumps(vector,ensure_ascii=False,indent=2)+'\n')
 cmake=out/'app/entry/src/main/cpp/CMakeLists.txt';cmake.write_text(cmake.read_text()+'\n# Fixed GL4ES bitmap batches must not erase ink under adjacent glyph padding.\ntarget_compile_definitions(olex2core_graphics PRIVATE OLEX2_PLATFORM_RASTER_GLYPH_FLUSH=1)\n')
 app.update(versionName='0.22.25',versionCode=222500);(out/'app/AppScope/app.json5').write_text(json.dumps(dict(app=app),ensure_ascii=False,indent=2)+'\n')
 names=[path,patch,'source.lock.json','original-vector-font-inputs.json','app/entry/src/main/cpp/CMakeLists.txt','app/AppScope/app.json5']
 result=dict(version='0.22.25',base='0.22.24',base_manifest_sha256=sha(base/'olex-font-resample-inputs.json'),scope='Platform-only original raster-glyph batch flush; preserves glyphs/metrics/positions and unmodified vendor;24 raw four-pixel failure retained',
  files={name:dict(bytes=(out/name).stat().st_size,sha256=sha(out/name)) for name in names},original_font_source_before_sha256=hashlib.sha256(old).hexdigest(),original_font_source_after_sha256=hashlib.sha256(new).hexdigest(),patches=21,full_port_complete=False,functional_completion_percent=None)
 (out/'font-raster-flush-inputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=prepare(a.base,a.output);print(json.dumps(dict(version=d['version'],files=len(d['files']),output=str(a.output.resolve()),target_executed=False),indent=2))
