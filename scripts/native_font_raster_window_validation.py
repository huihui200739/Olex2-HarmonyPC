"""Integer raster math for the actual AArch64 FMLA binary; no fitted shifts/tolerance."""
from pathlib import Path
import json,math,struct
_BASE=Path(__file__).with_name('native_font_window_validation.py')
text=_BASE.read_text();old="versionName='0.22.17',versionCode=221700";new="versionName='0.22.19',versionCode=221900"
if text.count(old)!=1:raise ValueError('Unexpected original font proof baseline')
namespace=dict(__name__='candidate19_integer_raster_analytical_profile',__file__=str(_BASE))
exec(compile(text.replace(old,new,1),str(_BASE)+' [exact19 identity]','exec'),namespace)
IDENTITY=namespace['IDENTITY'];require=namespace['require']
def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def raster_position(case,width,height):
 require(case['origin']==[32,32],'Original integer draw coordinates changed')
 expected=[0.0]*16;expected[0]=f32(2/width);expected[5]=f32(2/height);expected[10]=-1.0;expected[12]=expected[13]=-1.0;expected[15]=1.0
 identity=[1.0 if i in (0,5,10,15) else 0.0 for i in range(16)]
 projection=case.get('projection');modelview=case.get('modelview')
 require(isinstance(projection,list) and len(projection)==16 and all(type(v) in (int,float) and math.isfinite(v) for v in projection),'Missing actual finite projection')
 require([f32(v) for v in projection]==expected and modelview==identity,'Actual original orthographic/modelview matrices differ')
 # Actual glRasterPos3f contains AArch64 fmla for viewport XY. Source and the
 # HAP-bound unstripped binary disassembly are retained separately. Products of
 # binary32 operands at these bounded values are exact in binary64, then one
 # binary32 rounding models fmla, rather than rounding its product separately.
 coordinates=[]
 for extent,scale,offset in ((width,projection[0],projection[12]),(height,projection[5],projection[13])):
  clip=f32(f32(scale)*32+f32(offset));half=f32(extent/2)
  coordinates.append(f32(clip*half+half))
 return coordinates
def expected_frame_with_raster(reference,case,width,height):
 require(case['text']=='Ab09' and case['fixed'] is False,'Original glyph text contract changed')
 x,y=raster_position(case,width,height);result=bytearray(b'\xff\xff\xff\xff'*(width*height));ink=0
 for code in case['text'].encode('ascii'):
  require(bool(reference['masks'][code]),'Visible glyph is blank')
  for sx,sy in reference['masks'][code]:
   if not (0<=reference['max_height']-sy<reference['max_height']):continue
   px=int(x)+sx-reference['leftmost'];py=int(y)+reference['max_height']-sy
   require(0<=px<width and 0<=py<height,'Reference glyphs clipped')
   i=(py*width+px)*4;result[i:i+4]=b'\xff\x00\x00\xff';ink+=1
  x=f32(x+reference['metrics'][code][2]+reference['char_offset'])
 return bytes(result),ink
# All prior outer restoration, identity, resource, glyph metrics and strict
# full-frame equality gates stay intact; only the independent position model
# changes from ideal real arithmetic to the actual compiled float32 FMLA path.
namespace['expected_frame']=expected_frame_with_raster
_original_validate=namespace['validate']
def validate(folder,previous=None):
 result=_original_validate(folder,previous);folder=Path(folder)
 raw=json.loads((folder/f'shared-original-font-run-{result["run"]}.json').read_text())['original']['legacy']
 result['raster_reference']=[dict(label=case['label'],requested_integer_origin=case['origin'],independent_float32_fmla_window=raster_position(case,raw['width'],raw['height'])) for case in raw['font']['cases']]
 result['raster_model']='actual AArch64 fmla viewport arithmetic and bitmap integer conversion; no empirical image translation'
 return result
