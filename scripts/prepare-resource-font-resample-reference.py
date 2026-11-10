#!/usr/bin/env python3
"""Use independent unchanged fixed wx methods for synthetic256 RGB reference cells."""
import argparse,hashlib,json,struct,subprocess,zipfile
from pathlib import Path
from native_font_progress_validation import ROOT,bound,require
from resource_font_resample_reference import CASES

def prepare(fixtures,executable,out):
 fixtures,executable,out=fixtures.resolve(),executable.resolve(),out.resolve()
 meta=json.loads((fixtures/'manifest.json').read_text());archive=fixtures/'synthetic-resample-deflate.fnt';raw=archive.read_bytes();expected=meta['files'][archive.name]
 require(len(raw)==expected['bytes'] and hashlib.sha256(raw).hexdigest()==expected['sha256'],'Synthetic resample archive changed')
 reference=ROOT/'out/gx-application-evaluation/font-resample-wx-reference24-attempt2'
 original=json.loads((reference/'manifest.json').read_text());require(original['original_method_bodies_unmodified'] and original['generated_sha256']==hashlib.sha256((reference/'original-wx-rgb-methods.cpp').read_bytes()).hexdigest(),'Original wx method reference changed')
 compile_commands=executable.parent/'compile_commands.json';commands=json.loads(compile_commands.read_text())
 for file in (reference/'original-wx-rgb-methods.cpp',ROOT/'experiments/gx-font-resample/tests/reference_cells.cpp'):
  rows=[row for row in commands if row['file']==str(file) and 'resource_font_wx_reference_cells.dir' in row['command']]
  require(len(rows)==1 and '-ffp-contract=off' in rows[0]['command'],'Exact source reference compile recipe missing')
 out.mkdir(parents=True,exist_ok=False);cases=[]
 with zipfile.ZipFile(archive) as original_zip:
  for label,style,points in CASES:
   entry_points=8 if points<8 else 72;entry=style+str(entry_points);payload=original_zip.read(entry);ow,oh=struct.unpack_from('<HH',payload);bits=payload[4:];require(len(bits)==ow*oh*32,'Original bit-array size mismatch')
   rgb=bytearray(ow*oh*3*256)
   for bit in range(ow*oh*256):
    if bits[bit//8]&(1<<(bit%8)):rgb[bit*3]=255
   source=out/(label+'-source.rgb');source.write_bytes(rgb);w,h=int(ow*points/entry_points+0.5),int(oh*points/entry_points+0.5)
   target=out/(label+'-expected.rgb');run=subprocess.run([str(executable),str(ow),str(oh),str(w),str(h),str(source),str(target)],capture_output=True,text=True,check=True)
   cases.append(dict(label=label,entry=entry,points=points,entry_points=entry_points,source_width=ow,source_height=oh,width=w,height=h,fixed=style[0]=='f',source=bound(source),expected=bound(target),reference_output=run.stdout.strip()))
 evidence=[bound(executable),bound(compile_commands),bound(reference/'original-wx-rgb-methods.cpp'),bound(reference/'manifest.json'),bound(ROOT/'out/gx-application-evaluation/wx-image-v3.2.6-reference.cpp'),bound(ROOT/'experiments/gx-font-resample/tests/reference_cells.cpp'),bound(Path(__file__)),bound(ROOT/'scripts/resource_font_resample_reference.py')]
 result=dict(passed=True,scope='Synthetic font opaque RGB cells from unchanged fixed wx3.2.6 methods, minimal storage shim; not full wx runtime, independent GPU or device evidence',archive=bound(archive),fixture_manifest=bound(fixtures/'manifest.json'),cases=cases,evidence=evidence,full_port_complete=False,functional_completion_percent=None)
 (out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('fixtures','reference-executable','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();d=prepare(a.fixtures,a.reference_executable,a.output);print(json.dumps(dict(passed=d['passed'],cases=len(d['cases']),output=str(a.output.resolve()),target_executed=False),indent=2))
