"""Exact28 same ten owner startup gates plus the actual main-window display."""
from font_display_tool_profiles import startup_verifier_source
from font_display_reference import validate_metrics
_namespace=dict(__name__='candidate28_exact_initialized_startup_verifier')
exec(compile(startup_verifier_source(),'candidate28_exact_initialized_startup_verifier','exec'),_namespace)

def validate_runtime_snapshot(snapshot,identity):
    result=_namespace['validate_runtime_snapshot'](snapshot,identity)
    try:
        metrics=validate_metrics(snapshot.get('graphics',{}).get('fontDisplay'))
        result['display_metrics']=metrics;passed=True
    except (ValueError,AttributeError):passed=False
    result['checks'].append(dict(name='actual main-window display is ready without a fixed-DPI fallback',passed=passed))
    result['passed']=all(row['passed'] for row in result['checks'])
    result['actual_display_DPI_qualified']=False
    return result
