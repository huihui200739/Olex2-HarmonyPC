"""Exact19 guard; original212 UI and all independent analytical gates retained."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];_BASE=ROOT/'scripts/texture_context_application_validation.py'
text=_BASE.read_text();old="identity.get('versionName')=='0.22.14' and identity.get('versionCode')==221400 and manifest['version']=='0.22.14'";new="identity.get('versionName')=='0.22.20' and identity.get('versionCode')==222000 and manifest['version']=='0.22.20'"
if text.count(old)!=1:raise ValueError('Unexpected frozen212 identity guard')
namespace=dict(__name__='candidate20_exact212_analytical_profile',__file__=str(_BASE))
exec(compile(text.replace(old,new,1),str(_BASE)+' [exact19 identity]','exec'),namespace)
validate_application=namespace['validate_application'];verify_tool_profile=namespace['verify_tool_profile'];canonical=namespace['canonical']
