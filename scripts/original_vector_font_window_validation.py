"""Exact20 native raster plus genuine Hershey lists; reference shares GPU backend."""
import hashlib
import json
from pathlib import Path
import re
from native_font_raster_window_validation import expected_frame_with_raster, f32, require

ROOT=Path(__file__).resolve().parents[1]
_base=ROOT/'scripts/native_font_window_validation.py'
text=_base.read_text()
for old,new in (("versionName='0.22.17',versionCode=221700","versionName='0.22.20',versionCode=222000"),
                ("legacy['font_requests']==run*3","legacy['font_requests']==run*6")):
 if text.count(old)!=1:raise ValueError('Unexpected frozen font analytical profile: '+old)
 text=text.replace(old,new,1)
_namespace=dict(__name__='candidate20_preserved_native_raster_checks',__file__=str(_base))
exec(compile(text,str(_base)+' [exact20 six requests per run]','exec'),_namespace)
_namespace['expected_frame']=expected_frame_with_raster
IDENTITY=_namespace['IDENTITY']


def source_table(text):
 marker='int gl_font_simplex[95][112] = {'
 require(text.count(marker)==1,'Missing unique original95x112 vector table')
 body=text.split(marker,1)[1].split('};',1)[0]
 body=re.sub(r'/\*.*?\*/','',body,flags=re.S)
 require(re.fullmatch(r'[\s,\d-]+',body) is not None,'Unexpected original vector initializer syntax')
 values=[int(x) for x in re.findall(r'-?\d+',body)]
 require(len(values)==95*112,'Original vector table dimensions differ')
 require(all(-32768<=v<=32767 for v in values),'Original vector table bounds differ')
 table=[values[i:i+112] for i in range(0,len(values),112)]
 require(all(row[j]!=-1 or row[j+1]==-1 for row in table for j in range(2,112,2)),
         'Current original sentinel pairs differ')
 return table


