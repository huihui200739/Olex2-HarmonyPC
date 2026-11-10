"""Exact candidate16 identity guard; all frozen prior 212 analytical gates remain."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
_BASE=ROOT/'scripts/texture_context_application_validation.py'
text=_BASE.read_text()
old="identity.get('versionName')=='0.22.14' and identity.get('versionCode')==221400 and manifest['version']=='0.22.14'"
new="identity.get('versionName')=='0.22.16' and identity.get('versionCode')==221600 and manifest['version']=='0.22.16'"
if text.count(old)!=1:raise ValueError('Unexpected frozen 212-check package profile')
_namespace=dict(__name__='candidate16_exact_212_analytical_profile',__file__=str(_BASE))
exec(compile(text.replace(old,new,1),str(_BASE)+' [candidate16 identity profile]','exec'),_namespace)
validate_application=_namespace['validate_application']
verify_tool_profile=_namespace['verify_tool_profile']
canonical=_namespace['canonical']
