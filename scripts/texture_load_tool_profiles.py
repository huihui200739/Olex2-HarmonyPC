"""Exact candidate15 profiles of frozen platform/startup tools; no weakened gates."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def once(text,old,new):
    if text.count(old)!=1:
        raise ValueError('Unexpected frozen tool profile source: '+old[:110])
    return text.replace(old,new,1)

def retained_execution_source(text,baseline):
    # Persist exactly the executed source, in addition to baseline file copies.
    marker='finally:\n'
    replacement=('finally:\n'
      " (report/'executed-profile.py').write_text(_EXECUTED_PROFILE)\n"
      " result['execution_profile']=dict(baseline="+repr(baseline)+",source_file='executed-profile.py',source_sha256=hashlib.sha256(_EXECUTED_PROFILE.encode()).hexdigest(),candidate='0.22.15')\n")
    return once(text,marker,replacement)

def collection_source():
    baseline='collect-gx-texture-context-evaluation.py'
    text=(ROOT/'scripts'/baseline).read_text()
    text=once(text,'from texture_context_application_validation import validate_application',
              'from texture_load_application_validation import validate_application')
    text=once(text,"'gx-texture-context-platform-app-'","'gx-texture-load-platform-app-'")
    marker='proof=app_path.parent/\'build-provenance.json\';identity=package_identity(hap)'
    text=once(text,marker,marker+"\n require(identity==dict(bundleName='com.example.olex2harmonypc',versionName='0.22.15',versionCode=221500),'Only exact candidate15 may be collected')")
    return retained_execution_source(text,baseline)

def startup_source():
    baseline='run-texture-context-startup-audit.py'
    text=(ROOT/'scripts'/baseline).read_text()
    text=once(text,'from texture_context_startup_validation import validate_runtime_snapshot',
              'from texture_load_startup_validation import validate_runtime_snapshot')
    text=once(text,"'texture-context-startup-audit-'","'texture-load-startup-audit-'")
    text=once(text,"identity['versionName']=='0.22.14' and identity['versionCode']==221400",
              "identity['versionName']=='0.22.15' and identity['versionCode']==221500")
    text=once(text,'Only the exact persistent GX candidate14 is accepted','Only the exact original texture load candidate15 is accepted')
    return retained_execution_source(text,baseline)

def startup_verifier_source():
    text=(ROOT/'scripts/texture_context_startup_validation.py').read_text()
    text=once(text,"identity.get('versionName')!='0.22.14' or identity.get('versionCode')!=221400",
              "identity.get('versionName')!='0.22.15' or identity.get('versionCode')!=221500")
    return once(text,'Only candidate14 long-lived owner startup accepted','Only candidate15 long-lived owner startup accepted')

def execute_profile(text,baseline):
    namespace=dict(__name__='candidate15_exact_tool_profile',__file__=str(ROOT/'scripts'/baseline),_EXECUTED_PROFILE=text)
    exec(compile(text,baseline+' [exact candidate15 profile]','exec'),namespace)
