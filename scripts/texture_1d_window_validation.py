#!/usr/bin/env python3
"""Independently inspect bounded synthetic 1D texture samples in a real window.

CPU texture-read success is not GPU evidence. Require received/hash-bound actual
framebuffer bytes and driver program reflection for both resource generations.
This does not validate actual context migration, a persistent GX owner or fonts.
"""
import argparse,hashlib,json,math,re
from pathlib import Path
PALETTE=((255,0,0,255),(0,255,0,191),(0,0,255,127),(255,255,0,63),(255,0,255,0))
STORES=[8,13,2,3,8,11,3,2]
def validate(folder):
 folder=Path(folder);checks=[]
 def check(name,ok,detail=None):checks.append(dict(name=name,passed=bool(ok),detail=detail))
 try:
  received=json.loads((folder/'received-artifacts.json').read_text());run=received['run']
  integrated=json.loads((folder/f'shared-original-integrated-run-{run}.json').read_text());original=integrated['original'];legacy=original['legacy'];probe=legacy['texture_1d']
  w,h=legacy['width'],legacy['height']
  check('bounded actual window size',type(w) is int and type(h) is int and 64<=w<=4096 and 64<=h<=4096)
  if not checks[-1]['passed']:raise ValueError('Missing bounded actual window size')
  check('exclusive current-process run',type(run) is int and run==integrated['run']==original['run']==legacy['run'] and str(original['pid'])==received['pid'] and original['artifact_subdirectory']==received['artifact_subdirectory']==f"original-renderer-pid-{received['pid']}-run-{run}")
  check('explicit current-window route',probe['schema']==1 and probe['route']=='original-manager-current-window' and probe['offscreen'] is False and probe['resource_recreation']=='same-current-context-only')
  check('full qualification remains unclaimed',probe['physical_pc_verified'] is False and probe['full_port_complete'] is False)
  specs=[(tw,channels,f'texture-1d-w{tw}-'+('rgb' if channels==3 else 'rgba')) for tw in (3,5) for channels in (3,4)]
  cases=probe['cases'];check('four ordered RGB/RGBA sample cases',[(c['texture_width'],c['channels'],c['name']) for c in cases]==specs)
  required={f'shared-original-integrated-run-{run}.json',f'shared-original-window-run-{run}.json',f'actual-driver-trace-run-{run}.log'}
  for _,_,name in specs:
   for generation in (0,1):
    label=f'{name}-generation-{generation}'
    required|={f'legacy-run-{run}-{label}.rgba',f'actual-gpu-run-{run}-{label}.json'}
  files=received['files']
  check('all texture artifacts explicitly target-hash bound',required<=files.keys() and all(Path(name).name==name and (folder/name).is_file() and hashlib.sha256((folder/name).read_bytes()).hexdigest()==record['sha256']==record['target_sha256'] and (folder/name).stat().st_size==record['bytes'] for name,record in files.items()))
  trace=(folder/f'actual-driver-trace-run-{run}.log').read_text()
  adapted=re.findall(r'FPE ESSL300 adaptation shader=(\d+) type=(\d+)',trace)
  compiled={int(i):int(ok) for i,ok in re.findall(r'compile (\d+) status=(\d+)',trace)}
  check('three exact FPE vertex/fragment variants compiled',len(adapted)==6 and len({i for i,_ in adapted})==6 and sorted(int(t) for _,t in adapted)==[35632]*3+[35633]*3 and all(compiled.get(int(i))==1 for i,_ in adapted))
  check('texture and original geometry reach actual driver',len(re.findall(r'draw(?:Arrays|Elements) mode=',trace))>=11)
  for case,(tw,channels,name) in zip(cases,specs):
   check(name+' preserves caller upload/destruction stores',case['caller_before']==case['caller_after_destruction']==STORES)
   check(name+' owned final texture released',case['last_texture_released'] is True)
   generations=case['generations'];check(name+' exact two resource generations',[g['generation'] for g in generations]==[0,1])
   original_raw=None
   for g in generations:
    generation=g['generation'];label=f'{name}-generation-{generation}'
    check(label+' original manager identity/transfer state',type(g['texture_id']) is int and g['texture_id']>0 and g['manager_lookup_matches'] is True and g['read_width']==tw and g['read_height']==1 and g['caller_after']==STORES)
    check(label+' restores nonzero caller 2D binding',type(g.get('previous_2d_binding')) is int and g['previous_2d_binding']>0 and g['previous_2d_binding']==g.get('after_read_2d_binding'))
    gpu=json.loads((folder/f'actual-gpu-run-{run}-{label}.json').read_text())
    uniforms=gpu.get('active_uniform_values',[])
    check(label+' actual driver framebuffer/program',gpu.get('program',0)>0 and gpu.get('framebuffer')==0 and gpu.get('viewport')==[0,0,w,h] and gpu.get('link_status')==1 and gpu.get('program_info')=='' and gpu.get('driver_error_before_snapshot')==gpu.get('driver_error')==0 and any(u.get('type')==35678 and u.get('size')==1 for u in uniforms))
    raw=(folder/f'legacy-run-{run}-{label}.rgba').read_bytes();check(label+' exact framebuffer size',len(raw)==w*h*4)
    if len(raw)!=w*h*4:continue
    step=max(1,min(7,h//128));tested=0;bad=[]
    for y in range(2,h-2,step):
     for x in range(2,w-2,step):
      scaled=(x+.5)*tw/w;fraction=scaled-math.floor(scaled)
      if fraction<.05 or fraction>.95:continue
      reference=list(PALETTE[min(tw-1,int(scaled))]);reference[3]=255 if channels==3 else reference[3]
      pixel=list(raw[(y*w+x)*4:(y*w+x+1)*4]);tested+=1
      if any(abs(a-b)>2 for a,b in zip(pixel,reference)) and len(bad)<12:bad.append(dict(x=x,y=y,actual=pixel,expected=reference))
    check(label+' independent horizontal color/alpha stripes',tested>1000 and not bad,dict(tested=tested,failures=bad,sha256=hashlib.sha256(raw).hexdigest()))
    if original_raw is None:original_raw=raw
    else:check(name+' identical GPU samples after recreation',raw==original_raw)
 except (OSError,KeyError,ValueError,TypeError,IndexError) as e:
  check('complete readable 1D window evidence',False,str(e))
 return dict(passed=bool(checks) and all(c['passed'] for c in checks),checks=checks,functional_completion_percent=None,full_port_complete=False,physical_pc_verified=False,actual_context_migration_verified=False,scope='Four normal synthetic 1D textures sampled into the actual current window, two same-context resource generations; not persistent GX ownership, context migration, fonts or full rendering/UI qualification')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=Path);a=p.parse_args();r=validate(a.folder)
 (a.folder/'independent-texture-1d-validation.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(dict(passed=r['passed'],checks=len(r['checks']),failed=[c for c in r['checks'] if not c['passed']]),ensure_ascii=False,indent=2));raise SystemExit(0 if r['passed'] else 1)
