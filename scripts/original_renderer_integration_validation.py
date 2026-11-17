"""Revalidate retained actual-window evidence; never equate it to the full app."""
import json
from pathlib import Path
ROUNDTRIPS=('initial','after-application-runtime-restart','after-view-command-restart',
            'final-after-surface-and-cold-restarts','final-same-process-repeat-2','final-same-process-repeat-3')
ADDED_CHECKS={'shared_named_controls_unclipped'}|{'shared_original_'+label for label in ROUNDTRIPS}

def summarize_application(app,root,identity):
    root=Path(root).resolve();checks=app['checks'];names=[i['name'] for i in checks]
    baseline=json.loads((root/'scripts/original_ui_checks.json').read_text())['checks']
    if len(names)!=194 or len(set(names))!=194 or len(baseline)!=187 or [n for n in names if n not in ADDED_CHECKS]!=baseline:
        raise ValueError('Original 187 UI checks were removed, reordered, or new checks are missing')
    if not app['result'].startswith('passed:') or not all(i['passed'] is True for i in checks):raise ValueError('Application regression is not passed')
    by_name={i['name']:i for i in checks};rounds=[]
    for label in ROUNDTRIPS:
        actual=by_name['shared_original_'+label]['details']
        items=actual['checks']
        if (actual.get('passed') is not True or actual.get('full_port_complete') is not False
            or actual.get('functional_completion_percent','missing') is not None
            or actual.get('validated_identity')!=identity or len(items)!=126
            or len({i['name'] for i in items})!=126 or not all(i['passed'] is True for i in items)):
            raise ValueError('Missing exact successful original-window evidence: '+label)
        rounds.append(dict(label=label,pid=actual['pid'],run=actual['run'],checks=len(items)))
    if len({r['pid'] for r in rounds[:4]})!=4 or any(r['run']!=1 for r in rounds[:4]):raise ValueError('Expected four independent live processes')
    if [r['run'] for r in rounds[3:]]!=[1,2,3] or len({r['pid'] for r in rounds[3:]})!=1:raise ValueError('Expected three successive runs in the same final live process')
    return dict(passed=True,roundtrips=rounds,original_ui_checks_preserved=187,total_ui_checks=194,
                total_independent_drawing_checks=126*6,functional_completion_percent=None,full_port_complete=False,
                physical_pc_verified=False,scope='Three synthetic primitives and next-main-frame continuity only; not full renderer/GUI/resources/Python/plugins')

def validate_application(app,root,identity):
    from original_renderer_validation import validate
    root=Path(root).resolve();summary=summarize_application(app,root,identity)
    checks=app['checks']
    report=(root/app['report_directory']).resolve()
    if not report.is_relative_to(root/'reports/app'):raise ValueError('Application report outside retained reports')
    by_name={i['name']:i for i in checks};rounds=[]
    for label in ROUNDTRIPS:
        prefix='shared-'+label
        actual=validate(report/'runtime'/(prefix+'-original'),report/'runtime'/(prefix+'-before'),report/'runtime'/(prefix+'-after'),identity,create_previews=False)
        # Stored evidence is JSON: object keys are strings and tuples become
        # arrays. Compare the exact serialized value, not Python-only types.
        actual=json.loads(json.dumps(actual,ensure_ascii=False,allow_nan=False))
        if not actual['passed'] or len(actual['checks'])!=126 or actual!=by_name['shared_original_'+label]['details']:
            raise ValueError('Actual original-window evidence changed or failed: '+label)
        rounds.append(dict(label=label,pid=actual['pid'],run=actual['run'],checks=len(actual['checks'])))
    return summary
