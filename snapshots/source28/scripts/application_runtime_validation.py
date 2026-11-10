"""Independent fixed UI/real settings expectations, not a production parser."""
from pathlib import PurePosixPath
SOURCE='upstream/xlib/xapp.cpp: original TXApp empty startup (core metadata catalogs separately exposed); upstream/sdl/bapp.cpp: original TBasicApp directories and ExportLibrary functions + settingsfile.cpp: original settings format; platform-owned private directories and atomic SaveOptions override'
LICENSE_SHA256='ffaf87b82fdb58c4afe80e329b2d67c15d9bd792cf6097f751f67aade19fa864'
SPECS=(
 ('application_function_enable_hints','app.SetOption(ui.commandHints,true)',True,'',True),
 ('application_function_base_dir','app.BaseDir()',True,'baseDir',None),
 ('application_function_shared_dir','app.SharedDir()',True,'sharedDir',None),
 ('application_function_instance_dir','app.InstanceDir()',True,'instanceDir',None),
 ('application_function_config_dir','app.ConfigDir()',True,'configDir',None),
 ('application_function_arg_count','app.ArgCount()',True,'1',None),
 ('application_function_first_arg','app.GetArg(0)',True,'olex2-harmony',None),
 ('application_function_platform','app.Platform()',True,'OHOS64',None),
 ('application_function_hide_hints','app.SetOption(ui.commandHints,false)',True,'',False),
 ('application_function_read_option','app.OptValue(ui.commandHints)',True,'false',None),
 ('application_function_bad_option_rejected','app.SetOption(ui.commandHints,invalid)',False,None,None),
 ('application_function_unknown_rejected','app.Missing()',False,None,None),
 ('application_function_save_options','app.SaveOptions()',True,'',None),
)
SAMPLES={n:dict(sequence=i,input=text,passed=passed,text=result,hints=hints) for i,(n,text,passed,result,hints) in enumerate(SPECS,1)}
SAMPLES['application_function_restore_hints']=dict(sequence=1,input='app.SetOption(ui.commandHints,true)',passed=True,text='',hints=True)

def hex_digest(value):return isinstance(value,str) and len(value)==64 and all(c in '0123456789abcdef' for c in value)
def core_runtime_matches(s):
 try:
  return (isinstance(s,dict) and s['originalCoreInitialized'] is True and s['originalCoreEmptyFile'] is True and
          all(type(s[k]) is int and s[k]==n for k,n in (('originalCoreMacroCount',135),('originalCoreFunctionCount',38),('originalCoreFileCount',1))))
 except (KeyError,TypeError):return False

def runtime_snapshots(value,path=''):
 if isinstance(value,dict):
  for key,item in value.items():
   child=path+'/'+str(key)
   if key=='applicationRuntime':yield child,item
   yield from runtime_snapshots(item,child)
 elif isinstance(value,list):
  for index,item in enumerate(value):yield from runtime_snapshots(item,path+'/'+str(index))

def app_core_evidence_matches(app):
 try:
  checks={c['name']:c for c in app['checks']};sample=checks['original_core_runtime_empty_startup']
  snapshots=list(runtime_snapshots(app))
  return (sample['passed'] is True and sample['details']==checks['application_runtime_baseline']['details'] and
          runtime_matches(sample['details']['applicationRuntime']) and bool(snapshots) and
          all(runtime_matches(s) for _,s in snapshots))
 except (KeyError,TypeError,AttributeError):return False

def runtime_matches(s):
 try:
  if not core_runtime_matches(s) or not all(s[k] is True for k in ('initialized','licenseVerified')) or s['licenseSha256']!=LICENSE_SHA256 or s['source']!=SOURCE or s['platform']!='OHOS64' or s['error']!='':return False
  if type(s['originalFunctionCount']) is not int or s['originalFunctionCount']!=22:return False
  if not all(type(s[k]) is bool for k in ('optionsRestored','commandHints','lastSaveCommitted')):return False
  if not all(type(s[k]) is int and 0<=s[k]<=100000 for k in ('optionsCount','saveRevision')):return False
  base=s['baseDir'];suffix='/olex2-runtime-v1/base/'
  if not isinstance(base,str) or not base.startswith('/') or not base.endswith(suffix) or '/files/' not in base:return False
  root=base[:-len('/base/')]
  if PurePosixPath(root).as_posix()!=root or '..' in PurePosixPath(root).parts:return False
  for key,sub in [('baseDir','base'),('instanceDir','instance'),('sharedDir','shared'),('configDir','config')]:
   if s[key]!=root+'/'+sub+'/':return False
  return hex_digest(s['optionsSha256']) if s['optionsCount'] else s['optionsSha256']=='' or hex_digest(s['optionsSha256'])
 except (KeyError,TypeError,AttributeError):return False

