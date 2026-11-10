"""Clearly isolated synthetic verifier fixtures, never emulator observations."""
import hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from texture_context_window_validation import validate,SPECS
from texture_1d_window_validation import PALETTE,STORES

class RetainedTextureVerifierTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='olex2-synthetic-texture-context-');self.root=Path(self.tmp.name)
  self.first=self.make(1);self.second=self.make(2)
 def tearDown(self):self.tmp.cleanup()
 def make(self,run,pid='1234'):
  path=self.root/f'pid-{pid}-run-{run}';path.mkdir();w=h=80
  cases=[]
  def save(name,value):
   if isinstance(value,dict):value=(json.dumps(value)+'\n').encode()
   if isinstance(value,str):value=value.encode()
   (path/name).write_bytes(value)
  for index,(tw,channels,name) in enumerate(SPECS):
   cases.append(dict(name=name,texture_width=tw,channels=channels,texture_address=1000+index, resident_id=index+1,restored_from_prior_context=run>1,min_filter=9728,mag_filter=9728,wrap_s=33071,caller_before=STORES[:],caller_after=STORES[:]))
   cpu=bytes(c if i!=3 or channels==4 else 255 for color in PALETTE[:tw] for i,c in enumerate(color))
   raw=bytes(c if i!=3 or channels==4 else 255 for y in range(h) for x in range(w) for i,c in enumerate(PALETTE[min(tw-1,int((x+.5)*tw/w))]))
   save(f'persistent-run-{run}-{name}-cpu.rgba',cpu);save(f'persistent-run-{run}-{name}.rgba',raw)
   save(f'actual-gpu-run-{run}-{name}.json',dict(synthetic_test_fixture=True,program=99,framebuffer=0,viewport=[0,0,w,h],link_status=1,program_info='',driver_error_before_snapshot=0,driver_error=0,active_uniform_values=[dict(type=35678,size=1)]))
  legacy=dict(run=run,width=w,height=h,execution_completed=True,persistent_textures_detached_before_gl4es_close=True,persistent_textures=dict(schema=1,route='original-renderer-manager-across-real-contexts',offscreen=False,run=run,manager_address=900,cases=cases))
  original=dict(synthetic_test_fixture=True,run=run,pid=int(pid),artifact_subdirectory=f'original-renderer-pid-{pid}-run-{run}',shared_existing_runtime=True,same_main_window_surface=True,original_context_isolated=True,swapped=True,temporary_context_detached=True,temporary_context_destroyed=True,parent_state_restored=True,legacy=legacy,error='')
  save(f'shared-original-window-run-{run}.json',original)
  save(f'shared-original-integrated-run-{run}.json',dict(synthetic_test_fixture=True,run=run,original=original))
  save(f'actual-driver-trace-run-{run}.log','synthetic verifier input; not a driver capture\n')
  manifest=dict(pid=pid,run=run,artifact_subdirectory=original['artifact_subdirectory'],files={})
  for file in path.iterdir():
   data=file.read_bytes();sha=hashlib.sha256(data).hexdigest();manifest['files'][file.name]=dict(sha256=sha,target_sha256=sha,bytes=len(data))
  (path/'received-artifacts.json').write_text(json.dumps(manifest))
  return path
 def bind(self,path,name):
  p=path/'received-artifacts.json';m=json.loads(p.read_text());data=(path/name).read_bytes();sha=hashlib.sha256(data).hexdigest();m['files'][name]=dict(sha256=sha,target_sha256=sha,bytes=len(data));p.write_text(json.dumps(m))
 def mutate(self,path,change,bind=True):
  m=json.loads((path/'received-artifacts.json').read_text());run=m['run'];name=f'shared-original-integrated-run-{run}.json';d=json.loads((path/name).read_text());change(d);(path/name).write_text(json.dumps(d))
  window=f'shared-original-window-run-{run}.json';(path/window).write_text(json.dumps(d['original']))
  if bind:self.bind(path,name);self.bind(path,window)
 def assertRejected(self):self.assertFalse(validate(self.second,self.first)['passed'])
 def test_initial_alone_is_not_migration(self):
  r=validate(self.first);self.assertTrue(r['passed']);self.assertFalse(r['actual_context_migration_verified'])
 def test_two_separate_bounded_synthetic_observations(self):
  r=validate(self.second,self.first);self.assertTrue(r['passed']);self.assertTrue(r['actual_context_migration_verified']);self.assertIsNone(r['functional_completion_percent']);self.assertFalse(r['full_port_complete'])
 def test_continuation_without_previous_rejected(self):self.assertFalse(validate(self.second)['passed'])
 def test_same_folder_rejected(self):self.assertFalse(validate(self.first,self.first)['passed'])
 def test_old_context_not_destroyed(self):self.mutate(self.first,lambda d:d['original'].update(temporary_context_destroyed=False));self.assertRejected()
 def test_context_not_isolated(self):self.mutate(self.second,lambda d:d['original'].update(original_context_isolated=False));self.assertRejected()
 def test_manager_address_changed(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures'].update(manager_address=901));self.assertRejected()
 def test_texture_address_changed(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'][0].update(texture_address=1111));self.assertRejected()
 def test_duplicate_texture_objects(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'][1].update(texture_address=1000));self.assertRejected()
 def test_bool_manager_address_rejected(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures'].update(manager_address=True));self.assertRejected()
 def test_bool_filter_rejected(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'][0].update(min_filter=True));self.assertRejected()
 def test_not_restored(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'][0].update(restored_from_prior_context=False));self.assertRejected()
 def test_bool_not_actual_true_detach(self):self.mutate(self.second,lambda d:d['original']['legacy'].update(persistent_textures_detached_before_gl4es_close=1));self.assertRejected()
 def test_hash_stale(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'][0].update(resident_id=987),bind=False);self.assertRejected()
 def test_rebound_wrong_cpu_bytes(self):
  name='persistent-run-2-'+SPECS[0][2]+'-cpu.rgba';p=self.second/name;p.write_bytes(bytes(len(p.read_bytes())));self.bind(self.second,name);self.assertRejected()
 def test_rebound_wrong_gpu_bytes(self):
  name='persistent-run-2-'+SPECS[0][2]+'.rgba';p=self.second/name;p.write_bytes(bytes(len(p.read_bytes())));self.bind(self.second,name);self.assertRejected()
 def test_rebound_broken_driver(self):
  name='actual-gpu-run-2-'+SPECS[0][2]+'.json';p=self.second/name;d=json.loads(p.read_text());d['driver_error']=1280;p.write_text(json.dumps(d));self.bind(self.second,name);self.assertRejected()
 def test_previous_observation_rechecked(self):
  name='persistent-run-1-'+SPECS[0][2]+'.rgba';p=self.first/name;p.write_bytes(bytes(len(p.read_bytes())));self.bind(self.first,name);self.assertRejected()
 def test_missing_artifact(self):(self.second/('persistent-run-2-'+SPECS[0][2]+'.rgba')).unlink();self.assertRejected()
 def test_target_binding_differs(self):
  p=self.second/'received-artifacts.json';m=json.loads(p.read_text());next(iter(m['files'].values()))['target_sha256']='0'*64;p.write_text(json.dumps(m));self.assertRejected()
 def test_manifest_path_outside_folder(self):
  p=self.second/'received-artifacts.json';m=json.loads(p.read_text());m['files']['../outside']={};p.write_text(json.dumps(m));self.assertRejected()
 def test_other_pid_not_continuation(self):
  third=self.make(2,pid='1235');self.assertFalse(validate(third,self.first)['passed'])
 def test_missing_case(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures']['cases'].pop());self.assertRejected()
 def test_wrong_environment_type(self):self.mutate(self.second,lambda d:d['original']['legacy']['persistent_textures'].update(offscreen=0));self.assertRejected()

if __name__=='__main__':unittest.main()
