#!/usr/bin/env python3
"""Alter isolated in-memory copies only; never edit real emulator observations."""
import copy,json,sys,tempfile,unittest
from pathlib import Path
from texture_1d_application_validation import validate_application
from renderer_probe_identity import package_identity
source,report,hap=(Path(sys.argv.pop(1)).resolve() for _ in range(3))
app=json.loads(report.read_text());identity=package_identity(hap)
class EvidenceTests(unittest.TestCase):
 def setUp(self):
  self.app=copy.deepcopy(app);self.identity=dict(identity)
  self.fixture=tempfile.TemporaryDirectory(prefix='isolated-synthetic-1d-application-',dir=source.parent)
  self.addCleanup(self.fixture.cleanup)
  Path(self.fixture.name,'SYNTHETIC-FIXTURE.txt').write_text('Only isolated in-memory UI records are changed. Genuine emulator reports remain read-only.\n')
 def row(self,name):return next(r for r in self.app['checks'] if r['name']==name)
 def reject(self):
  with self.assertRaises(ValueError):validate_application(self.app,source,self.identity)
 def test_positive_actual_observations(self):
  r=validate_application(self.app,source,self.identity);self.assertEqual(r['total_ui_checks'],200);self.assertEqual(r['total_independent_texture_checks'],384);self.assertTrue(r['original_194_ui_checks_preserved']);self.assertFalse(r['actual_context_migration_verified'])
 def test_original_only_194_cannot_qualify_1d(self):
  self.app['checks']=[r for r in self.app['checks'] if not r['name'].startswith('shared_texture_1d_')];self.reject()
 def test_duplicate_ui_check(self):self.app['checks'].append(copy.deepcopy(self.app['checks'][0]));self.reject()
 def test_original_order_changed(self):self.app['checks'][0],self.app['checks'][1]=self.app['checks'][1],self.app['checks'][0];self.reject()
 def test_failed_outer_check(self):self.app['checks'][0]['passed']=False;self.reject()
 def test_original_126_check_missing(self):self.row('shared_original_initial')['details']['checks'].pop();self.reject()
 def test_texture64_check_altered(self):self.row('shared_texture_1d_initial')['details']['checks'][0]['passed']=False;self.reject()
 def test_texture_round_missing(self):self.row('shared_texture_1d_initial')['name']='synthetic-not-the-required-round';self.reject()
 def test_wrong_package(self):self.identity['versionName']='0.22.9';self.reject()
 def test_wrong_same_process_sequence(self):self.row('shared_original_final-same-process-repeat-3')['details']['run']=99;self.reject()
 def test_context_migration_overclaim(self):self.row('shared_texture_1d_initial')['details']['actual_context_migration_verified']=True;self.reject()
 def test_rehashed_native_pass_flag_not_a_substitute(self):self.row('shared_original_initial')['details']['run']=99;self.reject()
unittest.main(verbosity=2)
