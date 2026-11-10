"""Exact26 retains every25 gate and independently reads exact system-family RGB/GPU evidence."""
import hashlib
import json
from pathlib import Path
import re
from font_family_reference import description, parse_description, probe_selection
from native_font_progress_validation import ROOT, require
from native_font_window_validation import inspect_cells
from native_font_raster_window_validation import expected_frame_with_raster
from vector_font_state_window_validation import validated_mask

_BASE=ROOT/'scripts/font_raster_flush_window_validation.py'
_text=_BASE.read_text();old="versionName='0.22.25',versionCode=222500";new="versionName='0.22.26',versionCode=222600"
require(_text.count(old)==1,'Changed exact25 identity profile')
_namespace=dict(__name__='candidate26_complete_preserved_font25_gates',__file__=str(_BASE))
exec(compile(_text.replace(old,new,1),str(_BASE)+' [exact26 identity]','exec'),_namespace)
# Keep the frozen25 underlay/version intact. The explicit26 overlay takes final
# precedence for the eleven actual changed inputs, not a weakened old hash gate.
_resample=_namespace['_namespace'];_resource=_resample['_namespace'];_vector=_resource['_namespace']
_resource['SOURCE_MANIFESTS']+=('font-family-inputs.json',)
_native_text=_vector['text'];old="legacy['font_requests']==run*15";new="legacy['font_requests']==run*20"
require(_native_text.count(old)==1,'Changed exact25 native fifteen-request gate')
_native=dict(__name__='candidate26_exact_twenty_font_requests',__file__=str(ROOT/'scripts/native_font_window_validation.py'))
_native_execution_source=_native_text.replace(old,new,1)
exec(compile(_native_execution_source,'candidate26_twenty_actual_requests','exec'),_native)
_native['expected_frame']=expected_frame_with_raster;_vector['_namespace']=_native
IDENTITY=_namespace['IDENTITY']
require(IDENTITY==_native['IDENTITY']==dict(bundleName='com.example.olex2harmonypc',versionName='0.22.26',versionCode=222600),'Lost exact26 identity')
LABELS=['first','scaled','restored','other-family','other-style']

def validate_source(source):
    source=_namespace['validate_source'](source)
    meta=json.loads((source/'font-family-inputs.json').read_text())
    require(meta['version']=='0.22.26' and meta['base']=='0.22.25' and meta['original_font_requests_per_action']==20 and meta['system_family_cases']==LABELS,'Exact26 family source/requests required')
    require(meta['base_manifest_sha256']==hashlib.sha256((source/'font-raster-flush-inputs.json').read_bytes()).hexdigest(),'Frozen25 underlay differs')
    app=json.loads((source/'app/AppScope/app.json5').read_text())['app']
    require({key:app[key] for key in IDENTITY}==IDENTITY,'Source26 app identity differs')
    return source

