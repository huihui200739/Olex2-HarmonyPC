"""Exact28 preserves all26 gates and adds actual main-window physical DPI binding."""
import json
from pathlib import Path
from native_font_progress_validation import ROOT,require
from font_display_source_validation import validate_source28
from font_display_reference import validate_metrics,validate_case

_BASE=ROOT/'scripts/font_family_window_validation.py'
_text=_BASE.read_text()
old="versionName='0.22.26',versionCode=222600";new="versionName='0.22.28',versionCode=222800"
require(_text.count(old)==2,'Changed frozen26 identity guards')
_text=_text.replace(old,new)
old="check('unchanged96 point conversion is explicit and not actual-display DPI qualification',observed['point_dpi']==96 and observed['actual_display_DPI_qualified'] is False)"
new="check('current physical x/y DPI observation replaces stale fixed96 telemetry without claiming full display qualification',observed['physical_point_dpi']==[raw['font']['display_metrics']['dpi_x'],raw['font']['display_metrics']['dpi_y']] and observed['actual_display_DPI_observed'] is True and observed['actual_display_DPI_qualified'] is False)"
require(_text.count(old)==1,'Changed frozen26 explicit fixed96 telemetry gate')
_text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate28_preserved26_pixels_with_actual_window_display',__file__=str(_BASE))
exec(compile(_text,str(_BASE)+' [exact28 identity and actual DPI telemetry]','exec'),_namespace)
_namespace['_resource']['SOURCE_MANIFESTS']+=('font-display-inputs.json','font-display-telemetry-inputs.json')
IDENTITY=_namespace['IDENTITY']
require(IDENTITY==dict(bundleName='com.example.olex2harmonypc',versionName='0.22.28',versionCode=222800),'Lost exact28 identity')

def validate_source(source):
    source=validate_source28(source)
    return _namespace['validate_source'](source)

def validate(folder,previous=None,source=None):
    require(source is not None,'Bind actual28 measured-display implementation')
    source=validate_source(source);folder=Path(folder)
    result=_namespace['validate'](folder,previous,source);run=result['run']
    original=json.loads((folder/f'shared-original-font-run-{run}.json').read_text())['original']['legacy']
    font=original['font'];metrics=validate_metrics(font['display_metrics'])
    cases=font['cases']+font['system_family']['cases']
    require(len(cases)==8,'All three native and five family actual-DPI cases required')
    for case in cases:
        parameters=validate_case(case,metrics)
        result['checks'].append(dict(name=case['label']+' binds exact observed display identity and float32 physical DPI size/axis scale',passed=True,detail=parameters))
    if previous is not None:
        prior=Path(previous);previous_meta=json.loads((prior/'received-artifacts.json').read_text());n=previous_meta['run']
        prior_metrics=json.loads((prior/f'shared-original-font-run-{n}.json').read_text())['original']['legacy']['font']['display_metrics']
        require(prior_metrics==metrics,'This preserved cache/pixel regression requires a stable actual display across rounds; no simulated monitor change')
    result['display_metrics']=metrics
    result['passed']=all(row['passed'] for row in result['checks'])
    return result
