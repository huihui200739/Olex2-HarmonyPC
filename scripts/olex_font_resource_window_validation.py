"""Exact23 native/vector and original resource bits; independent full-frame CPU reference."""
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from vector_font_state_window_validation import validated_mask
from native_font_raster_window_validation import f32,raster_position
from native_font_progress_validation import require
ROOT=Path(__file__).resolve().parents[1]
_base=ROOT/'scripts/original_vector_font_window_validation.py'
_text=_base.read_text()
for old,new in (("versionName='0.22.20',versionCode=222000","versionName='0.22.23',versionCode=222300"),
                ("legacy['font_requests']==run*6","legacy['font_requests']==run*9"),
                ("gpu['color_write']==[1,1,1,1]","validated_mask(gpu)")):
 require(_text.count(old)==1,'Unexpected exact original vector verifier source: '+old)
 _text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate23_preserved_complete_native_and_vector_nine_requests',__file__=str(_base),validated_mask=validated_mask)
exec(compile(_text,str(_base)+' [exact23 nine true font requests]','exec'),_namespace)
IDENTITY=_namespace['IDENTITY']
SOURCE_MANIFESTS=('persistent-gx-inputs.json','texture-context-inputs.json','texture-load-inputs.json','texture-load-route-inputs.json','native-font-inputs.json','native-font-layout-inputs.json','native-font-raster-inputs.json','original-vector-font-inputs.json','vector-font-state-inputs.json','olex-font-resource-inputs.json','olex-font-resource-call-inputs.json')


def validate_source(source):
 source=Path(source).resolve();files={}
 for name in SOURCE_MANIFESTS:files.update(json.loads((source/name).read_text())['files'])
 require(json.loads((source/'olex-font-resource-call-inputs.json').read_text())['version']=='0.22.23','Exact23 source required')
 for name,row in files.items():
  path=(source/name).resolve();require(path.is_relative_to(source),'Source manifest path escaped candidate')
  data=path.read_bytes();require(len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],'Frozen source input changed: '+name)
 return source


