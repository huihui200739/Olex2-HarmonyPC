"""Strict validation of the ordinary synthetic libpng decoder observation set."""
from gx_application_validation import validate_image_resource_audit
EXPECTED_CHECKS=624
EXPECTED_IMAGES=56
EXPECTED_HIDDEN_RGB_IMAGES=34
EXPECTED_ADAM7_IMAGES=29

def expected_cases():
 cases=set()
 for interlace in (0,1):
  for depth in (1,2,4,8,16):
   for transparency in (0,1):cases.add(f'png-0-{depth}-{transparency}-{interlace}-8x8.png')
  for depth in (1,2,4,8):
   for transparency in (0,1):cases.add(f'png-3-{depth}-{transparency}-{interlace}-8x8.png')
  for depth in (8,16):
   for kind in (2,6,4):cases.add(f'png-{kind}-{depth}-0-{interlace}-8x8.png')
   cases.add(f'png-2-{depth}-1-{interlace}-8x8.png')
 for size in (1,2,4):cases.add(f'png-6-8-0-1-{size}x{size}.png')
 cases.add('合成透明颜色.png')
 return cases

def validate_png_observations(raw):
 def require(value,message):
  if not value:raise ValueError(message)
 def exact(value,expected,label):require(type(value) is type(expected) and value==expected,label+' differs')
 exact(raw.get('passed'),True,'PNG pass marker')
 exact(raw.get('decoder_version'),'1.6.59','Fixed PNG decoder version')
 for key,value in [('checks',EXPECTED_CHECKS),('decoded_images',EXPECTED_IMAGES),('hidden_rgb_images',EXPECTED_HIDDEN_RGB_IMAGES),('adam7_images',EXPECTED_ADAM7_IMAGES)]:exact(raw.get(key),value,key)
 exact(raw.get('real_gpu_execution'),False,'GPU execution scope')
 exact(raw.get('full_port_complete'),False,'Full-port scope')
 images=raw.get('images');require(isinstance(images,list) and len(images)==EXPECTED_IMAGES,'PNG image matrix differs')
 names=[r.get('name') for r in images];require(len(set(names))==EXPECTED_IMAGES and set(names)==expected_cases(),'PNG ordinary fixture matrix missing/duplicated')
 allocations=releases=0
 for row in images:
  name=row['name'];exact(row.get('decoded'),True,'Image decode')
  require(type(row.get('source_bit_depth')) is int and row['source_bit_depth'] in (1,2,4,8,16),'PNG depth differs')
  require(type(row.get('source_color_type')) is int and row['source_color_type'] in (0,2,3,4,6),'PNG color type differs')
  require(type(row.get('source_interlace')) is int and row['source_interlace'] in (0,1),'PNG interlace differs')
  if name=='合成透明颜色.png':expected=(8,6,0,12,True)
  else:
   parts=name[:-4].split('-');kind,depth,transparent,interlace=map(int,parts[1:5]);w,h=map(int,parts[5].split('x'));expected=(depth,kind,interlace,w*h*3,False)
  for key,value in zip(('source_bit_depth','source_color_type','source_interlace','rgb_bytes','unicode_path'),expected):exact(row.get(key),value,key)
  a=row.get('allocations');r=row.get('releases');require(type(a) is int and a>0 and type(r) is int and r==a,'PNG allocation callback counts differ')
  exact(row.get('live_allocations'),0,'PNG live callbacks');exact(row.get('live_bytes'),0,'PNG live callback bytes')
  audit=row.get('input_resources');require(isinstance(audit,dict),'Missing own input audit')
  require(audit.get('handles')==[],'CPU PNG decoder must not claim public ImageKit handles')
  own=validate_image_resource_audit(audit,expected_created=[],expected_attempted=[],require_image_handles=False,proof='owned-memory')
  require(own['input_opened'] is True,'PNG input open not observed')
  allocations+=a;releases+=r
 return dict(passed=True,checks=EXPECTED_CHECKS,decoded_images=EXPECTED_IMAGES,hidden_rgb_images=EXPECTED_HIDDEN_RGB_IMAGES,adam7_images=EXPECTED_ADAM7_IMAGES,
             observed_libpng_callback_allocations=allocations,observed_libpng_callback_releases=releases,
             imagekit_executed=False,real_gpu_execution=False,full_port_complete=False,functional_completion_percent=None,
             full_process_metadata_scan_passed=all(x['input_resources']['balanced'] for x in images),
             all_process_resources_attributed=False)
