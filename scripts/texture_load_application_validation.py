"""Same strict 212-check analytical verifier, explicitly profiled for candidate15.

This does not execute or qualify the separate new LoadTextures button. Its six
actual-window rounds retain their own source-bound raw evidence and validator.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
_BASE=ROOT/'scripts/texture_context_application_validation.py'
_TEXT=_BASE.read_text()
_OLD="identity.get('versionName')=='0.22.14' and identity.get('versionCode')==221400 and manifest['version']=='0.22.14'"
_NEW="identity.get('versionName')=='0.22.15' and identity.get('versionCode')==221500 and manifest['version']=='0.22.15'"
if _TEXT.count(_OLD)!=1:
    raise ValueError('Unexpected frozen 212-check analytical identity profile')
_NAMESPACE=dict(__name__='candidate15_frozen_212_analytical_profile',__file__=str(_BASE))
exec(compile(_TEXT.replace(_OLD,_NEW,1),str(_BASE)+' [candidate15 identity profile]','exec'),_NAMESPACE)
validate_application=_NAMESPACE['validate_application']
verify_tool_profile=_NAMESPACE['verify_tool_profile']
validate_structure=_NAMESPACE['validate_structure']
canonical=_NAMESPACE['canonical']
