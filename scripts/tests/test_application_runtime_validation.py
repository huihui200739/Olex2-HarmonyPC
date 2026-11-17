"""Only isolated, explicitly synthetic runtime/function report fixtures."""
import copy,hashlib,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from application_runtime_validation import SOURCE,LICENSE_SHA256,SAMPLES,app_core_evidence_matches,runtime_snapshots,core_runtime_matches,runtime_matches,function_sample_matches,recovery_matches
class ApplicationRuntimeValidationTests(unittest.TestCase):
 def runtime(self,hints=True):
  root='/data/storage/el2/base/haps/entry/files/olex2-runtime-v1/'
  return dict(originalCoreInitialized=True,originalCoreEmptyFile=True,originalCoreMacroCount=135,originalCoreFunctionCount=38,originalCoreFileCount=1,initialized=True,optionsRestored=False,licenseVerified=True,commandHints=hints,lastSaveCommitted=False,originalFunctionCount=22,optionsCount=1,saveRevision=0,baseDir=root+'base/',instanceDir=root+'instance/',sharedDir=root+'shared/',configDir=root+'config/',licenseSha256=LICENSE_SHA256,optionsSha256=hashlib.sha256(('ui.commandHints='+str(hints).lower()+'\n').encode()).hexdigest(),platform='OHOS64',source=SOURCE,error='')
 def sample(self,name):
  e=SAMPLES[name];before=dict(synthetic=True,applicationFunction=dict(sequence=e['sequence']-1),applicationRuntime=self.runtime(e['hints'] is not True),graphics=dict(synthetic=True,yaw=32),desktopCommand=dict(sequence=0))
  a=copy.deepcopy(before);r=a['applicationRuntime'];wanted=e['text'];text=r[wanted].rstrip('/') if wanted in ('baseDir','instanceDir','sharedDir','configDir') else wanted
  a['applicationFunction']=dict(sequence=e['sequence'],input=e['input'],passed=e['passed'],optionsChanged=False,text=text if e['passed'] else '',error='' if e['passed'] else 'isolated synthetic rejected expression',source=SOURCE)
  if e['hints'] is not None:
   r['commandHints']=e['hints'];r['lastSaveCommitted']=True;r['saveRevision']=1;r['optionsSha256']=self.runtime(e['hints'])['optionsSha256'];a['applicationFunction']['optionsChanged']=e['hints']!=before['applicationRuntime']['commandHints']
  elif name=='application_function_save_options':r['lastSaveCommitted']=True;r['saveRevision']=1
  return a,before
 def test_all_fixed_samples(self):
  for name in SAMPLES:
   a,b=self.sample(name);self.assertTrue(function_sample_matches(name,a,b),name)
 def test_first_stored_default_option_is_a_real_map_change(self):
  n='application_function_enable_hints';a,b=self.sample(n);b['applicationRuntime'].update(commandHints=True,optionsCount=0,optionsSha256='');a['applicationFunction']['optionsChanged']=True;self.assertTrue(function_sample_matches(n,a,b))
 def test_before_sequence_must_match_actual_predecessor(self):
  n='application_function_config_dir';a,b=self.sample(n);b['applicationFunction']['sequence']=999;self.assertFalse(function_sample_matches(n,a,b))
 def test_runtime_registry_count_not_a_boolean(self):
  r=self.runtime();r['originalFunctionCount']=True;self.assertFalse(runtime_matches(r))
 def test_runtime_source_must_be_exact(self):
  r=self.runtime();r['source']='different synthetic source';self.assertFalse(runtime_matches(r))
 def test_bad_resource_hash_rejected(self):
  r=self.runtime();r['licenseSha256']='0'*64;self.assertFalse(runtime_matches(r))
 def test_unknown_resource_verification_rejected(self):
  r=self.runtime();r['licenseVerified']=False;self.assertFalse(runtime_matches(r))
 def test_directory_mapping_must_share_private_root(self):
  r=self.runtime();r['configDir']='/tmp/unrelated/';self.assertFalse(runtime_matches(r))
 def test_native_desktop_platform_label_not_ohos_evidence(self):
  r=self.runtime();r['platform']='HOST-COMPATIBILITY-TEST';self.assertFalse(runtime_matches(r))
 def test_literal_input_must_match(self):
  n='application_function_config_dir';a,b=self.sample(n);a['applicationFunction']['input']='app.ConfigDir';self.assertFalse(function_sample_matches(n,a,b))
 def test_cpu_flag_cannot_replace_visible_setting(self):
  n='application_function_hide_hints';a,b=self.sample(n);a['applicationRuntime']['commandHints']=True;self.assertFalse(function_sample_matches(n,a,b))
 def test_set_option_requires_real_file_hash_change(self):
  n='application_function_hide_hints';a,b=self.sample(n);a['applicationRuntime']['optionsSha256']=b['applicationRuntime']['optionsSha256'];self.assertFalse(function_sample_matches(n,a,b))
 def test_save_requires_actual_commit_revision(self):
  n='application_function_save_options';a,b=self.sample(n);a['applicationRuntime']['saveRevision']=0;self.assertFalse(function_sample_matches(n,a,b))
 def test_failed_option_must_keep_saved_bytes(self):
  n='application_function_bad_option_rejected';a,b=self.sample(n);a['applicationRuntime']['optionsSha256']='0'*64;self.assertFalse(function_sample_matches(n,a,b))
 def test_application_function_must_not_change_view_command_state(self):
  n='application_function_base_dir';a,b=self.sample(n);a['desktopCommand']['sequence']=1;self.assertFalse(function_sample_matches(n,a,b))
 def test_application_function_must_not_change_graphics(self):
  n='application_function_base_dir';a,b=self.sample(n);a['graphics']['yaw']=99;self.assertFalse(function_sample_matches(n,a,b))
 def recovery(self):
  before,_=self.sample('application_function_save_options');before['applicationRuntime']['commandHints']=False
  after=copy.deepcopy(before);after['applicationRuntime'].update(optionsRestored=True,saveRevision=0,lastSaveCommitted=False)
  after['applicationFunction']=dict(sequence=0,input='',passed=False,optionsChanged=False,text='',error='',source='')
  checks=[dict(name=n,passed=True,details={}) for n in ('application_runtime_options_saved','application_runtime_cold_restart_new_process','application_runtime_restored_after_restart','application_runtime_fresh_function_state')]
  checks[1]['details']=dict(prior_pid='123',new_pid='456')
  return dict(synthetic=True,application_runtime_before_restart=before,application_runtime_after_restart=after,checks=checks)
 def test_isolated_synthetic_recovery(self):self.assertTrue(recovery_matches(self.recovery()))
 def test_replayed_function_after_restart_rejected(self):
  r=self.recovery();r['application_runtime_after_restart']['applicationFunction']['sequence']=13;self.assertFalse(recovery_matches(r))
 def test_saved_hash_must_survive_process_change(self):
  r=self.recovery();r['application_runtime_after_restart']['applicationRuntime']['optionsSha256']='0'*64;self.assertFalse(recovery_matches(r))
 def test_same_pid_rejected(self):
  r=self.recovery();r['checks'][1]['details']['new_pid']='123';self.assertFalse(recovery_matches(r))
 def test_missing_or_malformed_reports(self):
  for value in [None,{},[],dict(applicationRuntime={})]:
   self.assertFalse(runtime_matches(value));self.assertFalse(recovery_matches(value));self.assertFalse(function_sample_matches('application_function_base_dir',value,{}))

 def test_core_non_dummy_empty_startup_required(self):
  self.assertTrue(core_runtime_matches(self.runtime()))
  r=self.runtime();r['originalCoreInitialized']=False;self.assertFalse(runtime_matches(r))
 def test_core_empty_file_required(self):
  r=self.runtime();r['originalCoreEmptyFile']=False;self.assertFalse(runtime_matches(r))
 def test_core_macro_registry_exact(self):
  r=self.runtime();r['originalCoreMacroCount']=134;self.assertFalse(runtime_matches(r))
 def test_core_function_registry_exact(self):
  r=self.runtime();r['originalCoreFunctionCount']=39;self.assertFalse(runtime_matches(r))
 def test_core_one_file_container_required(self):
  r=self.runtime();r['originalCoreFileCount']=2;self.assertFalse(runtime_matches(r))
 def test_core_missing_fields_rejected(self):
  for key in ['originalCoreInitialized','originalCoreEmptyFile','originalCoreMacroCount','originalCoreFunctionCount','originalCoreFileCount']:
   with self.subTest(key=key):
    r=self.runtime();del r[key];self.assertFalse(runtime_matches(r))
 def test_core_bool_or_float_registry_count_rejected(self):
  for key in ['originalCoreMacroCount','originalCoreFunctionCount','originalCoreFileCount']:
   for value in [True,False,1.0,135.0,38.0]:
    with self.subTest(key=key,value=value):
     r=self.runtime();r[key]=value;self.assertFalse(runtime_matches(r))
 def test_core_string_flags_rejected(self):
  for key in ['originalCoreInitialized','originalCoreEmptyFile']:
   r=self.runtime();r[key]='true';self.assertFalse(runtime_matches(r))

 def core_app(self):
  baseline=dict(applicationRuntime=self.runtime(),applicationFunction=dict(sequence=0))
  return dict(synthetic=True,checks=[dict(name='application_runtime_baseline',passed=True,details=copy.deepcopy(baseline)),dict(name='original_core_runtime_empty_startup',passed=True,details=copy.deepcopy(baseline))],extra=[dict(nested=dict(applicationRuntime=self.runtime()))])
 def test_core_app_all_nested_snapshots_observed(self):
  a=self.core_app();self.assertEqual(len(list(runtime_snapshots(a))),3);self.assertTrue(app_core_evidence_matches(a))
 def test_core_app_nested_partial_startup_rejected(self):
  a=self.core_app();a['extra'][0]['nested']['applicationRuntime']['originalCoreInitialized']=False;self.assertFalse(app_core_evidence_matches(a))
 def test_core_app_nested_wrong_registry_rejected(self):
  a=self.core_app();a['extra'][0]['nested']['applicationRuntime']['originalCoreMacroCount']=136;self.assertFalse(app_core_evidence_matches(a))
 def test_core_app_missing_startup_check_rejected(self):
  a=self.core_app();a['checks'].pop();self.assertFalse(app_core_evidence_matches(a))
 def test_core_app_failed_startup_check_rejected(self):
  a=self.core_app();a['checks'][1]['passed']=False;self.assertFalse(app_core_evidence_matches(a))
 def test_core_app_mismatched_original_settings_source_rejected(self):
  a=self.core_app();a['extra'][0]['nested']['applicationRuntime']['source']='isolated synthetic unsupported source';self.assertFalse(app_core_evidence_matches(a))

if __name__=='__main__':unittest.main()
