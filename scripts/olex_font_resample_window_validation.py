"""Exact24 preserves all23 gates and checks genuine RGB resampling against fixed wx source."""
import hashlib,json,struct,zipfile
from pathlib import Path
from native_font_progress_validation import ROOT,require,check_bound
from vector_font_state_window_validation import validated_mask
from resource_font_resample_reference import CASES,inspect_resource_rgb,resource_rgb_frame
_base=ROOT/'scripts/olex_font_resource_window_validation.py';_text=_base.read_text()
for old,new in (
 ("versionName='0.22.23',versionCode=222300","versionName='0.22.24',versionCode=222400"),
 ("legacy['font_requests']==run*9","legacy['font_requests']==run*15"),
 ("'olex-font-resource-inputs.json','olex-font-resource-call-inputs.json')","'olex-font-resource-inputs.json','olex-font-resource-call-inputs.json','olex-font-resample-inputs.json')"),
 ("(source/'olex-font-resource-call-inputs.json').read_text())['version']=='0.22.23'","(source/'olex-font-resample-inputs.json').read_text())['version']=='0.22.24'")):
 require(_text.count(old)==1,'Changed frozen23 verifier recipe: '+old);_text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate24_preserved_complete_resource23_gates',__file__=str(_base))
exec(compile(_text,str(_base)+' [exact24 fifteen true font requests]','exec'),_namespace)
IDENTITY=_namespace['IDENTITY'];validate_source=_namespace['validate_source']

