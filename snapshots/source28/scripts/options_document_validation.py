"""Fixed system-picker/settings expectations; never a production file parser."""
import re,hashlib
from application_runtime_validation import runtime_matches, hex_digest
NAMES=('options_document_baseline','options_document_export_cancelled','options_document_import_cancelled',
 'options_document_exported_snapshot','options_document_saved_ui','options_document_prepare_import_hints_hidden',
 'options_document_imported_snapshot','options_document_import_restores_visible_setting',
 'options_document_cold_restart_new_process','options_document_import_restored_after_restart','options_document_fresh_transfer_state')
SAMPLES={'options_document_export_cancelled':('export',1),'options_document_import_cancelled':('import',2),
 'options_document_exported_snapshot':('export',3),'options_document_imported_snapshot':('import',4)}
EMPTY=dict(sequence=0,operation='',state='idle',selectedUri='',byteCount=0,sha256='',importedOptions=0,optionsChanged=False,destinationMayBePartial=False,error='')
DEFAULT_SETTINGS=b'ui.commandHints=true\n'
DEFAULT_SETTINGS_SHA256=hashlib.sha256(DEFAULT_SETTINGS).hexdigest()
def empty_matches(value):
 try:return all(type(value[key]) is type(expected) and value[key]==expected for key,expected in EMPTY.items())
 except (KeyError,TypeError,AttributeError):return False
def same_runtime(a,b):
 return all(a[k]==b[k] for k in ('optionsSha256','optionsCount','saveRevision','commandHints','lastSaveCommitted','optionsRestored','baseDir','sharedDir','instanceDir','configDir','licenseSha256','originalFunctionCount'))
def transfer_sample_matches(name,actual,before,exported):
 try:
  op,sequence=SAMPLES[name];d=actual['optionsDocument'];b=before['applicationRuntime'];a=actual['applicationRuntime']
  if not runtime_matches(a) or not runtime_matches(b):return False
  if not all(type(d[k]) is t for k,t in [('sequence',int),('byteCount',int),('importedOptions',int),('optionsChanged',bool),('destinationMayBePartial',bool)]):return False
  if d['sequence']!=sequence or d['operation']!=op or d['destinationMayBePartial'] or d['error']!='':return False
  if actual['applicationFunction']!=before['applicationFunction'] or actual['desktopCommand']!=before['desktopCommand']:return False
  if before['optionsDocument']['sequence']!=sequence-1:return False
  if actual['graphics']['surfaceReady'] is not False or actual['graphics']['initialized'] is not False:return False
  if sequence<=2:
   return d['state']=='cancelled' and d['selectedUri']=='' and d['sha256']=='' and d['byteCount']==0 and d['importedOptions']==0 and not d['optionsChanged'] and same_runtime(a,b)
  if d['state']!='passed' or not hex_digest(d['sha256']) or not 0<d['byteCount']<=1048576:return False
  if d['sha256']==DEFAULT_SETTINGS_SHA256 and d['byteCount']!=len(DEFAULT_SETTINGS):return False
  if not re.fullmatch(r'file://docs/storage/Users/currentUser/olex2-ui-settings-[0-9a-f]{12}\.options',d['selectedUri']):return False
  if sequence==3:return d['sha256']==b['optionsSha256'] and d['importedOptions']==0 and not d['optionsChanged'] and same_runtime(a,b) and a['commandHints'] is True
  e=exported['optionsDocument'];r=exported['applicationRuntime']
  return all(d[k]==e[k] for k in ('sha256','byteCount','selectedUri')) and d['optionsChanged'] and d['importedOptions']==r['optionsCount'] and a['optionsCount']==b['optionsCount']==r['optionsCount'] and b['commandHints'] is False and a['commandHints'] is True and a['lastSaveCommitted'] is True and a['saveRevision']==b['saveRevision']+1 and a['optionsSha256']==e['sha256'] and a['optionsSha256']!=b['optionsSha256']
 except (KeyError,TypeError,AttributeError,ValueError):return False

