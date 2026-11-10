"""Exact candidate16 route profiles and explicit global lease validation.

The route counters must stay independent; physical context leases must instead
advance by every action. Neither value may be substituted for the other.
"""
import inspect
from pathlib import Path
from persistent_gx_window_validation import SOURCE,original_profile,replace_once,validate_lifetime
ROOT=Path(__file__).resolve().parents[1]
_BASE=ROOT/'scripts/texture_load_window_validation.py'
text=_BASE.read_text()
text=replace_once(text,"versionName='0.22.15', versionCode=221500","versionName='0.22.16', versionCode=221600")
text=replace_once(text,"'explicit candidate15 package'","'explicit candidate16 package'")
_load=dict(__name__='candidate16_exact_texture_load_profile',__file__=str(_BASE))
exec(compile(text,str(_BASE)+' [candidate16 identity profile]','exec'),_load)
IDENTITY=_load['IDENTITY']
validate_texture_load=_load['validate']

_lifetime_text=inspect.getsource(validate_lifetime)
_lifetime_text=replace_once(_lifetime_text,'def validate_lifetime(proof,pid,run,snapshots,previous=None):',
    'def validate_lifetime(proof,pid,run,snapshots,expected_lease,previous=None):')
_lifetime_text=replace_once(_lifetime_text,"proof['binding_after']==proof['binding_before']+1==run",
    "proof['binding_after']==proof['binding_before']+1==expected_lease")
_lifetime_text=replace_once(_lifetime_text,"'one balanced context lease per actual run'",
    "'one balanced global context lease per planned mixed action'")
_lifetime=dict(SOURCE=SOURCE)
exec(compile(_lifetime_text,'candidate16_explicit_global_lease_profile','exec'),_lifetime)
validate_lifetime_mixed=_lifetime['validate_lifetime']

_original=dict(__name__='candidate16_original_126_profile',__file__=str(ROOT/'scripts/original_renderer_validation.py'))
exec(compile(original_profile(),_original['__file__']+' [genuine persistent GX profile]','exec'),_original)
validate_original_window=_original['validate']

def action_plan(first):
    if first not in ('load','original'):
        raise ValueError('Unknown first route')
    other='original' if first=='load' else 'load'
    return (first,other)*3

def expected_counters(plan):
    counts={'load':0,'original':0};output=[]
    for lease,route in enumerate(plan,1):
        if route not in counts:raise ValueError('Unknown route')
        counts[route]+=1
        output.append(dict(route=route,run=counts[route],global_lease=lease))
    return output
