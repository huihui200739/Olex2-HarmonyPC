#!/usr/bin/env python3
"""Retain a fresh raster FMLA proof for the selected actual local source-built HAP."""
import argparse,json,subprocess
from pathlib import Path
from native_font_progress_validation import bound,validate_disassembly
from renderer_probe_identity import package_identity
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--hap',type=Path,required=True);p.add_argument('--output-prefix',type=Path,required=True);a=p.parse_args();hap=a.hap.resolve();identity=package_identity(hap)
 dso=hap.parent/'stage/app/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libolex2core_graphics.so';tools=Path('/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin')
 symbols=subprocess.run([str(tools/'llvm-nm'),'-n','-S','--defined-only',str(dso)],capture_output=True,text=True,check=True).stdout
 rows=[line.split() for line in symbols.splitlines() if line.split()[-1:]==['gl4es_glRasterPos3f']]
 if len(rows)!=1:raise ValueError('Expected unique actual raster function')
 start,size=int(rows[0][0],16),int(rows[0][1],16);end=start+size
 command=[str(tools/'llvm-objdump'),'--disassemble',f'--start-address={start}',f'--stop-address={end}',str(dso)]
 dis=subprocess.run(command,capture_output=True,text=True,check=True).stdout;log=Path(str(a.output_prefix)+'-disassembly.log');proof=Path(str(a.output_prefix)+'-binary-proof.json')
 with log.open('x') as stream:stream.write(dis)
 fresh=subprocess.run(command,capture_output=True,text=True,check=True).stdout;check=validate_disassembly(log.read_text(),fresh,start,end)
 record=dict(identity=identity['versionName'],hap=bound(hap),provenance=bound(hap.with_suffix('.provenance.json')),unstripped_source_build_output=bound(dso),disassembly=bound(log),function='gl4es_glRasterPos3f',function_start=start,function_end=end,actual_float32_viewport_fmla=True,actual_disassembly_check=check,command=command,scope='Actual local source-built AArch64 viewport FMLA; not installed-code hashing or independent GPU line rasterization')
 with proof.open('x') as stream:json.dump(record,stream,indent=2);stream.write('\n')
 print(json.dumps(record,indent=2))
