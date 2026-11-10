#!/usr/bin/env python3
"""Prepare an isolated, source-built texture-transfer remediation candidate.

Production upstream, its 16-patch lock, vendor and standard HAP are untouched.
This does not exercise the old misaligned upload or certify a real GPU.
"""
import argparse,difflib,hashlib,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
base=a.base.resolve();out=a.output.resolve();evaluation=ROOT/'out/gx-application-evaluation'
if not base.is_relative_to(evaluation) or not out.is_relative_to(evaluation) or out.exists():raise ValueError('Use a new isolated evaluation mirror in this project')
lock=json.loads((base/'source.lock.json').read_text())
if len(lock['patches'])!=17:raise ValueError('Expected the preserved 17-patch graphics-scene candidate')
shutil.copytree(base,out,ignore=lambda directory,names: [n for n in names if n in ('reports','out','__pycache__','.hvigor','oh_modules')])
path=out/'upstream/glib/gltexture.cpp';before=path.read_bytes();newline='\r\n' if b'\r\n' in before else '\n';text=before.decode().replace('\r\n','\n')
text=text.replace('#include "gltexture.h"','#include "gltexture.h"\n#include "glpixelstorescope.h"',1)
# Existing pixel buffers are tightly packed. Restore only transfer alignment;
# binding semantics remain the original manager's contract, not a new claim.
text=text.replace('  olx_gl::pixelStore(GL_PACK_ALIGNMENT, 4);','  olex2_harmony::ScopedPackedPixels<olx_gl> pack(\n    GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS);',1)
text=text.replace('  olx_gl::pixelStore(GL_UNPACK_ALIGNMENT, 4);','  olex2_harmony::ScopedPackedPixels<olx_gl> unpack(\n    GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS);',3)
# Only the 2D methods are remediated here. Add1DTexture remains a separate,
# explicitly unsupported route and must not be tested as a working 2D backend.
for name,stop in [('Add2DTexture','Replace2DTexture'),('Replace2DTexture','Add1DTexture')]:
 start=text.index(('GLuint' if name=='Add2DTexture' else 'void')+' TTextureManager::'+name)
 end=text.index(('void' if stop=='Replace2DTexture' else 'GLuint')+' TTextureManager::'+stop,start)
 body=text[start:end];old='''  else {
    throw TInvalidArgumentException(__OlxSourceInfo,
      olxstr("format=") << (int)format);
  }
''';assert body.count(old)==1;body=body.replace(old,'')
 opening=body.index('{\n')+2
 validation='''  if (format != GL_RGB && format != GL_RGBA) {
    throw TInvalidArgumentException(__OlxSourceInfo,
      olxstr("format=") << (int)format);
  }
'''
 body=body[:opening]+validation+body[opening:]
 if name=='Replace2DTexture':
  assert body.count('  olx_gl::deleteTextures(1, &texId);\n')==1
  body=body.replace('  olx_gl::deleteTextures(1, &texId);\n','  // Update the existing object; retain its name, ID and sampler state.\n')
 text=text[:start]+body+text[end:]
after=text.replace('\n',newline).encode();path.write_bytes(after)
header='''#ifndef OLEX2_HARMONY_GL_PIXEL_STORE_SCOPE_H
#define OLEX2_HARMONY_GL_PIXEL_STORE_SCOPE_H

// Packed 2D unsigned-byte transfer state only. No claim about binding, active
// unit, 3D image layout, context lifetime or all other GL transfer parameters.
namespace olex2_harmony {
template<class GLApi> class ScopedPixelStore {
  unsigned int parameter_;
  int previous_=0;
public:
  ScopedPixelStore(unsigned int parameter,int desired):parameter_(parameter) {
    GLApi::get(parameter_,&previous_);
    GLApi::pixelStore(parameter_,desired);
  }
  ~ScopedPixelStore() noexcept {GLApi::pixelStore(parameter_,previous_);}
  ScopedPixelStore(const ScopedPixelStore&)=delete;
  ScopedPixelStore& operator=(const ScopedPixelStore&)=delete;
};
// Canonical packed 2D layout. Clear row length/skips while transferring and
// restore every caller value in reverse order, including on C++ scope exit.
template<class GLApi> class ScopedPackedPixels {
  ScopedPixelStore<GLApi> alignment_, rowLength_, skipRows_, skipPixels_;
public:
  ScopedPackedPixels(unsigned int alignment,unsigned int rowLength,
                    unsigned int skipRows,unsigned int skipPixels)
    :alignment_(alignment,1),rowLength_(rowLength,0),
     skipRows_(skipRows,0),skipPixels_(skipPixels,0) {}
  ScopedPackedPixels(const ScopedPackedPixels&)=delete;
  ScopedPackedPixels& operator=(const ScopedPackedPixels&)=delete;
};
}
#endif
''' 
newfile=out/'upstream/glib/glpixelstorescope.h';newfile.write_text(header)
patch=''.join(difflib.unified_diff(before.decode().splitlines(keepends=True),after.decode().splitlines(keepends=True),fromfile='a/glib/gltexture.cpp',tofile='b/glib/gltexture.cpp',lineterm='\n'))
patch+=''.join(difflib.unified_diff([],header.splitlines(keepends=True),fromfile='/dev/null',tofile='b/glib/glpixelstorescope.h',lineterm='\n'))
name='0018-texture-transfer-alignment-and-state.patch';patchfile=out/'patches'/name;patchfile.write_bytes(patch.encode())
shutil.copy2(patchfile,ROOT/'experiments/gx-application/patches'/name)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
lock['patches'].append(dict(file='patches/'+name,sha256=sha(patchfile)))
for file,is_new in [('glib/gltexture.cpp',False),('glib/glpixelstorescope.h',True)]:
 existing=next((row for row in lock['patched_files'] if row['file']==file),None)
 record=dict(file=file,sha256=sha(out/'upstream'/file))
 if is_new:record['new_file']=True
 if existing is None:lock['patched_files'].append(record)
 else:existing.update(record)
lock['evaluation_scope']='isolated candidate: external GX scene, owned-memory image decoding and 2D packed pixel-store transfer/state remediation; not GPU/UI or full-port qualification'
(out/'source.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
app=out/'app/AppScope/app.json5';s=app.read_text();assert '0.22.6' in s and '220600' in s;app.write_text(s.replace('0.22.6','0.22.8').replace('220600','220800'))
print(json.dumps(dict(source=str(out),patches=len(lock['patches']),new_patch_sha256=sha(patchfile),production_unchanged=True,real_gpu_executed=False),indent=2))
