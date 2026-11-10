"""Validate the explicit libpng backend at the original platform texture seam."""
from gx_application_validation import require,integer,_reference,validate_image_resource_audit,validate_png_decoder_resources

def validate_png_texture_hook(report,fixtures,require_resources=False,resource_proof='full-metadata'):
 require(report.get('execution_completed') is True,'PNG platform texture seam did not complete')
 require(report.get('backend')=='libpng-unassociated-rgb','Missing explicit exact PNG backend identity')
 references={f['file']:f for f in fixtures};require('synthetic-rgb-2.png' in references,'Missing Unicode RGB reference')
 references['合成颜色-2.png']=references['synthetic-rgb-2.png']
 expected_names=['synthetic-rgb-2.png','synthetic-rgba-2.png','synthetic-rgb-1.png','synthetic-rgb-3.png','synthetic-opaque-rgba-4.png','合成颜色-2.png','synthetic-no-image-selected.png']
 cycles=report.get('cycles');require(isinstance(cycles,list) and len(cycles)==3,'Expected three original PNG texture application cycles')
 decoded=transparent=unsupported=allocations=0
 for number,cycle in enumerate(cycles,1):
  for key,wanted in [('cycle',number),('original_graphics_identity',1),('scene_destroyed',1),('font_requests',0),('make_current_attempts',1),('singleton_cleared',1)]:
   require(type(cycle.get(key)) is int and cycle[key]==wanted,'PNG texture lifecycle differs: '+key)
  before=integer(cycle.get('fd_before'),'own fd before',1);after=integer(cycle.get('fd_after'),'own fd after',1)
  require(before==after,'PNG platform image descriptors were not balanced')
  images=cycle.get('images');require(isinstance(images,list) and [row.get('fixture') for row in images]==expected_names,'PNG virtual seam fixture order/content differs')
  for row in images:
   name=row['fixture'];missing=name=='synthetic-no-image-selected.png'
   if missing:w=h=0;expected=[];status='file-error';channels=0
   else:
    fixture=references[name];w,h,rgba=_reference(fixture);channels=fixture['channels']
    status='unsupported-dimensions' if w!=h or w&(w-1) else 'decoded'
    expected=[v for i,v in enumerate(rgba) if i%4!=3] if status=='decoded' else []
    if status=='decoded' and any(rgba[i]!=255 for i in range(3,len(rgba),4)):transparent+=1
   require(row.get('status')==status and row.get('accepted') is (status=='decoded'),'Wrong PNG image decision: '+name)
   for key,wanted in [('width',w),('height',h),('size',w if status=='decoded' else 0),('stride',w*3 if status=='decoded' else 0),('read_bytes',w*h*3 if status=='decoded' else 0),('format',0),('alpha_metadata',0)]:
    require(type(row.get(key)) is int and row[key]==wanted,'PNG virtual seam metadata differs: '+name+' '+key)
   rgb=row.get('rgb');require(isinstance(rgb,list) and all(type(v) is int and 0<=v<=255 for v in rgb) and rgb==expected,'Original PNG RGB differs, including hidden colors: '+name)
   require(isinstance(row.get('error'),str) and bool(row['error'])==(status!='decoded'),'PNG error/result disagree')
   png=row.get('png_decoder');validate_png_decoder_resources(png,opened=not missing)
   for key,wanted in [('source_bit_depth',0 if missing else 8),('source_color_type',0 if missing else (6 if channels==4 else 2)),('source_interlace',0)]:
    require(type(png.get(key)) is int and png[key]==wanted,'PNG source metadata differs: '+key)
   allocations+=png['allocations']
   if require_resources or 'resource_audit' in row:validate_image_resource_audit(row.get('resource_audit'),expected_created=[],expected_attempted=[],require_image_handles=False,proof=resource_proof)
   if status=='decoded':decoded+=1
   else:unsupported+=1
 return dict(cycles=3,decoded_rgb_images=decoded,transparent_hidden_rgb_images=transparent,explicit_unsupported_or_missing=unsupported,
             observed_libpng_callback_allocations=allocations,observed_libpng_callback_releases=allocations,process_descriptor_counts_balanced=True,
             unicode_filename_tested=True,original_virtual_texture_seam=True,original_texture_gl_upload_tested=False,
             transparent_png_virtual_seam_verified=True,complete_application_backend=False,full_port_complete=False)
