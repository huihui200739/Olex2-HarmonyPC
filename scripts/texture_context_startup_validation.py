"""Genuine long-lived owner startup, distinct from earlier CPU decoder cycles."""
from texture_context_application_validation import runtime_profile
from renderer_probe_identity import installed_identity
from matrix_validation import original_matrices_match
from polygon_validation import polygon_probe_matches
from line_path_validation import path_probe_matches
from original_log_validation import empty_audit_matches
_namespace={'__name__':'persistent_gx_startup_runtime_profile'}
exec(compile(runtime_profile(),'persistent_gx_startup_runtime_profile','exec'),_namespace)
def validate_runtime_snapshot(snapshot,identity):
 if not isinstance(snapshot,dict):raise ValueError('Missing actual startup snapshot')
 if identity.get('versionName')!='0.22.14' or identity.get('versionCode')!=221400:raise ValueError('Only candidate14 long-lived owner startup accepted')
 checks=[]
 def check(name,value):checks.append(dict(name=name,passed=bool(value)))
 check('exact current installed identity',snapshot.get('installedPackage')==installed_identity(identity))
 check('unique empty genuine GX runtime',_namespace['runtime_matches'](snapshot.get('applicationRuntime')))
 check('all actual utility checks',isinstance(snapshot.get('checks'),list) and snapshot.get('utilityPassed') is True and type(snapshot.get('checkCount')) is int and len(snapshot.get('checks',[]))==snapshot['checkCount'] and snapshot['checkCount']>0 and all(c.get('passed') is True for c in snapshot['checks']))
 graphics=snapshot.get('graphics')
 if not isinstance(graphics,dict):graphics={}
 check('main surface initialized and actual graphics checks',all(graphics.get(k) is True for k in ('initialized','surfaceReady','meshValidated','submissionValidated','geometryVerified','pixelVerified','coverageVerified','materialVerified','colorVerified')) and graphics.get('error')=='')
 check('actual original camera matrices',original_matrices_match(graphics))
 check('actual independent polygon probe',polygon_probe_matches(graphics))
 check('actual independent line-path probe',path_probe_matches(graphics))
 check('startup did not replay original logs',empty_audit_matches(snapshot.get('originalLog'),fresh=True))
 check('existing deferred settings save completed',isinstance(snapshot.get('workspaceState'),dict) and snapshot['workspaceState'].get('saveStatus')=='saved')
 check('full qualification flags remain false',snapshot.get('fullPortComplete') is False and snapshot.get('originalOlex2RendererTested') is False)
 return dict(passed=all(c['passed'] for c in checks),checks=checks,full_port_complete=False,functional_completion_percent=None,scope='Actual initialized empty long-lived GX owner and existing main-window startup only; not complete original application or hardware validation')
