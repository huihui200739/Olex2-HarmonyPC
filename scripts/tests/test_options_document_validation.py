"""Only isolated, explicitly synthetic picker/option report fixtures."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from options_document_validation import EMPTY,NAMES,SAMPLES,app_options_document_evidence_matches,transfer_sample_matches,recovery_matches
import test_application_runtime_validation as runtime_fixtures
class OptionsDocumentValidationTests(unittest.TestCase):
 def fixture(self):
  helper=runtime_fixtures.ApplicationRuntimeValidationTests();uri='file://docs/storage/Users/currentUser/olex2-ui-settings-012345abcdef.options';filename=uri.rsplit('/',1)[1]
  base=dict(synthetic=True,applicationRuntime=helper.runtime(),applicationFunction=dict(sequence=1,input='app.SetOption(ui.commandHints,true)',passed=True,optionsChanged=True,error=''),desktopCommand=dict(sequence=0),graphics=dict(surfaceReady=False,initialized=False),optionsDocument=copy.deepcopy(EMPTY))
  samples={};before={};prior=copy.deepcopy(base)
  for name,(op,seq) in SAMPLES.items():
   if seq==4:
    prior=copy.deepcopy(exported);prior['applicationRuntime']=helper.runtime(False);prior['applicationRuntime'].update(saveRevision=1,lastSaveCommitted=True)
    prior['applicationFunction'].update(sequence=2,input='app.SetOption(ui.commandHints,false)');hidden=copy.deepcopy(prior)
   before[op+'-'+str(seq)]=copy.deepcopy(prior);a=copy.deepcopy(prior)
   a['optionsDocument']=dict(sequence=seq,operation=op,state='cancelled' if seq<3 else 'passed',selectedUri='' if seq<3 else uri,byteCount=0 if seq<3 else 21,sha256='' if seq<3 else helper.runtime()['optionsSha256'],importedOptions=1 if seq==4 else 0,optionsChanged=seq==4,destinationMayBePartial=False,error='')
   if seq==4:a['applicationRuntime']=helper.runtime();a['applicationRuntime'].update(saveRevision=2,lastSaveCommitted=True)
   samples[name]=copy.deepcopy(a);prior=copy.deepcopy(a)
   if seq==3:exported=copy.deepcopy(a)
  imported=copy.deepcopy(prior);after=copy.deepcopy(base);after['applicationRuntime'].update(optionsRestored=True);after['applicationFunction'].update(sequence=0,input='',passed=False,optionsChanged=False)
  details={**samples,'options_document_baseline':base,'options_document_saved_ui':dict(visible_status='设置已导出，文件字节已核对'),'options_document_prepare_import_hints_hidden':hidden,'options_document_import_restores_visible_setting':dict(visible_hint_nodes=1,visible_status='已合并 1 个设置'),'options_document_cold_restart_new_process':dict(prior_pid='123',new_pid='456'),'options_document_import_restored_after_restart':after,'options_document_fresh_transfer_state':after}
  observation=lambda text:dict(expected=text,actual_committed_text=text,temporary_typing_mode_toggle=False,typing_mode_restore_event_sent=False)
  return dict(synthetic=True,checks=[dict(name=n,passed=True,details=copy.deepcopy(details[n])) for n in NAMES],options_document_before_samples=before,options_document_exported=exported,options_document_imported=imported,options_document_before_import=hidden,options_document_before_restart=imported,options_document_after_restart=after,options_document_filename_observation=observation(filename),options_document_function_input_observations={'options-document-hide-hints':observation('app.SetOption(ui.commandHints,false)')})
 def test_complete_isolated_synthetic_fixture(self):self.assertTrue(app_options_document_evidence_matches(self.fixture()))
 def mutate(self,edit):
  r=self.fixture();edit(r);self.assertFalse(app_options_document_evidence_matches(r))
 def test_missing_check_rejected(self):self.mutate(lambda r:r['checks'].pop())
 def test_missing_filename_observation_rejected(self):self.mutate(lambda r:r.pop('options_document_filename_observation'))
 def test_wrong_committed_filename_rejected(self):self.mutate(lambda r:r['options_document_filename_observation'].update(actual_committed_text='other.options'))
 def test_unrestored_temporary_typing_event_rejected(self):self.mutate(lambda r:r['options_document_filename_observation'].update(temporary_typing_mode_toggle=True))
 def test_wrong_import_hash_rejected(self):self.mutate(lambda r:r['options_document_imported']['optionsDocument'].update(sha256='0'*64))
 def test_false_visible_setting_rejected(self):self.mutate(lambda r:next(c['details'] for c in r['checks'] if c['name']=='options_document_import_restores_visible_setting').update(visible_hint_nodes=0))
 def test_same_pid_rejected(self):self.mutate(lambda r:next(c['details'] for c in r['checks'] if c['name']=='options_document_cold_restart_new_process').update(new_pid='123'))
 def test_replayed_document_operation_rejected(self):self.mutate(lambda r:r['options_document_after_restart']['optionsDocument'].update(sequence=4))
 def test_replayed_function_rejected(self):self.mutate(lambda r:r['options_document_after_restart']['applicationFunction'].update(sequence=2))
 def test_boolean_fresh_sequence_is_not_a_number(self):self.mutate(lambda r:r['options_document_after_restart']['optionsDocument'].update(sequence=False))
 def test_known_default_bytes_require_exact_length(self):
  r=self.fixture();n='options_document_exported_snapshot';a=next(c['details'] for c in r['checks'] if c['name']==n);a['optionsDocument']['byteCount']=20;self.assertFalse(transfer_sample_matches(n,a,r['options_document_before_samples']['export-3'],r['options_document_exported']))
 def test_changed_saved_hash_rejected(self):self.mutate(lambda r:r['options_document_after_restart']['applicationRuntime'].update(optionsSha256='0'*64))
 def test_native_flag_without_visible_control_rejected(self):self.mutate(lambda r:next(c['details'] for c in r['checks'] if c['name']=='options_document_saved_ui').update(visible_status='not verified'))
 def test_each_transfer_has_fixed_sequence_and_state(self):
  r=self.fixture();exported=r['options_document_exported']
  for name,(op,seq) in SAMPLES.items():
   actual=copy.deepcopy(next(c['details'] for c in r['checks'] if c['name']==name));before=r['options_document_before_samples'][op+'-'+str(seq)]
   self.assertTrue(transfer_sample_matches(name,actual,before,exported),name);actual['optionsDocument']['sequence']=999;self.assertFalse(transfer_sample_matches(name,actual,before,exported),name)
 def test_cancel_cannot_claim_a_file_write(self):
  r=self.fixture();n='options_document_export_cancelled';a=next(c['details'] for c in r['checks'] if c['name']==n);a['optionsDocument']['byteCount']=21;self.assertFalse(transfer_sample_matches(n,a,r['options_document_before_samples']['export-1'],r['options_document_exported']))
 def test_import_cannot_change_function_history(self):
  r=self.fixture();n='options_document_imported_snapshot';a=next(c['details'] for c in r['checks'] if c['name']==n);a['applicationFunction']['sequence']+=1;self.assertFalse(transfer_sample_matches(n,a,r['options_document_before_samples']['import-4'],r['options_document_exported']))
 def test_inactive_page_cannot_claim_active_surface(self):
  r=self.fixture();n='options_document_exported_snapshot';a=next(c['details'] for c in r['checks'] if c['name']==n);a['graphics']['initialized']=True;self.assertFalse(transfer_sample_matches(n,a,r['options_document_before_samples']['export-3'],r['options_document_exported']))
 def test_malformed_reports_rejected(self):
  for r in [None,{},[],dict(checks=[])]:self.assertFalse(app_options_document_evidence_matches(r));self.assertFalse(recovery_matches(r))
if __name__=='__main__':unittest.main()
