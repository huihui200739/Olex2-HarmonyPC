"""Hash-bound, real-window synthetic texture continuation across separate contexts.

A run 1 alone never establishes migration. Each continuation re-reads and validates
both process-scoped folders and requires an actually destroyed previous context.
"""
import hashlib,json,math
from pathlib import Path
from texture_1d_window_validation import PALETTE,STORES
SPECS=[(w,c,f'persistent-texture-1d-w{w}-'+('rgb' if c==3 else 'rgba')) for w in (3,5) for c in (3,4)]
def integer(value,minimum=0):return type(value) is int and value>=minimum

def _single(folder):
 folder=Path(folder);checks=[];summary={}
 def check(name,ok,detail=None):checks.append(dict(name=name,passed=bool(ok),detail=detail))
 try:
  received=json.loads((folder/'received-artifacts.json').read_text());run=received['run'];pid=received['pid']
  check('strict positive run and process identity',integer(run,1) and isinstance(pid,str) and pid.isdigit() and int(pid)>0)
  if not checks[-1]['passed']:raise ValueError('Invalid run or PID')
  integrated_name=f'shared-original-integrated-run-{run}.json'
  integrated=json.loads((folder/integrated_name).read_text());original=integrated['original'];legacy=original['legacy'];probe=legacy['persistent_textures']
  w,h=legacy['width'],legacy['height']
  check('bounded actual window',integer(w,64) and integer(h,64) and w<=4096 and h<=4096)
  if not checks[-1]['passed']:raise ValueError('Unbounded window')
  check('exact process-scoped run',integer(integrated['run'],1) and integer(original['run'],1) and integer(legacy['run'],1) and run==integrated['run']==original['run']==legacy['run'] and str(original['pid'])==pid and original['artifact_subdirectory']==received['artifact_subdirectory']==f'original-renderer-pid-{pid}-run-{run}')
  check('real isolated context completed and destroyed',all(original.get(key) is True for key in ('shared_existing_runtime','same_main_window_surface','original_context_isolated','swapped','temporary_context_detached','temporary_context_destroyed','parent_state_restored')) and original.get('error')=='' and legacy.get('execution_completed') is True)
  check('explicit retained renderer-manager route',type(probe.get('schema')) is int and probe['schema']==1 and probe.get('route')=='original-renderer-manager-across-real-contexts' and probe.get('offscreen') is False and type(probe.get('run')) is int and probe['run']==run)
  check('texture manager actually detached before compatibility close',legacy.get('persistent_textures_detached_before_gl4es_close') is True)
  manager=probe['manager_address'];check('positive live manager address',integer(manager,1))
  cases=probe['cases'];check('four exact ordered synthetic cases',len(cases)==4 and [(c['texture_width'],c['channels'],c['name']) for c in cases]==SPECS)
  if not checks[-1]['passed']:raise ValueError('Missing or unexpected cases')
  files=received['files'];required={integrated_name,f'shared-original-window-run-{run}.json',f'actual-driver-trace-run-{run}.log'}
  for _,_,name in SPECS:required|={f'persistent-run-{run}-{name}.rgba',f'persistent-run-{run}-{name}-cpu.rgba',f'actual-gpu-run-{run}-{name}.json'}
  check('explicit local and target hash binding',required<=files.keys() and all(Path(n).name==n and len(r['sha256'])==64 and r['sha256']==r['target_sha256']==hashlib.sha256((folder/n).read_bytes()).hexdigest() and integer(r['bytes']) and (folder/n).stat().st_size==r['bytes'] for n,r in files.items()))
  window=json.loads((folder/f'shared-original-window-run-{run}.json').read_text())
  check('integrated original equals captured window record',original==window)
  addresses={};samples={}
  for case,(tw,channels,name) in zip(cases,SPECS):
   address=case['texture_address'];addresses[name]=address
   check(name+' live texture identity',integer(address,1) and integer(case['resident_id'],1) and case['resident_id']<2**32-1)
   check(name+' explicit actual generation',case['restored_from_prior_context'] is (run>1))
   check(name+' transfer state preserved',case['caller_before']==case['caller_after']==STORES)
   check(name+' stored nearest min/mag filters reapplied',type(case['min_filter']) is int and type(case['mag_filter']) is int and case['min_filter']==case['mag_filter']==9728)
   # This candidate explicitly reapplies wrap_s in Draw; do NOT claim it proves
   # preservation of the wrapping property across contexts.
   check(name+' explicit draw-time edge wrap',type(case['wrap_s']) is int and case['wrap_s']==33071)
   expected=bytes(channel if i!=3 or channels==4 else 255 for color in PALETTE[:tw] for i,channel in enumerate(color))
   cpu=(folder/f'persistent-run-{run}-{name}-cpu.rgba').read_bytes()
   check(name+' exact original readback RGBA',cpu==expected)
   gpu=json.loads((folder/f'actual-gpu-run-{run}-{name}.json').read_text())
   check(name+' real driver framebuffer and sampler program',integer(gpu.get('program'),1) and gpu.get('framebuffer')==0 and gpu.get('viewport')==[0,0,w,h] and gpu.get('link_status')==1 and gpu.get('program_info')=='' and gpu.get('driver_error_before_snapshot')==gpu.get('driver_error')==0 and any(u.get('type')==35678 and u.get('size')==1 for u in gpu.get('active_uniform_values',[])))
   raw=(folder/f'persistent-run-{run}-{name}.rgba').read_bytes()
   check(name+' exact framebuffer byte length',len(raw)==w*h*4)
   samples[name]=dict(cpu=hashlib.sha256(cpu).hexdigest(),gpu=hashlib.sha256(raw).hexdigest())
   if len(raw)!=w*h*4:continue
   step=max(1,min(7,h//128));tested=0;bad=[]
   for y in range(2,h-2,step):
    for x in range(2,w-2,step):
     scaled=(x+.5)*tw/w;fraction=scaled-math.floor(scaled)
     if fraction<.05 or fraction>.95:continue
     expected=list(PALETTE[min(tw-1,int(scaled))]);expected[3]=expected[3] if channels==4 else 255
     actual=list(raw[(y*w+x)*4:(y*w+x+1)*4]);tested+=1
     if any(abs(a-b)>2 for a,b in zip(actual,expected)) and len(bad)<12:bad.append(dict(x=x,y=y,actual=actual,expected=expected))
   check(name+' independent GPU color and alpha samples',tested>1000 and not bad,dict(samples=tested,failures=bad))
  check('four distinct original texture objects',len(set(addresses.values()))==4 and manager not in addresses.values())
  summary=dict(pid=pid,run=run,manager_address=manager,texture_addresses=addresses,sample_hashes=samples,width=w,height=h,folder=str(folder),integrated_sha256=hashlib.sha256((folder/integrated_name).read_bytes()).hexdigest())
 except (OSError,KeyError,ValueError,TypeError,IndexError,AttributeError) as error:
  check('complete readable retained texture evidence',False,str(error))
 return dict(passed=bool(checks) and all(c['passed'] for c in checks),checks=checks,**summary)

def validate(folder,previous_folder=None):
 current=_single(folder);checks=list(current['checks']);prior=None
 def check(name,ok,detail=None):checks.append(dict(name=name,passed=bool(ok),detail=detail))
 if previous_folder is None:
  check('initial generation is not migration proof',current.get('run')==1)
 else:
  prior=_single(previous_folder)
  check('both original context observations independently pass',current['passed'] and prior['passed'],dict(previous_checks=len(prior['checks']),previous_failures=[c for c in prior['checks'] if not c['passed']]))
  check('same process consecutive distinct context leases',current.get('pid')==prior.get('pid') and integer(current.get('run'),2) and current['run']==prior.get('run',-2)+1 and Path(folder).resolve()!=Path(previous_folder).resolve())
  check('same original manager and four texture objects',current.get('manager_address')==prior.get('manager_address') and current.get('texture_addresses')==prior.get('texture_addresses'))
  check('same bounded window and exact GPU and CPU bytes',current.get('width')==prior.get('width') and current.get('height')==prior.get('height') and current.get('sample_hashes')==prior.get('sample_hashes'))
 passed=current['passed'] and all(c['passed'] for c in checks)
 return dict(passed=passed,checks=checks,observation={k:v for k,v in current.items() if k not in ('checks','passed')},previous_observation=None if prior is None else {k:v for k,v in prior.items() if k not in ('checks','passed')},actual_context_migration_verified=bool(passed and prior is not None),full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,scope='Four original-renderer-owned synthetic level-0 1D RGB/RGBA textures over separately created and destroyed actual-window contexts; nearest filters reapplied; wrapping explicitly set per draw. Not LoadTextures/fonts/full UI or exceptional resource recovery.')
