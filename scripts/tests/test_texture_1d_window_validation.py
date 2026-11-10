#!/usr/bin/env python3
"""Isolated, explicitly synthetic fixtures testing the 1D evidence verifier."""
import copy,hashlib,json,tempfile,unittest,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from texture_1d_window_validation import PALETTE,STORES,validate
class Validation(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.folder=Path(self.temp.name);self.files={};w,h=96,64;run=1;pid='12345';cases=[]
  for tw in (3,5):
   for channels in (3,4):
    name=f'texture-1d-w{tw}-'+('rgb' if channels==3 else 'rgba');gens=[]
    row=b''.join(bytes(PALETTE[int((x+.5)*tw/w)][:3]+(255 if channels==3 else PALETTE[int((x+.5)*tw/w)][3],)) for x in range(w))
    for generation in (0,1):
     label=f'{name}-generation-{generation}';gens.append(dict(generation=generation,texture_id=10+generation,previous_2d_binding=7,after_read_2d_binding=7,manager_lookup_matches=True,read_width=tw,read_height=1,caller_after=STORES.copy()))
     self.write(f'legacy-run-{run}-{label}.rgba',row*h)
     self.write(f'actual-gpu-run-{run}-{label}.json',dict(program=3,framebuffer=0,viewport=[0,0,w,h],link_status=1,program_info='',driver_error_before_snapshot=0,driver_error=0,active_uniform_values=[dict(type=35678,size=1)]))
    cases.append(dict(name=name,texture_width=tw,channels=channels,caller_before=STORES.copy(),caller_after_destruction=STORES.copy(),last_texture_released=True,generations=gens))
  probe=dict(schema=1,route='original-manager-current-window',offscreen=False,resource_recreation='same-current-context-only',physical_pc_verified=False,full_port_complete=False,cases=cases)
  self.integrated=dict(run=1,original=dict(run=1,pid=int(pid),artifact_subdirectory=f'original-renderer-pid-{pid}-run-1',legacy=dict(run=1,width=w,height=h,texture_1d=probe)))
  self.write('shared-original-window-run-1.json',self.integrated['original']);self.write('shared-original-integrated-run-1.json',self.integrated)
  self.trace=''.join(f'FPE ESSL300 adaptation shader={i} type={35633 if i%2==0 else 35632}\ncompile {i} status=1\n' for i in range(4,10))+'drawArrays mode=4\n'*3+'drawElements mode=4\n'*8;self.write('actual-driver-trace-run-1.log',self.trace)
  self.received=dict(run=1,pid=pid,artifact_subdirectory=f'original-renderer-pid-{pid}-run-1',files=self.files);self.manifest()
 def write(self,name,data):
  if isinstance(data,dict):data=json.dumps(data).encode()
  if isinstance(data,str):data=data.encode()
  (self.folder/name).write_bytes(data);sha=hashlib.sha256(data).hexdigest();self.files[name]=dict(sha256=sha,target_sha256=sha,bytes=len(data))
 def manifest(self):
  (self.folder/'received-artifacts.json').write_text(json.dumps(self.received))
 def mutate(self,fn):
  fn(self.integrated['original']['legacy']['texture_1d']);self.write('shared-original-integrated-run-1.json',self.integrated);self.manifest()
 def reject(self):self.assertFalse(validate(self.folder)['passed'])
 def test_positive_explicitly_synthetic(self):self.assertTrue(validate(self.folder)['passed'])
 def test_missing_cases(self):self.mutate(lambda p:p['cases'].pop());self.reject()
 def test_duplicate_generation(self):self.mutate(lambda p:p['cases'][0]['generations'][1].update(generation=0));self.reject()
 def test_offscreen(self):self.mutate(lambda p:p.update(offscreen=True));self.reject()
 def test_wrong_context_claim(self):self.mutate(lambda p:p.update(resource_recreation='full-context-migration'));self.reject()
 def test_state_not_restored(self):self.mutate(lambda p:p['cases'][1]['generations'][0]['caller_after'].__setitem__(1,0));self.reject()
 def test_manager_mismatch(self):self.mutate(lambda p:p['cases'][0]['generations'][1].update(manager_lookup_matches=False));self.reject()
 def test_2d_binding_not_restored(self):self.mutate(lambda p:p['cases'][0]['generations'][0].update(after_read_2d_binding=0));self.reject()
 def test_resource_not_released(self):self.mutate(lambda p:p['cases'][0].update(last_texture_released=False));self.reject()
 def test_empty_texture_id(self):self.mutate(lambda p:p['cases'][0]['generations'][0].update(texture_id=0));self.reject()
 def test_truncated_pixels(self):
  self.write('legacy-run-1-texture-1d-w3-rgb-generation-0.rgba',b'abc');self.manifest();self.reject()
 def test_wrong_pixels_despite_native_flags(self):
  name='legacy-run-1-texture-1d-w3-rgb-generation-0.rgba';self.write(name,b'\0'*(96*64*4));self.manifest();self.reject()
 def test_wrong_alpha(self):
  name='legacy-run-1-texture-1d-w5-rgba-generation-1.rgba';data=bytearray((self.folder/name).read_bytes());data[3::4]=b'\xff'*(96*64);self.write(name,bytes(data));self.manifest();self.reject()
 def test_target_hash_mismatch(self):
  next(iter(self.files.values()))['target_sha256']='0'*64;self.manifest();self.reject()
 def test_missing_required_binding(self):
  del self.files['actual-gpu-run-1-texture-1d-w5-rgba-generation-1.json'];self.manifest();self.reject()
 def test_cpu_only_no_driver_submission(self):
  self.write('actual-driver-trace-run-1.log','drawArrays mode=4\n'*3);self.manifest();self.reject()
 def test_missing_sampler(self):
  name='actual-gpu-run-1-texture-1d-w3-rgb-generation-0.json';d=json.loads((self.folder/name).read_text());d['active_uniform_values']=[];self.write(name,d);self.manifest();self.reject()
 def test_wrong_window(self):
  name='actual-gpu-run-1-texture-1d-w3-rgb-generation-0.json';d=json.loads((self.folder/name).read_text());d['framebuffer']=4;self.write(name,d);self.manifest();self.reject()
 def test_missing_shader_variant(self):
  self.write('actual-driver-trace-run-1.log',self.trace.replace('FPE ESSL300 adaptation shader=9 type=35632','absent'));self.manifest();self.reject()
 def test_failed_shader_variant(self):
  self.write('actual-driver-trace-run-1.log',self.trace.replace('compile 8 status=1','compile 8 status=0'));self.manifest();self.reject()
 def test_full_port_claim(self):self.mutate(lambda p:p.update(full_port_complete=True));self.reject()
 def test_wrong_pid(self):
  self.received['pid']='99999';self.manifest();self.reject()
 def test_incomplete_input_fails_closed(self):
  (self.folder/'received-artifacts.json').write_text('{}');self.reject()
if __name__=='__main__':unittest.main(verbosity=2)
