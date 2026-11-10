"""Exact28 own212 diagnostic verification plus actual display capture validation."""
import json
from pathlib import Path
from font_display_reference import validate_metrics
from font_display_tool_profiles import once
ROOT=Path(__file__).resolve().parents[1]
_BASE=ROOT/'scripts/texture_context_application_validation.py'
text=_BASE.read_text()
text=once(text,"identity.get('versionName')=='0.22.14' and identity.get('versionCode')==221400 and manifest['version']=='0.22.14'",
          "identity.get('versionName')=='0.22.28' and identity.get('versionCode')==222800 and manifest['version']=='0.22.28'")
namespace=dict(__name__='candidate28_exact212_analytical_profile',__file__=str(_BASE))
exec(compile(text,str(_BASE)+' [exact28 identity]','exec'),namespace)
verify_tool_profile=namespace['verify_tool_profile'];canonical=namespace['canonical']

def validate_application(app,root,identity):
    result=namespace['validate_application'](app,root,identity)
    report=(Path(root)/app['report_directory']).resolve();observations=[]
    for label in namespace['ROUNDTRIPS']:
        pair=[]
        for phase in ('before','after'):
            path=report/'runtime'/('shared-'+label+'-'+phase)/'native-verification.json'
            snapshot=json.loads(path.read_text());metrics=validate_metrics(snapshot.get('graphics',{}).get('fontDisplay'))
            pair.append(metrics);observations.append(dict(label=label,phase=phase,metrics=metrics))
        if pair[0]!=pair[1]:raise ValueError('Actual display changed across fixed-display UI probe: '+label)
    result.update(actual_window_display_captures=observations,actual_display_DPI_observed=True,
                  actual_display_DPI_qualified=False)
    return result