def function_sample_matches(name,actual,before):
 try:
  e=SAMPLES[name];f=actual['applicationFunction'];a=actual['applicationRuntime'];b=before['applicationRuntime']
  if not runtime_matches(a) or not runtime_matches(b) or f['source']!=SOURCE:return False
  if type(before['applicationFunction']['sequence']) is not int or before['applicationFunction']['sequence']!=e['sequence']-1:return False
  for k in ('sequence','input','passed'):
   if type(f[k]) is not type(e[k]) or f[k]!=e[k]:return False
  if not isinstance(f['error'],str) or bool(f['error'])==e['passed'] or not isinstance(f['text'],str) or type(f['optionsChanged']) is not bool:return False
  if any(a[k]!=b[k] for k in ('baseDir','instanceDir','sharedDir','configDir','licenseSha256','platform','originalFunctionCount','optionsRestored','originalCoreInitialized','originalCoreEmptyFile','originalCoreMacroCount','originalCoreFunctionCount','originalCoreFileCount')):return False
  if actual['desktopCommand']!=before['desktopCommand'] or actual['graphics']!=before['graphics']:return False
  if e['passed']:
   wanted=e['text'];wanted=a[wanted].rstrip('/') if wanted in ('baseDir','instanceDir','sharedDir','configDir') else wanted
   if f['text']!=wanted:return False
  elif f['optionsChanged'] or a['optionsSha256']!=b['optionsSha256'] or a['optionsCount']!=b['optionsCount']:return False
  if e['hints'] is not None:
   if a['optionsCount'] not in (b['optionsCount'],b['optionsCount']+1):return False
   expected_change=e['hints']!=b['commandHints'] or a['optionsCount']==b['optionsCount']+1
   if a['commandHints'] is not e['hints'] or f['optionsChanged'] is not expected_change:return False
   if not a['lastSaveCommitted'] or a['saveRevision']!=b['saveRevision']+1 or not hex_digest(a['optionsSha256']):return False
   if f['optionsChanged'] and a['optionsSha256']==b['optionsSha256']:return False
  else:
   if a['commandHints'] is not b['commandHints'] or f['optionsChanged'] or a['optionsCount']!=b['optionsCount'] or a['optionsSha256']!=b['optionsSha256']:return False
   if name=='application_function_save_options':
    if not a['lastSaveCommitted'] or a['saveRevision']!=b['saveRevision']+1:return False
   elif a['saveRevision']!=b['saveRevision'] or a['optionsSha256']!=b['optionsSha256']:return False
  return True
 except (KeyError,TypeError,AttributeError):return False

def recovery_matches(app):
 try:
  before=app['application_runtime_before_restart'];after=app['application_runtime_after_restart'];b=before['applicationRuntime'];a=after['applicationRuntime']
  if not runtime_matches(a) or not runtime_matches(b) or not a['optionsRestored'] or a['commandHints'] or a['saveRevision']!=0 or a['lastSaveCommitted']:return False
  if any(a[k]!=b[k] for k in ('baseDir','instanceDir','sharedDir','configDir','optionsCount','optionsSha256','licenseSha256','originalFunctionCount','originalCoreInitialized','originalCoreEmptyFile','originalCoreMacroCount','originalCoreFunctionCount','originalCoreFileCount')):return False
  checks={c['name']:c for c in app['checks']};p=checks['application_runtime_cold_restart_new_process']['details']
  if not all(checks[k]['passed'] is True for k in ('application_runtime_options_saved','application_runtime_cold_restart_new_process','application_runtime_restored_after_restart','application_runtime_fresh_function_state')):return False
  if not all(isinstance(p[k],str) and p[k].isdigit() for k in ('prior_pid','new_pid')) or p['prior_pid']==p['new_pid']:return False
  saved=dict(sequence=13,input='app.SaveOptions()',passed=True,optionsChanged=False,text='',error='',source=SOURCE)
  if not b['lastSaveCommitted'] or b['saveRevision']<1 or not all(type(before['applicationFunction'][k]) is type(v) and before['applicationFunction'][k]==v for k,v in saved.items()):return False
  fresh=dict(sequence=0,input='',passed=False,optionsChanged=False,text='',error='',source='')
  return all(type(after['applicationFunction'][k]) is type(v) and after['applicationFunction'][k]==v for k,v in fresh.items()) and after['desktopCommand']['sequence']==0
 except (KeyError,TypeError,AttributeError):return False


def app_runtime_evidence_matches(app):
 try:
  checks={c['name']:c for c in app['checks']}
  baseline=checks['application_runtime_baseline']
  if baseline['passed'] is not True or not runtime_matches(baseline['details']['applicationRuntime']):return False
  if not recovery_matches(app):return False
  for name in SAMPLES:
   if checks[name]['passed'] is not True or not function_sample_matches(name,checks[name]['details'],app['application_function_before_samples'][name]):return False
  for name,count in [('application_runtime_hints_hidden',0),('application_runtime_hints_visible',1)]:
   if checks[name]['passed'] is not True or checks[name]['details']['visible_hint_nodes']!=count:return False
  obs=app['application_function_input_observations']
  return set(obs)==set(SAMPLES) and all(o['expected']==o['actual_committed_text']==SAMPLES[name]['input'] and type(o['temporary_typing_mode_toggle']) is bool and o['typing_mode_restore_event_sent'] is o['temporary_typing_mode_toggle'] for name,o in obs.items())
 except (KeyError,TypeError,AttributeError):return False