def validate(folder,previous=None,source=None):
    require(source is not None,'Bind actual26 family implementation')
    source=validate_source(source);folder=Path(folder)
    result=_namespace['validate'](folder,previous,source);run=result['run']
    raw=json.loads((folder/f'shared-original-font-run-{run}.json').read_text())['original']['legacy']
    observed=raw['font']['system_family'];cases=observed['cases'];catalog=json.loads((folder/'system-font-catalog.json').read_text())
    planned=probe_selection(catalog)
    def check(label,value,detail=None):result['checks'].append(dict(name=label,passed=bool(value),detail=detail))
    require([c['label'] for c in cases]==LABELS,'All five actual26 family stages required')
    check('exact installed family/style selection with no default closest or synthetic substitution',observed['exact_family_style_api']=='OH_Drawing_FontStyleSetCreateTypeface' and observed['default_fallback'] is False and observed['closest_style_match'] is False and observed['synthetic_style'] is False and observed['execution_completed'] is True)
    check('missing installed family and exact style explicitly rejected',observed['missing_family_rejected'] is True and observed['missing_style_rejected'] is True)
    check('unchanged96 point conversion is explicit and not actual-display DPI qualification',observed['point_dpi']==96 and observed['actual_display_DPI_qualified'] is False)
    frames={};cells={};summaries=[];masks=[]
    for index,(case,(family,style)) in enumerate(zip(cases,planned)):
        label=case['label'];prefix='family-'+label;points=27 if label=='scaled' else 18
        expected_description=description(family['name'],style)
        decoded=parse_description(case['description'])
        check(label+' exact installed family and actual style index/raw tuple',case['family_index']==family['index'] and case['style_index']==style['index'] and case['family_hex']==family['name'].encode().hex() and case['style_hex']==style['name'].encode().hex() and case['description']==expected_description and decoded['family']==family['name'] and all(case[key]==style[key] for key in ('weight_code','width_code','slant_code')))
        check(label+' genuine selected-face original256 raster path',case['font_created'] is True and case['explicit_system_family'] is True and case['is_vector'] is False and case['fixed'] is False and case['point_size']==points and type(case['actual_face_bold']) is bool and type(case['actual_face_italic']) is bool)
        builds=(run-1)*3+index+1 if index<3 else run
        rasters=(run-1)*2+index+1 if index<3 else 1
        check(label+' exact creation public release and CPU raster cache counts',case['builds']==builds and case['releases']==builds-1 and case['rasterizations']==rasters)
        rgb=(folder/(prefix+'-cells.rgb')).read_bytes();frame=(folder/(prefix+'-actual.rgba')).read_bytes()
        reference=inspect_cells(rgb,case['cell_width'],case['cell_height']);expected,ink=expected_frame_with_raster(reference,case,raw['width'],raw['height'])
        check(label+' all256 original glyph metrics from selected face RGB cells',case['glyphs']==reference['metrics'] and all(case[key]==reference[key] for key in ('max_width','max_height','leftmost','char_offset')))
        check(label+' full actual GPU frame equals independent original glyph layout',frame==expected and ink>0,dict(actual_sha256=hashlib.sha256(frame).hexdigest(),expected_sha256=hashlib.sha256(expected).hexdigest(),ink=ink))
        gpu=json.loads((folder/f'actual-gpu-run-{run}-{prefix}.json').read_text())
        check(label+' actual default-window linked GPU and integer write state',gpu['framebuffer']==0 and gpu['viewport']==[0,0,raw['width'],raw['height']] and gpu['program']>0 and gpu['link_status']==1 and gpu['driver_error_before_snapshot']==gpu['driver_error']==0 and validated_mask(gpu))
        pattern=r'actual-shader-run-'+str(run)+'-'+re.escape(prefix)+r'-[0-9]+[.]txt'
        shaders=[p for p in folder.glob(f'actual-shader-run-{run}-{prefix}-*.txt') if re.fullmatch(pattern,p.name)]
        check(label+' exact actual shader stage sources retained',len(shaders)==2 and all(p.stat().st_size>20 for p in shaders))
        frames[label]=frame;cells[label]=rgb
        summaries.append(dict(label=label,family=family['name'],style=style['name'],family_index=case['family_index'],style_index=case['style_index'],point_size=points,font_address=case['font_address'],actual_face_bold=case['actual_face_bold'],actual_face_italic=case['actual_face_italic'],cells_sha256=hashlib.sha256(rgb).hexdigest(),actual_sha256=hashlib.sha256(frame).hexdigest()))
        masks.append(dict(label=label,actual_integer=gpu['color_write_integer'],legacy_boolean=gpu['color_write'],raw_symbol_boolean=gpu['color_write_raw_boolean']))
    check('one genuine family font object preserved through scale restoration',observed['font_object_preserved'] is True and len({c['font_address'] for c in cases[:3]})==1 and cases[0]['font_address']>0)
    check('different real family and style own separate original font objects',len({cases[i]['font_address'] for i in (0,3,4)})==3 and cases[0]['family_index']!=cases[3]['family_index'] and cases[0]['style_index']!=cases[4]['style_index'])
    check('restoring selected family returns exact original CPU and GPU bytes',cells['first']==cells['restored'] and frames['first']==frames['restored'] and frames['first']!=frames['scaled'])
    if previous is not None:
        previous=Path(previous);prior=json.loads((previous/'received-artifacts.json').read_text());old=json.loads((previous/f'shared-original-font-run-{prior["run"]}.json').read_text())['original']['legacy']['font']['system_family']['cases']
        check('actual installed family/style catalog stays unchanged across destroyed contexts',(folder/'system-font-catalog.json').read_bytes()==(previous/'system-font-catalog.json').read_bytes())
        check('all selected-family font objects and exact GPU cells survive actual context replacement',all(a['font_address']==b['font_address'] and cells[a['label']]==(previous/('family-'+a['label']+'-cells.rgb')).read_bytes() and frames[a['label']]==(previous/('family-'+a['label']+'-actual.rgba')).read_bytes() for a,b in zip(cases,old)))
    result['family_cases']=summaries;result['family_mask_observations']=masks;result['system_family_count']=len(catalog['families'])
    result['font_raster_reference_scope']='Actual selected NativeDrawing RGB cells and exact original GPU layout; not an independent font rasterizer, complete family coverage, chooser dialog or actual-display DPI proof'
    result['passed']=all(row['passed'] for row in result['checks']);return result
