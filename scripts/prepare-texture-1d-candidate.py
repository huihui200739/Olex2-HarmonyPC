#!/usr/bin/env python3
"""Create an isolated 1D upload/target-aware resource-recreation candidate.

Only normal synthetic graphics are used; production and earlier candidates stay
unchanged. No prebuilt library, GL mock or test writer enters the HAP.
"""
import argparse,difflib,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def prepare(base,out,version):
 evaluation=ROOT/'out/gx-application-evaluation'
 base=base.resolve();out=out.resolve()
 if not base.is_relative_to(evaluation) or not out.is_relative_to(evaluation) or out.exists():
  raise ValueError('Select an existing evaluation base and a new evaluation output')
 lock=json.loads((base/'source.lock.json').read_text())
 if len(lock['patches'])!=18 or json.loads((base/'app/AppScope/app.json5').read_text())['app']['versionName']!='0.22.9':
  raise ValueError('Expected the frozen 18-patch PNG candidate 0.22.9')
 shutil.copytree(base,out,ignore=lambda directory,names:[n for n in names if n in ('reports','out','__pycache__','.hvigor','oh_modules')])
 path=out/'upstream/glib/gltexture.cpp';before=path.read_bytes();newline='\r\n' if b'\r\n' in before else '\n';s=before.decode().replace('\r\n','\n')
 start=s.index('GLuint TTextureManager::Add1DTexture(');stop=s.index('//.............................................................................',start)
 s=s[:start]+'''GLuint TTextureManager::Add1DTexture(const olxstr& name, GLint level,
  GLsizei width, GLint border, GLenum format, const GLvoid *pixels)
{
  if (format != GL_RGB && format != GL_RGBA) {
    throw TInvalidArgumentException(__OlxSourceInfo,
      olxstr("format=") << (int)format);
  }
  GLuint texId = 0;
  olx_gl::genTextures(1, &texId);
  olx_gl::bindTexture(GL_TEXTURE_1D, texId);
  olex2_harmony::ScopedPackedPixels<olx_gl> unpack(
    GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS);
  olx_gl::texImage(GL_TEXTURE_1D, level, format == GL_RGB ? 3 : 4,
    width, border, format, GL_UNSIGNED_BYTE, pixels);
  TGlTexture* tex = new TGlTexture(texId, tpt1D, level, name);
  Textures.Add(texId, tex);
  return texId;
}
'''+s[stop:]
 assert s.count('  if (data.height != 0) {')==1
 s=s.replace('  if (data.height != 0) {','  // A 1D readback reports height one, but still requires the 1D overload.\n  if (tex.GetType() == GL_TEXTURE_2D) {',1)
 marker='  for (size_t i=0; i < Textures.Count(); i++) {\n    TGlTexture &glt = *Textures.GetValue(i);'
 start=s.index('void TTextureManager::AfterContextChange()')
 tail=s[start:];assert tail.count(marker)==1
 tail=tail.replace(marker,'  sorted::PrimitiveAssociation<GLuint, TGlTexture*> recreated;\n'+marker,1)
 tail=tail.replace('      glt.WriteData(TextureData[i]);\n    }\n  }','      glt.WriteData(TextureData[i]);\n    }\n    recreated.Add(glt.GetId(), &glt);\n  }\n  // Manager lookups must follow the regenerated object IDs.\n  Textures = recreated;',1)
 s=s[:start]+tail
 # The pinned, unmodified GL4ES GetTexImage returns without reading 1D.
 # Its public 2D binding can alias the same logical texture. Preserve that
 # binding during a narrowly enabled backend adapter, not a vendor change.
 helper='''
#ifdef OLEX2_HARMONY_GL4ES_TEXTURE_1D_READBACK
namespace {
class ScopedGl4esReadAlias {
  GLint previous_ = 0;
public:
  explicit ScopedGl4esReadAlias(GLuint id) {
    olx_gl::get(GL_TEXTURE_BINDING_2D, &previous_);
    olx_gl::bindTexture(GL_TEXTURE_2D, id);
  }
  ~ScopedGl4esReadAlias() {
    olx_gl::bindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_));
  }
};
}
#endif
'''
 s=s.replace('#include "glpixelstorescope.h"','#include "glpixelstorescope.h"\n'+helper,1)
 old='''  olx_gl::getTexImage(tex.GetType(), tex.GetLevel(), GL_RGBA,
    GL_UNSIGNED_BYTE, rv.data);'''
 new='''#ifdef OLEX2_HARMONY_GL4ES_TEXTURE_1D_READBACK
  if (tex.GetType() == GL_TEXTURE_1D) {
    ScopedGl4esReadAlias alias(tex.GetId());
    olx_gl::getTexImage(GL_TEXTURE_2D, tex.GetLevel(), GL_RGBA,
      GL_UNSIGNED_BYTE, rv.data);
  }
  else
#endif
  {
    olx_gl::getTexImage(tex.GetType(), tex.GetLevel(), GL_RGBA,
      GL_UNSIGNED_BYTE, rv.data);
  }'''
 assert s.count(old)==1;s=s.replace(old,new,1)
 after=s.replace('\n',newline).encode();path.write_bytes(after)
 patch=''.join(difflib.unified_diff(before.decode().splitlines(keepends=True),after.decode().splitlines(keepends=True),fromfile='a/glib/gltexture.cpp',tofile='b/glib/gltexture.cpp',lineterm='\n'))
 name='0019-texture-1d-upload-and-target-aware-recreation.patch';p=out/'patches'/name;p.write_bytes(patch.encode())
 shutil.copy2(p,ROOT/'experiments/gx-application/patches'/name)
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 lock['patches'].append(dict(file='patches/'+name,sha256=sha(p)))
 next(row for row in lock['patched_files'] if row['file']=='glib/gltexture.cpp')['sha256']=sha(path)
 lock['evaluation_scope']='isolated candidate: original 1D texture upload, target-aware data restoration and regenerated-ID lookups; actual window qualification pending; full port incomplete'
 (out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
 app=out/'app/AppScope/app.json5';identity=json.loads(app.read_text());identity['app'].update(versionCode=220000+int(version.split('.')[-1])*100,versionName=version);app.write_text(json.dumps(identity,ensure_ascii=False,indent=2)+'\n')
 # Each new mirror retains its own exact PNG provenance engine and probe.
 for name in ('build_provenance_png.py','build_provenance.py','vendor_sources.py'):
  shutil.copy2(ROOT/'scripts'/name,out/'scripts'/name)
 shutil.copy2(ROOT/'scripts/build_provenance_png.py',out/'scripts/build_provenance_graphics.py')
 for name in ('window_texture_1d_probe.h','window_texture_1d_probe.cpp'):
  shutil.copy2(ROOT/'experiments/gx-texture-1d/native'/name,out/'native'/name)
 path=out/'native/original_renderer_bridge.cpp';s=path.read_text()
 s=s.replace('#include "original_renderer_host.h"','#include "original_renderer_host.h"\n#include "window_texture_1d_probe.h"',1)
 old='    out<<"],\\"execution_completed\\":true,\\"pixel_validation\\":\\"pending independent host analysis\\",\\"error\\":\\"\\"}";'
 new='    out<<"],\\"texture_1d\\":"<<RunOriginalTexture1DWindowProbe(h,w,hh)<<",\\"execution_completed\\":true,\\"pixel_validation\\":\\"pending independent host analysis\\",\\"error\\":\\"\\"}";'
 assert old in s;s=s.replace(old,new,1);path.write_text(s)
 with (out/'app/entry/src/main/cpp/CMakeLists.txt').open('a') as f:
  f.write('\n# Actual-window synthetic 1D texture sampling, no test-owned GL mocks.\ntarget_sources(olex2core_graphics PRIVATE "${OLEX2_ROOT}/native/window_texture_1d_probe.cpp")\ntarget_compile_definitions(olex2core_graphics PRIVATE OLEX2_HARMONY_GL4ES_TEXTURE_1D_READBACK=1)\n')
 # Keep all 126 original per-round checks. Exactly three pinned FPE pairs
 # now cover untextured, RGB and RGBA variants; no arbitrary count allowance.
 path=out/'scripts/original_renderer_validation.py';s=path.read_text()
 assert s.count('len(adapted)==2')==1;s=s.replace('len(adapted)==2','len(adapted)==6');path.write_text(s)
 shutil.copy2(ROOT/'scripts/texture_1d_window_validation.py',out/'scripts/texture_1d_window_validation.py')
 path=out/'scripts/run-app-smoke.py';s=path.read_text()
 marker="    record('shared_original_'+label,validation['passed'],validation)"
 assert marker in s
 s=s.replace(marker,marker+"\n    invoke('texture_1d_window_validation.py',folder)\n    texture_validation=json.loads((folder/'independent-texture-1d-validation.json').read_text())\n    record('shared_texture_1d_'+label,texture_validation['passed'],texture_validation)")
 path.write_text(s)
 # Search-header visibility is not proof that text can already be typed.
 shutil.copy2(ROOT/'scripts/picker_readiness.py',out/'scripts/picker_readiness.py')
 path=out/'scripts/run-app-smoke.py';s=path.read_text()
 s=s.replace('import argparse\n','import argparse\nfrom picker_readiness import wait_for_source_or_focused_search\n',1)
 begin=s.index('    ready_deadline=time.monotonic()+20;ready_attempt=0');end=s.index('    if not selected_source_visible(nodes):',begin)
 s=s[:begin]+"    nodes,readiness=wait_for_source_or_focused_search(nodes,uri,\n        lambda attempt:layout('layout-options-import-ready-'+str(attempt),bundle=None),\n        lambda current:click(current,id='FileManager_SearchBarComp_Input'))\n    summary['options_document_picker_readiness']=readiness\n"+s[end:]
 path.write_text(s)
 # This is a shared host catalog reference, not copied emulator evidence.
 reference=ROOT/'reports/app/original-log-verified-host-original_log-tests.log'
 if reference.is_file():
  directory=out/'reports/app';directory.mkdir(parents=True,exist_ok=True)
  shutil.copy2(reference,directory/reference.name)
 print(json.dumps(dict(source=str(out),patches=19,patch_sha256=sha(p),production_unchanged=True,real_gpu_executed=False),indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--version',choices=('0.22.11',),default='0.22.11');a=p.parse_args();prepare(a.base,a.output,a.version)
