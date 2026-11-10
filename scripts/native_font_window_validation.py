"""Read genuine font evidence; derive bitmap text pixels from received RGB cells."""
from pathlib import Path
import hashlib,json
IDENTITY=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.17',versionCode=221700)
def require(value,message):
 if not value:raise ValueError(message)
def sha(data):return hashlib.sha256(data).hexdigest()
def inspect_cells(raw,width,height):
 require(type(width) is int and type(height) is int and 0<width<=4096 and 0<height<=4096,'Invalid cell dimensions')
 require(width*height*3*256<=128*1024*1024 and len(raw)==width*height*3*256,'Incorrect bounded RGB cell bytes')
 masks=[];metrics=[];max_width=max_height=0;leftmost=1000
 for code in range(256):
  start=code*width*height*3;pixels=[]
  for y in range(height):
   for x in range(width):
    i=start+(y*width+x)*3
    if (raw[i]|raw[i+1]|raw[i+2])!=255:pixels.append((x,y))
  masks.append(pixels)
  if pixels:
   xs,ys=zip(*pixels);left,top,right,bottom=min(xs),min(ys),max(xs),max(ys)
   metrics.append([left,top,right,bottom]);max_width=max(max_width,right-left);max_height=max(max_height,bottom);leftmost=min(leftmost,left)
  else:metrics.append(None)
 require(max_width>0 and max_height>0,'Native font contains no bounded glyph ink')
 offset=2+max_width//10;blank_right=min((max_width//8+1)*8,offset*5)
 for code,glyph in enumerate(metrics):
  if glyph is None:metrics[code]=[0,max_height//2,blank_right,max_height//2]
 return dict(masks=masks,metrics=metrics,max_width=max_width,max_height=max_height,leftmost=leftmost,char_offset=offset)
def expected_frame(reference,case,width,height):
 require(case['text']=='Ab09' and case['origin']==[32,32] and case['fixed'] is False,'Unexpected font draw contract')
 result=bytearray(b'\xff\xff\xff\xff'*(width*height));cursor=0;ink=0
 for code in case['text'].encode('ascii'):
  require(bool(reference['masks'][code]),'Visible test glyph is blank')
  for x,y in reference['masks'][code]:
   px=32+cursor+x-reference['leftmost'];py=32+reference['max_height']-y
   if not (0<=reference['max_height']-y<reference['max_height']):continue
   require(0<=px<width and 0<=py<height,'Reference text would be clipped')
   i=(py*width+px)*4;result[i:i+4]=b'\xff\x00\x00\xff';ink+=1
  cursor+=reference['metrics'][code][2]+reference['char_offset']
 return bytes(result),ink
def validate(folder,previous=None):
 folder=Path(folder);manifest=json.loads((folder/'received-artifacts.json').read_text());run=manifest['run']
 require(manifest['identity']==IDENTITY and type(run) is int and run>=1,'Only exact17 actual font evidence accepted')
 for name,record in manifest['files'].items():
  path=folder/name;require(path.resolve().is_relative_to(folder.resolve()),'Artifact path escaped its folder')
  data=path.read_bytes();require(len(data)==record['bytes'] and sha(data)==record['sha256']==record['target_sha256'],'Target/local font bytes changed: '+name)
 observed=json.loads((folder/f'shared-original-font-run-{run}.json').read_text());original=observed['original'];legacy=original['legacy'];font=legacy['font'];checks=[]
 def check(name,value,detail=None):checks.append(dict(name=name,passed=bool(value),detail=detail))
 check('actual route and current process identities',observed['run']==original['run']==legacy['run']==run and original['pid']==manifest['pid'])
 check('actual main next frame selection and external EGL restored',all(observed.get(k) is True for k in ('limited_path_verified','main_graphics_revalidated','main_selection_unchanged','external_egl_restored')) and observed['main_frames_after']==observed['main_frames_before']+1)
 check('same-window independent context actually detached and destroyed',all(original.get(k) is True for k in ('shared_existing_runtime','same_main_window_surface','original_context_isolated','swapped','temporary_context_detached','temporary_context_destroyed','parent_state_restored')) and original.get('error')=='')
 check('actual unique long-lived owner and balanced leases',legacy['unique_owner'] is True and legacy['binding_before']==legacy['detachment_before']==run-1 and legacy['binding_after']==legacy['detachment_after']==run)
 check('original glyph lists retired before compatibility shutdown',legacy['font_resources_retired_before_gl4es_close'] is True and legacy['scene_unbound'] is True and legacy['scene_disabled'] is True and legacy['font_requests']==run*3 and legacy['execution_completed'] is True and legacy['error']=='')
 check('genuine original-font creation contract',font['native_rasterizer']=='OH_Drawing' and font['genuine_TGlFont'] is True and font['original_256_glyph_lists'] is True and font['font_object_preserved'] is True and font['execution_completed'] is True)
 cases=font['cases'];require([row['label'] for row in cases]==['normal','scaled','restored'],'Exactly normal/scale/restore required')
 frames={};cells={};summaries=[]
 for index,case in enumerate(cases):
  label=case['label'];raw=(folder/f'font-{label}-cells.rgb').read_bytes();actual=(folder/f'font-{label}-actual.rgba').read_bytes()
  ref=inspect_cells(raw,case['cell_width'],case['cell_height']);expected,ink=expected_frame(ref,case,legacy['width'],legacy['height'])
  metrics=[case[k] for k in ('max_width','max_height','leftmost','char_offset')]
  check(label+' genuine original metrics from all256 RGB source cells',case['glyphs']==ref['metrics'] and metrics==[ref[k] for k in ('max_width','max_height','leftmost','char_offset')])
  check(label+' genuine point-size and creation generation',case['font_created'] is True and case['point_size']==(27 if label=='scaled' else 18) and case['builds']==(run-1)*3+index+1 and case['releases']==(run-1)*3+index)
  check(label+' full actual window equals independent binary glyph layout',len(actual)==len(expected) and actual==expected,dict(pixels=legacy['width']*legacy['height'],expected_ink_pixels=ink,expected_sha256=sha(expected),actual_sha256=sha(actual)))
  gpu=json.loads((folder/f'actual-gpu-run-{run}-font-{label}.json').read_text())
  check(label+' actual default-window driver and linked program',gpu['framebuffer']==0 and gpu['viewport']==[0,0,legacy['width'],legacy['height']] and gpu['program']>0 and gpu['link_status']==1 and gpu['driver_error_before_snapshot']==gpu['driver_error']==0)
  shaders=list(folder.glob(f'actual-shader-run-{run}-font-{label}-*.txt'))
  check(label+' actual driver shader source retained',len(shaders)==2 and all(path.stat().st_size>0 for path in shaders))
  frames[label]=actual;cells[label]=raw;summaries.append(dict(label=label,cell_width=case['cell_width'],cell_height=case['cell_height'],point_size=case['point_size'],ink_pixels=ink,actual_rgba_sha256=sha(actual),cells_sha256=sha(raw)))
 check('same original font object through scale and restoration',len({c['font_address'] for c in cases})==1 and cases[0]['font_address']>0)
 check('restoring scale restores exact CPU and actual GPU bytes',cells['normal']==cells['restored'] and frames['normal']==frames['restored'])
 check('actual scaled glyphs differ and have more ink',frames['normal']!=frames['scaled'] and summaries[1]['ink_pixels']>summaries[0]['ink_pixels'])
 if previous is not None:
  previous=Path(previous);prior=json.loads((previous/'received-artifacts.json').read_text());prior_run=prior['run'];old=json.loads((previous/f'shared-original-font-run-{prior_run}.json').read_text())['original']['legacy']
  check('same actual font/owner/scene across destroyed contexts',manifest['pid']==prior['pid'] and run==prior_run+1 and all(legacy[k]==old[k] for k in ('owner_address','renderer_address','scene_address')) and cases[0]['font_address']==old['font']['cases'][-1]['font_address'])
  check('normal glyphs survive actual new-context reconstruction byte-exactly',cells['normal']==(previous/'font-restored-cells.rgb').read_bytes() and frames['normal']==(previous/'font-restored-actual.rgba').read_bytes())
 return dict(passed=all(c['passed'] for c in checks),checks=checks,pid=manifest['pid'],run=run,cases=summaries,actual_context_continuation_verified=previous is not None,full_port_complete=False,functional_completion_percent=None,scope='Genuine original256 glyph creation; only ASCII Ab09 actual pixels at normal/scaled/restored size, not complete font/UI support')