def original_metrics(table,points=18):
 require(len(table)==95 and all(len(row)==112 for row in table),'Expected95 complete original glyph rows')
 glyphs=[[-1,-1,-1,-1] for _ in range(256)]
 max_width=max_height=0;leftmost=1000
 for i,row in enumerate(table):
  left=top=right=bottom=-1
  for x,y in zip(row[2::2],row[3::2]):
   if x==y==-1:continue
   left=min(left,x);top=min(top,y);right=max(right,x);bottom=max(bottom,y)
  glyphs[i+32]=[left,top,right,bottom]
  max_width=max(max_width,right-left);max_height=max(max_height,bottom-top);leftmost=min(leftmost,left)
 glyphs[32]=[0,0,max_width//2,max_height]
 return dict(glyphs=glyphs,max_width=int(max_width*points/15),max_height=int(max_height*points/15),
             leftmost=int(leftmost*points/15),char_offset=0,
             text_width=int(sum(glyphs[c][2] for c in b'Ab09')*points/15))


def colored_pixels(raw,w,h):
 require(len(raw)==w*h*4,'Incorrect actual/reference vector window byte count')
 count=0
 for i in range(0,len(raw),4):
  pixel=raw[i:i+4]
  require(pixel in (b'\xff\xff\xff\xff',b'\xff\x00\x00\xff'),
          'Actual vector window contains non-reference colors')
  count+=pixel==b'\xff\x00\x00\xff'
 return count


def validate(folder,previous=None,source=None):
 require(source is not None,'Bind the actual frozen20 source, not an inferred table')
 folder,source=Path(folder),Path(source)
 manifest=json.loads((source/'original-vector-font-inputs.json').read_text())
 require(manifest['version']=='0.22.20','Only exact20 vector source accepted')
 src=manifest['original_vector_source'];require(src['path']=='upstream/glib/glfont.cpp','Wrong original vector source path')
 original_source=(source/src['path']).read_bytes();require(hashlib.sha256(original_source).hexdigest()==src['sha256'],'Original vector source bytes changed')
 table=source_table(original_source.decode());metrics=original_metrics(table)
 checked=_namespace['validate'](folder,previous);run=checked['run']
 raw=json.loads((folder/f'shared-original-font-run-{run}.json').read_text())['original']['legacy']
 vector=raw['font']['vector'];w,h=raw['width'],raw['height'];cases=vector['cases'];frames={};summaries=[]
 def check(name,value,detail=None):checked['checks'].append(dict(name=name,passed=bool(value),detail=detail))
 check('original vector descriptor genuine lists and honest shared-backend reference',
       vector['description']=='@18' and vector['genuine_CreateHershey'] is True and
       vector['reference_calls_TGlFont'] is False and vector['shared_GPU_backend'] is True and vector['execution_completed'] is True)
 check('actual source table equals independently parsed unchanged upstream data',vector['source_table']==table)
 require([c['label'] for c in cases]==['normal','scene-scaled','draw-scaled','restored'],'Four exact vector cases required')
 expected_projection=[0.0]*16;expected_projection[0]=f32(2/w);expected_projection[5]=f32(2/h);expected_projection[10]=-1.;expected_projection[12]=expected_projection[13]=-1.;expected_projection[15]=1.
 identity=[1.0 if i in (0,5,10,15) else 0.0 for i in range(16)]
 for index,case in enumerate(cases):
  label=case['label'];generation=(1,2,2,3)[index]
  actual=(folder/f'vector-{label}-actual.rgba').read_bytes();reference=(folder/f'vector-{label}-reference.rgba').read_bytes()
  ink=colored_pixels(actual,w,h);reference_ink=colored_pixels(reference,w,h)
  check(label+' genuine vector creation and unchanged intrinsic point size',case['font_created'] is True and case['is_vector'] is True and
        case['point_size']==18 and case['vector_scale']==1200 and case['rasterizations']==0 and case['draw_state_after']==0)
  check(label+' independent original95 glyph metrics and text width',all(case[k]==v for k,v in metrics.items()))
  check(label+' original vector creation and retirement generations',case['builds']==(run-1)*3+generation and case['releases']==(run-1)*3+generation-1)
  check(label+' fixed origin text actual matrices and original draw scalar',case['origin']==[32,96,0] and case['text']=='Ab09' and case['draw_scale']==(1800 if label=='draw-scaled' else 1200) and
        [f32(v) for v in case['projection']]==expected_projection and case['modelview']==identity)
  check(label+' entire actual window equals direct upstream-table replay without TGlFont',actual==reference and ink==reference_ink and ink>0 and case['actual_equals_direct_reference'] is True,
        dict(pixels=w*h,ink=ink,actual_sha256=hashlib.sha256(actual).hexdigest(),reference_sha256=hashlib.sha256(reference).hexdigest(),independent_GPU_backend=False))
  for route in ('actual','reference'):
   gpu=json.loads((folder/f'actual-gpu-run-{run}-vector-{label}-{route}.json').read_text())
   check(label+' '+route+' genuine default-window driver and shader',gpu['framebuffer']==0 and gpu['viewport']==[0,0,w,h] and gpu['program']>0 and gpu['link_status']==1 and
         gpu['driver_error_before_snapshot']==gpu['driver_error']==0 and gpu['color_write']==[1,1,1,1] and gpu['depth_test']==gpu['cull_face']==gpu['scissor_enabled']==0)
   shaders=list(folder.glob(f'actual-shader-run-{run}-vector-{label}-{route}-*.txt'))
   check(label+' '+route+' real driver shader source retained',len(shaders)==2 and all(p.stat().st_size>20 for p in shaders))
  frames[label]=actual;summaries.append(dict(label=label,ink=ink,actual_sha256=hashlib.sha256(actual).hexdigest(),font_address=case['font_address']))
 check('same distinct original vector font object through all four cases',vector['same_font_object'] is True and len({c['font_address'] for c in cases})==1 and
       type(cases[0]['font_address']) is int and cases[0]['font_address']>0 and cases[0]['font_address']!=raw['font']['cases'][0]['font_address'])
 check('surface font scaling preserves original intrinsic vector semantics',frames['scene-scaled']==frames['normal']==frames['restored'])
 check('original vector draw scalar genuinely enlarges visible strokes',frames['draw-scaled']!=frames['normal'] and summaries[2]['ink']>summaries[0]['ink'])
 if previous is not None:
  prior_manifest=json.loads((Path(previous)/'received-artifacts.json').read_text());n=prior_manifest['run']
  prior=json.loads((Path(previous)/f'shared-original-font-run-{n}.json').read_text())['original']['legacy']['font']['vector']
  check('same original vector object and exact actual pixels through new real context',cases[0]['font_address']==prior['cases'][-1]['font_address'] and frames['normal']==(Path(previous)/'vector-restored-actual.rgba').read_bytes())
 checked['passed']=all(c['passed'] for c in checked['checks'])
 checked['vector_cases']=summaries;checked['vector_reference_is_independent_GPU_backend']=False
 checked['scope']='Exact20 retained native ASCII raster + original @18 Hershey lists at normal/intrinsic surface scaling/draw scaling/restoration; direct geometry reference shares GL4ES/GPU backend; not complete fonts or UI'
 return checked
