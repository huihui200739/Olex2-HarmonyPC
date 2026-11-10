"""Exact28 UI/resource/startup profiles, retaining every old gate.

Only candidate binding, explicit target/installation guards, retained recipes,
and the additional measured-display check differ from the frozen14 recipes.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
IDENTITY=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.28',versionCode=222800)
PROFILE_DEPENDENCIES=('font_display_tool_profiles.py','font_display_application_validation.py',
    'font_display_startup_validation.py','font_display_reference.py',
    'texture_context_application_validation.py','texture_context_startup_validation.py',
    'hdc_install_validation.py')

def once(text,old,new):
    if text.count(old)!=1:raise ValueError('Unexpected frozen28 tool source: '+old[:110])
    return text.replace(old,new,1)

def retained_execution_source(text,baseline):
    replacement=('finally:\n'
        " (report/'executed-profile.py').write_text(_EXECUTED_PROFILE)\n"
        " result['execution_profile']=dict(baseline="+repr(baseline)+",source_file='executed-profile.py',source_sha256=hashlib.sha256(_EXECUTED_PROFILE.encode()).hexdigest(),candidate='0.22.28')\n"
        " result['profile_dependencies']={}\n")
    for name in PROFILE_DEPENDENCIES:
        replacement+=(" (report/"+repr(name)+").write_bytes((ROOT/'scripts'/"+repr(name)+").read_bytes())\n"
            " result['profile_dependencies']["+repr(name)+"]=hashlib.sha256((report/"+repr(name)+").read_bytes()).hexdigest()\n")
    return once(text,'finally:\n',replacement)

def collection_source():
    baseline='collect-gx-texture-context-evaluation.py';text=(ROOT/'scripts'/baseline).read_text()
    text=once(text,'from texture_context_application_validation import validate_application',
              'from font_display_application_validation import validate_application')
    text=once(text,"'gx-texture-context-platform-app-'","'gx-font-display-platform-app-'")
    marker="proof=app_path.parent/'build-provenance.json';identity=package_identity(hap)"
    text=once(text,marker,marker+"\n require(identity=="+repr(IDENTITY)+",'Only exact candidate28 may be collected')")
    text=once(text,'a=p.parse_args();','a=p.parse_args();\nif a.target!=\'127.0.0.1:5555\':raise ValueError(\'Only original existing emulator accepted\')')
    return retained_execution_source(text,baseline)

def startup_source():
    baseline='run-texture-context-startup-audit.py';text=(ROOT/'scripts'/baseline).read_text()
    text=once(text,'from texture_context_startup_validation import validate_runtime_snapshot',
              'from font_display_startup_validation import validate_runtime_snapshot\nfrom hdc_install_validation import validate_install_output')
    text=once(text,"'texture-context-startup-audit-'","'font-display-startup-audit-'")
    text=once(text,"identity['versionName']=='0.22.14' and identity['versionCode']==221400",
              "identity['versionName']=='0.22.28' and identity['versionCode']==222800")
    text=once(text,'Only the exact persistent GX candidate14 is accepted','Only the exact display font candidate28 is accepted')
    text=once(text,'a=p.parse_args()','a=p.parse_args()\nif a.target!=\'127.0.0.1:5555\':raise ValueError(\'Only original existing emulator accepted\')')
    text=once(text,' output=d.stdout+d.stderr\n require(d.returncode',
              " output=d.stdout+d.stderr\n if args and args[0]=='install':validate_install_output(output,d.returncode)\n require(d.returncode")
    return retained_execution_source(text,baseline)

def startup_verifier_source():
    text=(ROOT/'scripts/texture_context_startup_validation.py').read_text()
    text=once(text,"identity.get('versionName')!='0.22.14' or identity.get('versionCode')!=221400",
              "identity.get('versionName')!='0.22.28' or identity.get('versionCode')!=222800")
    return once(text,'Only candidate14 long-lived owner startup accepted','Only candidate28 long-lived owner startup accepted')

def ui_preparation_source():
    text=(ROOT/'scripts/prepare-texture-context-ui-tools.py').read_text()
    text=once(text,"identity['versionName']!='0.22.14' or identity['versionCode']!=221400",
              "identity['versionName']!='0.22.28' or identity['versionCode']!=222800")
    text=once(text,"source/'texture-context-inputs.json'","source/'font-display-telemetry-inputs.json'")
    return once(text,"manifest=dict(version='0.22.14'","manifest=dict(version='0.22.28'")

def strict_ui_install_source(text):
    return once(text,"    install = hdc('install', str(retained_hap))",
                "    from hdc_install_validation import validate_install_output\n    install = hdc('install', str(retained_hap))\n    validate_install_output(install)")

def execute_profile(text,baseline):
    namespace=dict(__name__='candidate28_exact_tool_profile',__file__=str(ROOT/'scripts'/baseline),_EXECUTED_PROFILE=text)
    exec(compile(text,baseline+' [exact candidate28 profile]','exec'),namespace)