def resource_metrics(bits,width,height):
 require(type(width) is int and type(height) is int and 0<width<=2048 and 0<height<=2048,'Invalid original resource dimensions')
 require(len(bits)==width*height*32,'Original256 bit-array size differs')
 masks=[];glyphs=[];max_width=max_height=0;leftmost=1000
 for code in range(256):
  points=[]
  for y in range(height):
   for x in range(width):
    index=(code*height+y)*width+x
    if bits[index//8]&(1<<(index%8)):points.append((x,y))
  masks.append(points)
  if points:
   xs,ys=zip(*points);left,top,right,bottom=min(xs),min(ys),max(xs),max(ys)
   glyphs.append([left,top,right,bottom]);max_width=max(max_width,right-left);max_height=max(max_height,bottom);leftmost=min(leftmost,left)
  else:glyphs.append(None)
 require(max_width>0 and max_height>0 and leftmost<1000,'Synthetic resource has no visible glyph geometry')
 bitmap_width=(max_width//8+1)*8
 for i,glyph in enumerate(glyphs):
  if glyph is None:glyphs[i]=[0,max_height//2,min(bitmap_width,6),max_height//2]
 return dict(masks=masks,glyphs=glyphs,max_width=max_width,max_height=max_height,leftmost=leftmost,char_offset=2,bitmap_height=max_height+1)


def resource_frame(reference,case,width,height):
 require(case['text']=='Ab09' and case['fixed'] is False,'Original resource draw contract changed')
 x,y=raster_position(case,width,height);out=bytearray(b'\xff\xff\xff\xff'*(width*height));ink=0
 for code in case['text'].encode('ascii'):
  glyph=reference['glyphs'][code]
  if glyph[0]>0 or glyph[3]>0:
   for sx,sy in reference['masks'][code]:
    bitmap_y=reference['bitmap_height']-sy
    if not (0<=bitmap_y<reference['bitmap_height']):continue
    px=int(x)+sx-reference['leftmost'];py=int(y)+bitmap_y
    require(0<=px<width and 0<=py<height,'Original resource text would be clipped')
    offset=(py*width+px)*4;out[offset:offset+4]=b'\xff\x00\x00\xff';ink+=1
  x=f32(x+glyph[2]+reference['char_offset'])
 return bytes(out),ink


def validate(folder,previous=None,source=None):
 require(source is not None,'Bind exact23 actual resource source')
 source=validate_source(source);folder=Path(folder)
 checked=_namespace['validate'](folder,previous,source);run=checked['run']
 raw=json.loads((folder/f'shared-original-font-run-{run}.json').read_text())['original']['legacy']
 font=raw['font'];resource=font['resource'];cases=resource['cases'];w,h=raw['width'],raw['height']
 def check(name,value,detail=None):checked['checks'].append(dict(name=name,passed=bool(value),detail=detail))
 check('genuine original resource CreateGlyphs without default-font replacement',resource['description']=='#harmony-synthetic-resource.fnt:nr__18' and resource['synthetic_fixture'] is True and resource['genuine_CreateGlyphs'] is True and resource['default_font_fallback'] is False and resource['execution_completed'] is True)
 require([case['label'] for case in cases]==['normal','scaled','restored'],'Three exact resource entry stages required')
 archive=folder/'resource-font-archive.fnt';frames={};summaries=[];masks=[]
 fixture=ROOT/'out/gx-application-evaluation/font-resource-fixtures22'
 fixture_manifest=json.loads((fixture/'manifest.json').read_text())
 require(hashlib.sha256((fixture/'manifest.json').read_bytes()).hexdigest()==json.loads((source/'olex-font-resource-inputs.json').read_text())['fixture_manifest_sha256'],'Frozen synthetic fixture manifest changed')
 expected_archive=fixture_manifest['files']['synthetic-resource-deflate.fnt']
 check('actual font archive bytes equal explicit independent synthetic resource',archive.stat().st_size==expected_archive['bytes'] and hashlib.sha256(archive.read_bytes()).hexdigest()==expected_archive['sha256'])
 with zipfile.ZipFile(archive) as original_zip:
  for index,case in enumerate(cases):
   label=case['label'];points=27 if label=='scaled' else 18;entry='nr__'+str(points)
   payload=original_zip.read(entry);width,height=struct.unpack_from('<HH',payload);bits=(folder/f'resource-{label}-bits.bin').read_bytes()
   check(label+' original ZIP entry dimensions and exact LSB bit-array',case['entry']==entry and case['width']==width and case['height']==height and bits==payload[4:] and case['compression']==8 and case['crc']==original_zip.getinfo(entry).CRC)
   reference=resource_metrics(bits,width,height);actual=(folder/f'resource-{label}-actual.rgba').read_bytes();expected,ink=resource_frame(reference,case,w,h)
   metrics={key:reference[key] for key in ('max_width','max_height','leftmost','char_offset')}
   text_width=sum(reference['glyphs'][code][2]+2 for code in b'Ab0')+reference['glyphs'][ord('9')][2]
   check(label+' genuine original resource bitmap and point size',case['font_created'] is True and case['is_vector'] is False and case['original_resource'] is True and case['point_size']==points and case['rasterizations']==0 and case['draw_state_after']==0)
   check(label+' independently decoded original256 glyph metrics and width',case['glyphs']==reference['glyphs'] and all(case[key]==value for key,value in metrics.items()) and case['text_width']==text_width)
   check(label+' exact build release and resource-read cache generations',case['builds']==(run-1)*3+index+1 and case['releases']==(run-1)*3+index and case['resource_reads']==(run-1)*2+index+1)
   check(label+' full actual window equals independent original bit-array bitmap reference',actual==expected and ink>0,dict(actual_sha256=hashlib.sha256(actual).hexdigest(),expected_sha256=hashlib.sha256(expected).hexdigest(),ink=ink))
   gpu=json.loads((folder/f'actual-gpu-run-{run}-resource-{label}.json').read_text())
   check(label+' actual default-window driver shader and integer write state',gpu['framebuffer']==0 and gpu['viewport']==[0,0,w,h] and gpu['program']>0 and gpu['link_status']==1 and gpu['driver_error_before_snapshot']==gpu['driver_error']==0 and gpu['depth_test']==gpu['cull_face']==gpu['scissor_enabled']==0 and validated_mask(gpu))
   shaders=list(folder.glob(f'actual-shader-run-{run}-resource-{label}-*.txt'))
   check(label+' original resource actual shader sources retained',len(shaders)==2 and all(path.stat().st_size>20 for path in shaders))
   frames[label]=actual;summaries.append(dict(label=label,entry=entry,ink=ink,actual_sha256=hashlib.sha256(actual).hexdigest(),font_address=case['font_address']))
   masks.append(dict(label=label,legacy_boolean=gpu['color_write'],raw_symbol_boolean=gpu['color_write_raw_boolean'],actual_integer=gpu['color_write_integer']))
 check('same distinct original resource font object through selected entry scaling',resource['font_object_preserved'] is True and len({case['font_address'] for case in cases})==1 and type(cases[0]['font_address']) is int and cases[0]['font_address']>0 and cases[0]['font_address'] not in (font['cases'][0]['font_address'],font['vector']['cases'][0]['font_address']))
 check('resource entry scale genuinely changes and exact restoration returns',frames['scaled']!=frames['normal']==frames['restored'] and summaries[1]['ink']>summaries[0]['ink'])
 if previous is not None:
  previous=Path(previous);prior=json.loads((previous/'received-artifacts.json').read_text());old=json.loads((previous/f'shared-original-font-run-{prior["run"]}.json').read_text())['original']['legacy']['font']['resource']
  check('same original resource object and exact pixels across real contexts',cases[0]['font_address']==old['cases'][-1]['font_address'] and frames['normal']==(previous/'resource-restored-actual.rgba').read_bytes())
 checked['resource_cases']=summaries;checked['resource_mask_observations']=masks
 checked['passed']=all(case['passed'] for case in checked['checks'])
 checked['scope']='Exact23 retained native/vector plus genuine original #file:style-size bit-array CreateGlyphs for synthetic nr__18/27 entry scale/restore; original wx resampling outside8..28 and full font resources/UI not certified'
 return checked
