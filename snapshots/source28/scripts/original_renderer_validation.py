#!/usr/bin/env python3
"""Independent validation of shared-window synthetic rendering and host continuity.

Does not reuse or invent the isolated bundle's parent-FBO/bundle proof fields.
Pixel and matrix references are analytical, not generated from native pass flags.
"""
import argparse, hashlib, json, math, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from layout_validation import controls_unclipped
from matrix_validation import original_matrices_match
from polygon_validation import polygon_probe_matches
from line_path_validation import path_probe_matches
from original_log_validation import audit_matches
from renderer_probe_identity import package_identity,installed_identity

def png(path,w,h,rgba):
 def chunk(kind,data):return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
 # GLES readback origin is bottom-left, PNG origin top-left.
 scan=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h-1,-1,-1))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan,6))+chunk(b'IEND',b''))

def validate_scenes(path,legacy,checks,create_previews):
 d=legacy
 def check(name,ok,detail=None):checks.append({'name':name,'passed':bool(ok),'detail':detail})
 trace_path=path.parent/f'actual-driver-trace-run-{d["run"]}.log'
 trace=trace_path.read_text();adapted=[int(x) for x in re.findall(r'FPE ESSL300 adaptation shader=(\d+)',trace)]
 compiled={int(i):int(ok) for i,ok in re.findall(r'compile (\d+) status=(\d+)',trace)}
 check('generated FPE vertex and fragment compiled',len(adapted)==6 and all(compiled.get(i)==1 for i in adapted),{'adapted':adapted,'compiled':compiled,'failed_unadapted_capability_probe_shaders':[i for i,ok in compiled.items() if not ok and i not in adapted]})
 links=re.findall(r'link (\d+) status=(\d+) uniforms=(\d+) attributes=(\d+) log=([^\n]*)',trace)
 check('actual link reflection not status alone',len(links)>0 and all(status=='1' and int(uniforms)>0 and int(attributes)>0 and not log.strip() for _,status,uniforms,attributes,log in links),links)
 check('actual primitive submission reached driver',len(re.findall('drawArrays mode=',trace))>=3)
 cases=legacy.get('cases',[]);check('three original primitive cases',[c['name'] for c in cases]==['triangle','glu-sphere','glu-disk'])
 w,h=d['width'],d['height'];margin=3/h;step=max(1,min(7,h//128))
 for i,c in enumerate(cases):
  name=c['name'];expected=(255,0,0,255) if i==0 else (0,255,0,255) if i==1 else (0,0,255,255);bg=(10,15,20,255)
  file=path.parent/f'legacy-run-{d["run"]}-{name}.rgba';raw=file.read_bytes();check(name+' exact readback size',len(raw)==w*h*4)
  if len(raw)!=w*h*4:continue
  if create_previews:png(file.with_suffix('.png'),w,h,raw)
  p=[0.0]*16;p[0]=2*h/w;p[5]=2;p[10]=-2/9;p[14]=-11/9;p[15]=1
  m=[0.0]*16;m[0]=m[5]=m[10]=m[15]=1;m[14]=-2
  for key,ref in [('projection',p),('modelview',m)]:
   actual=c.get(key,[]);check(name+' independent '+key,len(actual)==16 and all(type(a) in (int,float) and math.isfinite(a) and abs(a-b)<2e-5 for a,b in zip(actual,ref)),{'actual':actual,'reference':ref})
  gpu=json.loads((path.parent/f'actual-gpu-run-{d["run"]}-{name}.json').read_text())
  check(name+' actual driver program reflected',gpu.get('program',0)>0 and gpu.get('link_status')==1 and gpu.get('program_info')=='' and gpu.get('driver_error_before_snapshot')==gpu.get('driver_error')==0,gpu.get('program_info'))
  check(name+' actual window framebuffer',gpu.get('framebuffer')==0 and gpu.get('viewport')==[0,0,w,h] and gpu.get('scissor_enabled')==0 and gpu.get('depth_test')==1)
  uniforms={u['name']:u for u in gpu.get('active_uniform_values',[])}
  check(name+' actual uniform inventory',len(uniforms)>=10,len(uniforms))
  mvp=p.copy();mvp[14]=-7/9
  refs={'_gl4es_ModelViewMatrix':m,'_gl4es_ModelViewProjectionMatrix':mvp,'_gl4es_NormalMatrix':[1,0,0,0,1,0,0,0,1],'_gl4es_FrontLightModelProduct.sceneColor':[n/255 for n in expected],'_gl4es_BackLightModelProduct.sceneColor':[n/255 for n in expected]}
  for uniform,ref in refs.items():
   actual=uniforms.get(uniform,{}).get('values',[]);check(name+' GPU '+uniform,len(actual)==len(ref) and all(math.isfinite(a) and abs(a-b)<2e-5 for a,b in zip(actual,ref)),{'actual':actual,'reference':ref})
  for key in ('start_draw','end_draw','object_orient','get_dimensions','scene_destroyed'):check(name+' genuine callback '+key,c.get(key)==1,c.get(key))
  check(name+' actual context callback',c.get('make_current',0)>=4,c.get('make_current'))
  check(name+' expected display list usage',c['display_list_compiled']==(i>0))
  tested=inside=outside=0;bad=[]
  tri=[(-.22,-.20),(.22,-.20),(0,.22)]
  for y in range(0,h,step):
   wy=(y+.5-h/2)/h
   for x in range(0,w,step):
    wx=(x+.5-w/2)/h;label=None;r=math.hypot(wx,wy)
    if i==0:
     distances=[]
     for a,b in zip(tri,tri[1:]+tri[:1]):
      distances.append(((b[0]-a[0])*(wy-a[1])-(b[1]-a[1])*(wx-a[0]))/math.hypot(b[0]-a[0],b[1]-a[1]))
     if min(distances)>margin:label=True
     elif min(distances)<-margin:label=False
    elif i==1:
     if r<.22*math.cos(math.pi/16)*math.cos(math.pi/32)-margin:label=True
     elif r>.22+margin:label=False
    else:
     if .09+margin<r<.22*math.cos(math.pi/32)-margin:label=True
     elif r<.09*math.cos(math.pi/32)-margin or r>.22+margin:label=False
    if label is None:continue
    pixel=tuple(raw[(y*w+x)*4:(y*w+x+1)*4]);ref=expected if label else bg
    tested+=1;inside+=label;outside+=not label
    if any(abs(a-b)>2 for a,b in zip(pixel,ref)) and len(bad)<12:bad.append({'x':x,'y':y,'actual':pixel,'expected':ref})
  check(name+' independent pixel masks',not bad and inside>100 and outside>1000,{'tested':tested,'inside':inside,'outside':outside,'failures':bad})
  area=(.44*.42/2) if i==0 else math.pi*.22**2 if i==1 else math.pi*(.22**2-.09**2)
  count=raw.count(bytes(expected));ratio=count/(area*h*h)
  check(name+' independent projected area',.96<ratio<1.015,{'exact_color_pixels':count,'ideal_area_pixels':area*h*h,'ratio':ratio,'rgba_sha256':hashlib.sha256(raw).hexdigest()})

def validate(folder,before,after,expected_identity,create_previews=True):
 checks=[]
 def check(name,ok,detail=None):checks.append({'name':name,'passed':bool(ok),'detail':detail})
 received=json.loads((folder/'received-artifacts.json').read_text());run=received['run']
 integrated=json.loads((folder/f'shared-original-integrated-run-{run}.json').read_text())
 d=integrated.get('original') or {};legacy=d.get('legacy') or {}
 check('all original artifacts match received target hashes',bool(received.get('files')) and all(
  Path(name).name==name and (folder/name).is_file() and
  hashlib.sha256((folder/name).read_bytes()).hexdigest()==record.get('sha256')==record.get('target_sha256') and
  (folder/name).stat().st_size==record.get('bytes') for name,record in received.get('files',{}).items()))
 check('actual original run identity',integrated.get('run')==d.get('run')==legacy.get('run')==run and str(d.get('pid'))==received.get('pid'))
 check('exclusive current-process artifact identity',d.get('artifact_subdirectory')==received.get('artifact_subdirectory')==f"original-renderer-pid-{received.get('pid')}-run-{run}")
 check('standalone original report equals integrated report',json.loads((folder/f'shared-original-window-run-{run}.json').read_text())==d)
 required={f'shared-original-integrated-run-{run}.json',f'shared-original-window-run-{run}.json',f'actual-driver-trace-run-{run}.log'}|{f'actual-gpu-run-{run}-{name}.json' for name in ('parent-before','parent-after','triangle','glu-sphere','glu-disk')}|{f'legacy-run-{run}-{name}.rgba' for name in ('triangle','glu-sphere','glu-disk')}
 check('all required artifacts independently hash-bound',required.issubset(received.get('files',{})))
 check('native limited continuity gate',integrated.get('limited_path_verified') is True)
 for key in ('main_graphics_revalidated','main_selection_unchanged','external_egl_restored'):
  check(key,integrated.get(key) is True)
 check('actual next main frame',isinstance(integrated.get('main_frames_before'),int) and integrated.get('main_frames_after')==integrated['main_frames_before']+1)
 for key in ('shared_existing_runtime','same_main_window_surface','original_context_isolated','swapped','temporary_context_detached','temporary_context_destroyed','parent_state_restored'):
  check(key,d.get(key) is True)
 check('original execution and empty host core',legacy.get('execution_completed') is True and legacy.get('shared_existing_runtime') is True and legacy.get('original_empty_core') is True)
 check('both original errors empty',d.get('error')==legacy.get('error')=='')
 parent_a=json.loads((folder/f'actual-gpu-run-{run}-parent-before.json').read_text())
 parent_b=json.loads((folder/f'actual-gpu-run-{run}-parent-after.json').read_text())
 check('actual parent program/uniform/state exact restoration',parent_a==parent_b)
 for label,data in [('before',parent_a),('after',parent_b)]:
  check('actual parent '+label+' reflected error-free',data.get('driver_error_before_snapshot')==data.get('driver_error')==0 and data.get('program',0)>0 and data.get('link_status')==1 and data.get('program_info')=='' and len(data.get('active_uniform_values',[]))==7)
 a=json.loads((before/'native-verification.json').read_text());b=json.loads((after/'native-verification.json').read_text())
 check('same live process before and after', (before/'pid.txt').read_text().strip()==(after/'pid.txt').read_text().strip()==received.get('pid'))
 for label,data in [('before',a),('after',b)]:
  layout=json.loads(((before if label=='before' else after)/'layout.json').read_text());unclipped,layout_details=controls_unclipped(layout)
  check('actual controls unclipped '+label,unclipped,layout_details)
  identity=data.get('installedPackage',{})
  check('shared candidate identity '+label,identity==installed_identity(expected_identity))
  g=data['graphics'];r=data['applicationRuntime']
  check('main actual graphics '+label,all(g.get(k) is True for k in ('initialized','surfaceReady','meshValidated','submissionValidated','geometryVerified','pixelVerified','coverageVerified','materialVerified','colorVerified')) and g.get('error')=='')
  check('main independent camera uniforms '+label,original_matrices_match(g))
  check('main independent polygon '+label,polygon_probe_matches(g))
  check('main independent line paths '+label,path_probe_matches(g))
  check('single empty original runtime '+label,r.get('initialized') is True and r.get('originalCoreInitialized') is True and r.get('originalCoreEmptyFile') is True and r.get('originalFunctionCount')==22 and r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38 and r.get('error')=='')
  check('original log valid '+label,audit_matches(data.get('originalLog')))
  check('full completion flag remains false '+label,data.get('fullPortComplete') is False and data.get('originalOlex2RendererTested') is False)
 for key in a:
  if key!='graphics':check('existing host unchanged '+key,a[key]==b.get(key))
 check('main frames agree with native integration',a['graphics']['frames']==integrated.get('main_frames_before') and b['graphics']['frames']==integrated.get('main_frames_after'))
 validate_scenes(folder/f'shared-original-window-run-{run}.json',legacy,checks,create_previews)
 return {'passed':all(c['passed'] for c in checks),'checks':checks,'run':run,'pid':received.get('pid'),'functional_completion_percent':None,'validated_identity':expected_identity,'full_port_complete':False,'scope':'Three synthetic primitives through original renderer borrowing existing empty runtime; actual main-window next-frame continuity and host state. Not complete original application, hardware input, or physical PC validation.'}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--before',type=Path,required=True);p.add_argument('--after',type=Path,required=True);p.add_argument('--hap',type=Path,required=True);a=p.parse_args()
 try:result=validate(a.folder,a.before,a.after,package_identity(a.hap))
 except (KeyError,ValueError,TypeError,OSError) as error:result={'passed':False,'checks':[{'name':'complete readable evidence','passed':False,'detail':str(error)}],'functional_completion_percent':None}
 (a.folder/'independent-shared-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({'passed':result['passed'],'checks':len(result['checks']),'failed':[c for c in result['checks'] if not c['passed']]},ensure_ascii=False,indent=2))
 raise SystemExit(0 if result['passed'] else 1)
