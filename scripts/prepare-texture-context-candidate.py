#!/usr/bin/env python3
"""Separate 0.22.14 candidate: original renderer texture lifetime across contexts."""
import argparse,difflib,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def replace_once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected source shape: '+old[:90])
 return text.replace(old,new,1)
def prepare(base,out):
 base=base.resolve();out=out.resolve()
 if out.exists():raise FileExistsError(out)
 identity=json.loads((base/'app/AppScope/app.json5').read_text())['app']
 if (identity['versionName'],identity['versionCode'])!=('0.22.13',221300):raise ValueError('Requires frozen source13')
 lock=json.loads((base/'source.lock.json').read_text())
 if len(lock['patches'])!=19:raise ValueError('Requires exactly the frozen 19-patch candidate')
 # The candidate13 owner/window inputs must still match its immutable manifest.
 manifest=json.loads((base/'persistent-gx-inputs.json').read_text())
 for name,record in manifest['files'].items():
  path=base/name
  if sha(path)!=record['sha256'] or path.stat().st_size!=record['bytes']:raise ValueError('Frozen candidate input changed: '+name)
 for row in lock['patches']:
  if sha(base/row['file'])!=row['sha256']:raise ValueError('Frozen patch changed: '+row['file'])
 for row in lock['patched_files']:
  if sha(base/'upstream'/row['file'])!=row['sha256']:raise ValueError('Frozen upstream file changed: '+row['file'])
 shutil.copytree(base,out,ignore=lambda d,n:[x for x in n if x in ('reports','out','__pycache__','.hvigor','oh_modules')])
 path=out/'upstream/glib/gltexture.cpp';before=path.read_bytes();newline='\r\n' if b'\r\n' in before else '\n';text=before.decode().replace('\r\n','\n')
 text=replace_once(text,'    olx_gl::deleteTextures(1, (GLuint*)&texId);','''    // BeforeContextChange already retired detached GPU names. Destruction may
    // occur after the compatibility context has closed; never dispatch GL then.
    if (texId != ~GLuint(0)) {
      olx_gl::deleteTextures(1, &texId);
    }''')
 after=text.replace('\n',newline).encode();path.write_bytes(after)
 patch_name='0020-texture-manager-detached-context-destruction.patch'
 patch=''.join(difflib.unified_diff(before.decode().splitlines(keepends=True),after.decode().splitlines(keepends=True),fromfile='a/glib/gltexture.cpp',tofile='b/glib/gltexture.cpp',lineterm='\n'))
 patch_path=out/'patches'/patch_name;patch_path.write_bytes(patch.encode())
 lock['patches'].append(dict(file='patches/'+patch_name,sha256=sha(patch_path)))
 next(r for r in lock['patched_files'] if r['file']=='glib/gltexture.cpp')['sha256']=sha(path)
 lock['evaluation_scope']='original renderer-owned synthetic 1D textures retained across real window contexts; pending new candidate qualification; full port incomplete'
 (out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
 for name in ('persistent_texture_window_probe.h','persistent_texture_window_probe.cpp'):
  shutil.copy2(ROOT/'experiments/gx-texture-context/native'/name,out/'native'/name)
 path=out/'native/persistent_gx_window.cpp';text=path.read_text()
 text=replace_once(text,'#include "window_texture_1d_probe.h"','#include "window_texture_1d_probe.h"\n#include "persistent_texture_window_probe.h"')
 marker='    lease.Close();CheckGL("persistent renderer detach before compatibility shutdown");'
 text=replace_once(text,marker,'''    out<<",\\"persistent_textures\\":"<<RunPersistentTextureWindowProbe(h,genuine.GetTextureManager(),w,hh);
'''+marker+'''
    const bool texturesDetached=PersistentWindowTexturesDetached(genuine.GetTextureManager());
    if(!texturesDetached)throw std::runtime_error("Persistent textures remain resident before GL4ES close");
    out<<",\\"persistent_textures_detached_before_gl4es_close\\":true";''')
 path.write_text(text)
 with (out/'app/entry/src/main/cpp/CMakeLists.txt').open('a') as f:
  f.write('\n# Original renderer-owned resources, retained across actual window contexts.\ntarget_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/persistent_texture_window_probe.cpp")\n')
 path=out/'app/AppScope/app.json5';app=json.loads(path.read_text());app['app'].update(versionName='0.22.14',versionCode=221400);path.write_text(json.dumps(app,ensure_ascii=False,indent=2)+'\n')
 # Keep the prior frozen manifest as historical input, not a manifest for new files.
 files=['native/persistent_gx_window.cpp','native/persistent_texture_window_probe.cpp','native/persistent_texture_window_probe.h','upstream/glib/gltexture.cpp','source.lock.json','app/AppScope/app.json5','app/entry/src/main/cpp/CMakeLists.txt','patches/'+patch_name]
 result=dict(version='0.22.14',base='0.22.13',base_lock_sha256=sha(base/'source.lock.json'),files={n:dict(sha256=sha(out/n),bytes=(out/n).stat().st_size) for n in files},full_port_complete=False,functional_completion_percent=None)
 (out/'texture-context-inputs.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(source=str(out),version='0.22.14',patches=20,build_executed=False,actual_cross_context_gpu_tested=False,full_port_complete=False),indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();prepare(a.base,a.output)
