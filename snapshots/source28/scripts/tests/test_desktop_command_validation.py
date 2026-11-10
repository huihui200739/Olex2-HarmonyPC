"""Clearly labeled isolated synthetic reports, never real runtime evidence."""
import copy
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from desktop_command_validation import SOURCE,SAMPLES,command_sample_matches,expected_view_matches,command_recovery_matches
class DesktopCommandValidationTests(unittest.TestCase):
 def fixture(self,name):
  e=copy.deepcopy(SAMPLES[name]);graphics=e.pop('graphics');passed=e['passed']
  c=dict(e,source=SOURCE,applied=e['changed'],message=('not the complete Olex2 command library' if name.endswith('help') else 'accepted') if passed else '',error='' if passed else 'synthetic validation error')
  return dict(synthetic=True,desktopCommand=c,graphics=graphics)
 def test_all_fixed_independent_expectations(self):
  for name in SAMPLES:self.assertTrue(command_sample_matches(name,self.fixture(name)),name)
 def test_parser_flag_without_actual_view_change_rejected(self):
  name='desktop_command_mesh';r=self.fixture(name);r['graphics']['meshKind']='sphere';self.assertFalse(command_sample_matches(name,r))
 def test_wrong_source_rejected(self):
  name='desktop_command_view';r=self.fixture(name);r['desktopCommand']['source']='synthetic different engine';self.assertFalse(command_sample_matches(name,r))
 def test_uncommitted_input_rejected(self):
  name='desktop_command_help';r=self.fixture(name);r['desktopCommand']['input']='pc。';self.assertFalse(command_sample_matches(name,r))
 def test_cpu_intent_cannot_replace_actual_fields(self):
  name='desktop_command_view';r=self.fixture(name);r['graphics']['yaw']=32;r['requestedState']={'yaw':41};self.assertFalse(command_sample_matches(name,r))
 def test_failed_chain_must_not_change_view(self):
  name='desktop_command_failed_chain_unchanged';r=self.fixture(name);r['graphics']['yaw']=32;self.assertFalse(command_sample_matches(name,r))
 def test_failed_chain_must_not_be_applied(self):
  name='desktop_command_failed_chain_unchanged';r=self.fixture(name);r['desktopCommand']['applied']=True;self.assertFalse(command_sample_matches(name,r))
 def test_callback_count_not_invented(self):
  name='desktop_command_chain';r=self.fixture(name);r['desktopCommand']['dispatched']=1;self.assertFalse(command_sample_matches(name,r))
 def test_sequence_must_be_real_integer(self):
  name='desktop_command_help';r=self.fixture(name);r['desktopCommand']['sequence']=True;self.assertFalse(command_sample_matches(name,r))
 def test_boolean_graphics_not_numeric_substitutes(self):
  name='desktop_command_wire';r=self.fixture(name);r['graphics']['wireframe']=1;self.assertFalse(command_sample_matches(name,r))
 def test_nonfinite_view_rejected(self):
  for value in [float('nan'),float('inf'),True]:
   r=self.fixture('desktop_command_view');r['graphics']['yaw']=value;self.assertFalse(command_sample_matches('desktop_command_view',r))
 def test_failure_without_actual_error_rejected(self):
  name='desktop_command_unknown_rejected';r=self.fixture(name);r['desktopCommand']['error']='';self.assertFalse(command_sample_matches(name,r))
 def test_help_scope_must_remain_explicit(self):
  name='desktop_command_help';r=self.fixture(name);r['desktopCommand']['message']='complete Olex2 commands';self.assertFalse(command_sample_matches(name,r))
 def test_missing_or_unknown_report_rejected(self):
  self.assertFalse(command_sample_matches('desktop_command_help',{}));self.assertFalse(command_sample_matches('synthetic unknown',self.fixture('desktop_command_help')))
 def recovery_fixture(self):
  before=self.fixture('desktop_command_enter_submit');before['graphics']['materialCanonical']='isolated synthetic material';before['workspaceState']=dict(saveStatus='saved',schemaVersion=4)
  after=copy.deepcopy(before);after['workspaceState']=dict(restored=True)
  after['desktopCommand']=dict(sequence=0,input='',passed=False,changed=False,applied=False,dispatched=0,source='',message='',error='')
  names=('desktop_command_surface_recreated','desktop_command_survives_surface_recreation','desktop_command_state_saved','desktop_command_cold_restart_new_process','desktop_command_restored_after_restart')
  checks=[dict(name=name,passed=True,details=copy.deepcopy(before) if name=='desktop_command_survives_surface_recreation' else {}) for name in names]
  next(c for c in checks if c['name']=='desktop_command_cold_restart_new_process')['details']=dict(prior_pid='123',new_pid='456')
  return dict(synthetic=True,checks=checks,desktop_command_state_before_restart=before,desktop_command_state_after_restart=after)
 def test_isolated_synthetic_recovery(self):
  self.assertTrue(command_recovery_matches(self.recovery_fixture()))
 def test_replayed_command_sequence_after_restart_rejected(self):
  r=self.recovery_fixture();r['desktop_command_state_after_restart']['desktopCommand']['sequence']=13;self.assertFalse(command_recovery_matches(r))
 def test_fake_fresh_command_success_rejected(self):
  for key,value in [('passed',True),('changed',True),('dispatched',False),('message','replayed')]:
   r=self.recovery_fixture();r['desktop_command_state_after_restart']['desktopCommand'][key]=value;self.assertFalse(command_recovery_matches(r))
 def test_wrong_surface_recovery_view_rejected(self):
  r=self.recovery_fixture();next(c for c in r['checks'] if c['name']=='desktop_command_survives_surface_recreation')['details']['graphics']['wireframe']=False;self.assertFalse(command_recovery_matches(r))
 def test_same_process_restart_rejected(self):
  r=self.recovery_fixture();next(c for c in r['checks'] if c['name']=='desktop_command_cold_restart_new_process')['details']['new_pid']='123';self.assertFalse(command_recovery_matches(r))
 def test_incomplete_recovery_report_rejected(self):
  for r in [{},None,dict(checks=[]),dict(checks=[{}])]:self.assertFalse(command_recovery_matches(r))
if __name__=='__main__':unittest.main()