def validate(folder,previous=None,source=None):
 require(source is not None,'Bind actual24 resampling source')
 source=validate_source(source);folder=Path(folder);result=_namespace['validate'](folder,previous,source);run=result['run']
 reference_path=ROOT/'out/gx-application-evaluation/font-resample-reference24/result.json';reference=json.loads(reference_path.read_text())
 require(reference['passed'] and reference['full_port_complete'] is False,'Independent fixed wx source cells missing')
 for evidence in reference['evidence']+[reference['archive'],reference['fixture_manifest']]:check_bound(evidence)
 require(hashlib.sha256(check_bound(reference['fixture_manifest']).read_bytes()).hexdigest()==json.loads((source/'olex-font-resample-inputs.json').read_text())['fixture_manifest_sha256'],'Resample fixture manifest differs from actual24 source')
 raw=json.loads((folder/f'shared-original-font-run-{run}.json').read_text())['original']['legacy'];resample=raw['font']['resampled_resource'];cases=resample['cases'];w,h=raw['width'],raw['height']
 def check(name,value,detail=None):result['checks'].append(dict(name=name,passed=bool(value),detail=detail))
 require([case['label'] for case in cases]==[spec[0] for spec in CASES],'All six exact resampling stages required')
 check('original resource RGB resampling explicit synthetic route without fallback',resample['synthetic_fixture'] is True and resample['original_resource_RGB_path'] is True and resample['default_font_fallback'] is False and resample['execution_completed'] is True)
 archive=folder/'resample-font-archive.fnt';check('actual original resample archive bytes match independent fixture',archive.read_bytes()==check_bound(reference['archive']).read_bytes())
 summaries=[];frames={};masks=[]
 with zipfile.ZipFile(archive) as original_zip:
  for case,expected,(_,style,points) in zip(cases,reference['cases'],CASES):
   label=case['label'];prefix='resample-'+label;entry=style+('8' if points<8 else '72');payload=original_zip.read(entry);ow,oh=struct.unpack_from('<HH',payload)
   bits=(folder/(prefix+'-bits.bin')).read_bytes();rgb=(folder/(prefix+'-cells.rgb')).read_bytes();expected_rgb=check_bound(expected['expected']).read_bytes();check_bound(expected['source'])
   check(label+' actual source entry and exact256 LSB bits',case['entry']==entry and (case['width'],case['height'])==(ow,oh) and bits==payload[4:] and case['crc']==original_zip.getinfo(entry).CRC and case['compression']==8)
   check(label+' exact opaque RGB cells equal unchanged fixed wx source reference',rgb==expected_rgb and (case['cell_width'],case['cell_height'])==(expected['width'],expected['height']),dict(actual_sha256=hashlib.sha256(rgb).hexdigest(),expected_sha256=hashlib.sha256(expected_rgb).hexdigest()))
   metrics=inspect_resource_rgb(expected_rgb,expected['width'],expected['height'],expected['fixed'])
   check(label+' all256 original RGB glyph metrics and fixed/variable width',case['glyphs']==metrics['glyphs'] and all(case[key]==metrics[key] for key in ('max_width','max_height','leftmost','char_offset','text_width','fixed')))
   check(label+' genuine original RGB bitmap creation and requested point size',case['font_created'] is True and case['is_vector'] is False and case['original_resource'] is True and case['resource_uses_rgb'] is True and case['point_size']==points and case['entry_points']==expected['entry_points'] and case['rasterizations']==0 and case['draw_state_after']==0)
   if label.startswith('large'):
    stage={'large':1,'large-scaled':2,'large-restored':3}[label];builds=(run-1)*3+stage;reads=(run-1)*2+stage
   else:builds,reads=run,1
   check(label+' exact build release source-read and resampling cache counts',case['builds']==builds and case['releases']==builds-1 and case['resource_reads']==reads and case['resource_resamplings']==reads)
   expected_frame,ink=resource_rgb_frame(metrics,case,w,h);actual=(folder/(prefix+'-actual.rgba')).read_bytes()
   check(label+' full real window equals independently laid out original RGB bitmaps',actual==expected_frame and ink>0,dict(actual_sha256=hashlib.sha256(actual).hexdigest(),expected_sha256=hashlib.sha256(expected_frame).hexdigest(),ink=ink))
   gpu=json.loads((folder/f'actual-gpu-run-{run}-{prefix}.json').read_text())
   check(label+' actual default-window GPU shader and integer write state',gpu['framebuffer']==0 and gpu['viewport']==[0,0,w,h] and gpu['program']>0 and gpu['link_status']==1 and gpu['driver_error_before_snapshot']==gpu['driver_error']==0 and gpu['depth_test']==gpu['cull_face']==gpu['scissor_enabled']==0 and validated_mask(gpu))
   shaders=list(folder.glob(f'actual-shader-run-{run}-{prefix}-*.txt'));check(label+' actual shader sources retained',len(shaders)==2 and all(p.stat().st_size>20 for p in shaders))
   summaries.append(dict(label=label,entry=entry,point_size=points,font_address=case['font_address'],ink=ink,actual_sha256=hashlib.sha256(actual).hexdigest(),rgb_sha256=hashlib.sha256(rgb).hexdigest()));frames[label]=actual
   masks.append(dict(label=label,actual_integer=gpu['color_write_integer'],legacy_boolean=gpu['color_write'],raw_symbol_boolean=gpu['color_write_raw_boolean']))
 check('same original large resource object through real scale and restoration',resample['font_object_preserved'] is True and len({cases[i]['font_address'] for i in (1,2,3)})==1 and frames['large']!=frames['large-scaled'] and frames['large']==frames['large-restored'])
 check('small bold fixed-italic and italic resource objects remain distinct',len({cases[i]['font_address'] for i in (0,1,4,5)})==4)
 if previous is not None:
  prior=json.loads((Path(previous)/'received-artifacts.json').read_text());old=json.loads((Path(previous)/f'shared-original-font-run-{prior["run"]}.json').read_text())['original']['legacy']['font']['resampled_resource']['cases']
  check('all resampled objects and exact pixels survive actual context replacement',all(a['font_address']==b['font_address'] and frames[a['label']]==(Path(previous)/('resample-'+a['label']+'-actual.rgba')).read_bytes() for a,b in zip(cases,old)))
 result['resample_cases']=summaries;result['resample_mask_observations']=masks;result['resample_reference_scope']=reference['scope'];result['passed']=all(row['passed'] for row in result['checks'])
 result['scope']='Exact24 all prior native/vector/resource23 gates plus six synthetic small/large/scaled/restored/fixed-italic/identity RGB resource stages; not full font, wx runtime or full application qualification'
 return result