def recovery_matches(app):
 try:
  b=app['options_document_before_restart'];a=app['options_document_after_restart'];rt=a['applicationRuntime'];old=b['applicationRuntime'];checks={c['name']:c for c in app['checks']};p=checks['options_document_cold_restart_new_process']['details']
  if not runtime_matches(rt) or not runtime_matches(old):return False
  if not all(isinstance(p[k],str) and p[k].isdigit() for k in ('prior_pid','new_pid')) or p['prior_pid']==p['new_pid']:return False
  if not rt['optionsRestored'] or not rt['commandHints'] or rt['saveRevision']!=0 or rt['lastSaveCommitted']:return False
  if any(rt[k]!=old[k] for k in ('optionsSha256','optionsCount','baseDir','configDir','licenseSha256','originalFunctionCount')):return False
  if rt['optionsSha256']!=app['options_document_exported']['optionsDocument']['sha256'] or b!=app['options_document_imported']:return False
  if not empty_matches(a['optionsDocument']) or a['applicationFunction']['sequence']!=0 or a['applicationFunction']['input']!='' or a['desktopCommand']['sequence']!=0:return False
  return all(checks[name]['passed'] is True for name in NAMES[-3:])
 except (KeyError,TypeError,AttributeError,ValueError):return False

def app_options_document_evidence_matches(app):
 try:
  checks={c['name']:c for c in app['checks']}
  if not all(checks[name]['passed'] is True for name in NAMES):return False
  b=checks['options_document_baseline']['details']
  if not runtime_matches(b['applicationRuntime']) or not empty_matches(b['optionsDocument']) or not b['applicationRuntime']['commandHints']:return False
  exported=app['options_document_exported'];imported=app['options_document_imported']
  for name,(op,seq) in SAMPLES.items():
   if not transfer_sample_matches(name,checks[name]['details'],app['options_document_before_samples'][op+'-'+str(seq)],exported):return False
  if checks['options_document_exported_snapshot']['details']!=exported or checks['options_document_imported_snapshot']['details']!=imported:return False
  obs=app['options_document_filename_observation'];filename=exported['optionsDocument']['selectedUri'].rsplit('/',1)[1]
  if obs['expected']!=filename or obs['actual_committed_text']!=filename or type(obs['temporary_typing_mode_toggle']) is not bool or obs['typing_mode_restore_event_sent'] is not obs['temporary_typing_mode_toggle']:return False
  functions=app['options_document_function_input_observations'];f=functions['options-document-hide-hints'];text='app.SetOption(ui.commandHints,false)'
  if set(functions)!={'options-document-hide-hints'} or f['expected']!=text or f['actual_committed_text']!=text or type(f['temporary_typing_mode_toggle']) is not bool or f['typing_mode_restore_event_sent'] is not f['temporary_typing_mode_toggle']:return False
  hidden=app['options_document_before_import'];hf=hidden['applicationFunction'];hr=hidden['applicationRuntime'];er=exported['applicationRuntime']
  if hf['sequence']!=exported['applicationFunction']['sequence']+1 or hf['input']!=text or hf['passed'] is not True or not hf['optionsChanged'] or hf['error']!='' or hr['commandHints'] or hr['saveRevision']!=er['saveRevision']+1 or hr['optionsSha256']==er['optionsSha256']:return False
  if checks['options_document_prepare_import_hints_hidden']['details']!=hidden:return False
  if checks['options_document_saved_ui']['details']['visible_status']!='设置已导出，文件字节已核对':return False
  visible=checks['options_document_import_restores_visible_setting']['details']
  if visible['visible_hint_nodes']!=1 or visible['visible_status']!=f"已合并 {imported['optionsDocument']['importedOptions']} 个设置":return False
  return recovery_matches(app)
 except (KeyError,TypeError,AttributeError,ValueError):return False
