#!/usr/bin/env python3
"""Isolated, labelled synthetic negative fixtures; never change real evidence."""
import hashlib,importlib.util,json,re,sys,tempfile,unittest
from pathlib import Path
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('shared_validator',here/'original_renderer_validation.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
source,before_source,after_source,hap=(Path(sys.argv.pop(1)).resolve() for _ in range(4))
from renderer_probe_identity import package_identity
expected_identity=package_identity(hap)
class SharedValidatorTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='isolated-synthetic-negative-',dir=here)
  self.root=Path(self.temp.name);self.folder=self.root/'synthetic-original';self.before=self.root/'before';self.after=self.root/'after'
  for dest,src in ((self.folder,source),(self.before,before_source),(self.after,after_source)):
   dest.mkdir()
   for item in src.iterdir():
    if item.is_file(): (dest/item.name).symlink_to(item)
  self.run=json.loads((self.folder/'received-artifacts.json').read_text())['run']
  self.name=f'shared-original-integrated-run-{self.run}.json';self.d=json.loads((self.folder/self.name).read_text())
  (self.root/'SYNTHETIC-NEGATIVE-FIXTURE.txt').write_text('Intentionally altered local copies for validator rejection tests. Not device observations or genuine target hashes.\n')
 def tearDown(self):self.temp.cleanup()
 def replace(self,path,value):
  if path.is_symlink():path.unlink()
  path.write_bytes(value if isinstance(value,bytes) else value.encode())
 def write(self,name,data,rehash=True):
  path=self.folder/name;self.replace(path,data if isinstance(data,bytes) else json.dumps(data))
  if rehash:
   m=json.loads((self.folder/'received-artifacts.json').read_text());r=m['files'][name];r.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(),target_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),bytes=path.stat().st_size)
   self.replace(self.folder/'received-artifacts.json',json.dumps(m))
 def integrated(self):self.write(self.name,self.d)
 def result(self):return v.validate(self.folder,self.before,self.after,expected_identity,create_previews=False)
 def reject(self,name):
  r=self.result();self.assertFalse(r['passed']);self.assertTrue(any(c['name']==name and not c['passed'] for c in r['checks']),[c['name'] for c in r['checks'] if not c['passed']])
 def gpu(self):return json.loads((self.folder/f'actual-gpu-run-{self.run}-triangle.json').read_text())
 def host_after(self,fn):
  path=self.after/'native-verification.json';d=json.loads(path.read_text());fn(d);self.replace(path,json.dumps(d))
 def test_wrong_expected_package_identity_rejected(self):
  wrong=dict(expected_identity,versionCode=expected_identity['versionCode']+1)
  r=v.validate(self.folder,self.before,self.after,wrong,create_previews=False)
  self.assertFalse(r['passed'])
  self.assertTrue(any(c['name']=='shared candidate identity before' and not c['passed'] for c in r['checks']))
 def test_01_genuine_baseline(self):self.assertTrue(self.result()['passed'])
 def test_02_all_background(self):
  d=self.d['original']['legacy'];self.write(f'legacy-run-{self.run}-triangle.rgba',bytes([10,15,20,255])*(d['width']*d['height']));self.reject('triangle independent pixel masks')
 def test_03_uniform_not_intent(self):
  d=self.gpu();next(u for u in d['active_uniform_values'] if u['name']=='_gl4es_ModelViewProjectionMatrix')['values'][0]+=1;self.write(f'actual-gpu-run-{self.run}-triangle.json',d);self.reject('triangle GPU _gl4es_ModelViewProjectionMatrix')
 def test_04_link_log_error_despite_status(self):
  d=self.gpu();d['program_info']='synthetic attached shader not compiled';self.write(f'actual-gpu-run-{self.run}-triangle.json',d);self.reject('triangle actual driver program reflected')
 def test_05_compile_status_failure(self):
  name=f'actual-driver-trace-run-{self.run}.log';t=(self.folder/name).read_text();shader=re.findall(r'FPE ESSL300 adaptation shader=(\d+)',t)[-1];self.write(name,t.replace(f'compile {shader} status=1',f'compile {shader} status=0').encode());self.reject('generated FPE vertex and fragment compiled')
 def test_06_callback_missing(self):self.d['original']['legacy']['cases'][0]['object_orient']=0;self.integrated();self.reject('triangle genuine callback object_orient')
 def test_07_external_binding_not_restored(self):self.d['external_egl_restored']=False;self.integrated();self.reject('external_egl_restored')
 def test_08_temporary_context_not_disposed(self):self.d['original']['temporary_context_destroyed']=False;self.integrated();self.reject('temporary_context_destroyed')
 def test_09_next_frame_not_drawn(self):self.d['main_frames_after']=self.d['main_frames_before'];self.integrated();self.reject('actual next main frame')
 def test_10_wrong_parent_uniform(self):
  name=f'actual-gpu-run-{self.run}-parent-after.json';d=json.loads((self.folder/name).read_text());d['active_uniform_values'][0]['values'][0]+=1;self.write(name,d);self.reject('actual parent program/uniform/state exact restoration')
 def test_11_wrong_matrix_despite_native_pass(self):
  self.host_after(lambda d:d['graphics']['mvpMatrix'].__setitem__(0,5));self.reject('main independent camera uniforms after')
 def test_12_graphics_uninitialized(self):self.host_after(lambda d:d['graphics'].__setitem__('initialized',False));self.reject('main actual graphics after')
 def test_13_duplicate_runtime_registry_changed(self):self.host_after(lambda d:d['applicationRuntime'].__setitem__('originalCoreMacroCount',0));self.reject('single empty original runtime after')
 def test_14_command_state_changed(self):self.host_after(lambda d:d['desktopCommand'].__setitem__('sequence',999));self.reject('existing host unchanged desktopCommand')
 def test_15_wrong_process_after(self):self.replace(self.after/'pid.txt','999999\n');self.reject('same live process before and after')
 def test_16_old_artifact_directory(self):self.d['original']['artifact_subdirectory']='synthetic-stale-run';self.integrated();self.reject('exclusive current-process artifact identity')
 def test_17_target_hash_mismatch(self):
  name=f'actual-driver-trace-run-{self.run}.log';self.write(name,(self.folder/name).read_bytes()+b'\nsynthetic mismatch',rehash=False);self.reject('all original artifacts match received target hashes')
 def test_18_truncated_pixels(self):self.write(f'legacy-run-{self.run}-triangle.rgba',b'abc');self.reject('triangle exact readback size')
 def test_19_wrong_window_viewport(self):d=self.gpu();d['viewport'][2]=4;self.write(f'actual-gpu-run-{self.run}-triangle.json',d);self.reject('triangle actual window framebuffer')
 def test_20_wrong_bundle(self):self.host_after(lambda d:d['installedPackage'].__setitem__('bundleName','synthetic.other.bundle'));self.reject('shared candidate identity after')
 def test_21_duplicate_report_disagrees(self):
  name=f'shared-original-window-run-{self.run}.json';d=json.loads((self.folder/name).read_text());d['pid']=999999;self.write(name,d);self.reject('standalone original report equals integrated report')
 def test_22_missing_required_hash_binding(self):
  name=f'actual-gpu-run-{self.run}-triangle.json';path=self.folder/'received-artifacts.json';d=json.loads(path.read_text());del d['files'][name];self.replace(path,json.dumps(d));self.reject('all required artifacts independently hash-bound')
 def test_23_original_control_clipped(self):
  from layout_validation import attributes
  path=self.before/'layout.json';d=json.loads(path.read_text());node=next(n for n in attributes(d) if n.get('id')=='olex2-view-reset');node['origBounds']='[0,0][999999,999999]';self.replace(path,json.dumps(d));self.reject('actual controls unclipped before')
 def test_24_original_control_after_clipped(self):
  from layout_validation import attributes
  path=self.after/'layout.json';d=json.loads(path.read_text());node=next(n for n in attributes(d) if n.get('id')=='olex2-view-zoom');node['origBounds']='[0,0][999999,999999]';self.replace(path,json.dumps(d));self.reject('actual controls unclipped after')
 def test_25_nonfinite_original_matrix(self):
  self.d['original']['legacy']['cases'][0]['projection'][3]=float('nan');self.integrated();self.reject('triangle independent projection')
unittest.main(verbosity=2)
