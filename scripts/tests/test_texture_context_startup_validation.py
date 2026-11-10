"""Captured candidate14 startup and isolated mutations, not new device runs."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from texture_context_startup_validation import validate_runtime_snapshot
class TextureContextStartupValidation(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.reference=json.loads((Path(__file__).parent/'fixtures/texture_context_runtime_startup.json').read_text())
 def setUp(self):self.data=copy.deepcopy(self.reference);self.identity=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.14',versionCode=221400)
 def passed(self):
  r=validate_runtime_snapshot(self.data,self.identity);self.assertEqual(len(r['checks']),10);return r['passed']
 def test_captured_initialized_runtime(self):self.assertTrue(self.passed())
 def test_old_package_rejected(self):
  self.data['installedPackage']['versionCode']=221100;self.assertFalse(self.passed())
 def test_wrong_profile_identity_rejected(self):
  self.identity['versionName']='0.22.13'
  with self.assertRaises(ValueError):self.passed()
 def test_recursive_counts_not_substituted_for_root_counts(self):
  self.data['applicationRuntime']['originalCoreMacroCount']=196;self.data['applicationRuntime']['originalCoreFunctionCount']=52;self.assertFalse(self.passed())
 def test_runtime_uninitialized(self):
  self.data['applicationRuntime']['initialized']=False;self.assertFalse(self.passed())
 def test_old_runtime_source(self):
  self.data['applicationRuntime']['source']='TXApp';self.assertFalse(self.passed())
 def test_status_flags_not_a_matrix_proof(self):
  self.data['graphics']['mvpMatrix'][0]+=.05;self.assertFalse(self.passed())
 def test_no_replayed_log_or_false_completion(self):
  for key in ('fullPortComplete','originalOlex2RendererTested'):
   self.data=copy.deepcopy(self.reference);self.data[key]=True;self.assertFalse(self.passed())
  self.data=copy.deepcopy(self.reference);self.data['originalLog']['clearCount']=1;self.assertFalse(self.passed())
 def test_unsaved_workspace(self):
  self.data['workspaceState']['saveStatus']='pending';self.assertFalse(self.passed())
 def test_missing_or_empty_actual_checks(self):
  for checks,count in ((None,10),([],0),([],10)):
   self.data['checks']=checks;self.data['checkCount']=count;self.assertFalse(self.passed())
 def test_missing_main_surface_or_independent_probe(self):
  self.data['graphics']['surfaceReady']=False;self.assertFalse(self.passed())
  self.data=copy.deepcopy(self.reference);self.data['graphics']['linePathProbe']['verified']=False;self.assertFalse(self.passed())
if __name__=='__main__':unittest.main()
