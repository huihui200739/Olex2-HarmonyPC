"""Independent fixed topology, actual EBO bytes and synthetic edge pixels.

Two uncolored width-one FBO contours only, not primary mesh paths, original
color/width/selecting/normal state or general driver conformance.
"""
import hashlib
import math
import struct
from line_validation import graphic_snapshots
SOURCE='original sgloLineStrip/sgloLineLoop -> bounded source-edge indices -> actual main-context GLES3 mapped EBO and isolated RGBA8 FBO readback; uncolored width-one only'
REGIONS=(('bottom',32,8),('right',56,32),('top',32,56),('closing_left',8,32),('interior',32,32),('outside',2,32),('outside_closing_band',5,32),('inside_closing_band',11,32))

def path_probe_matches(g):
 try:
  p=g['linePathProbe']
  if not all(p[k] is True for k in ('verified','stateRestored','contextRestored')) or p['source']!=SOURCE or p['error']!='':return False
  if not isinstance(p['cases'],list) or len(p['cases'])!=2:return False
  for c,kind in zip(p['cases'],('strip','loop')):
   edges=[v for i in range(3) for v in (i,i+1)]+([3,0] if kind=='loop' else [])
   digest=hashlib.sha256(struct.pack('<'+'I'*len(edges),*edges)).hexdigest()
   if c['kind']!=kind or c['indexSha256']!=digest or c['actualIndices']!=edges or not all(type(v) is int for v in c['actualIndices']):return False
   if type(c['inputVertices']) is not int or c['inputVertices']!=4 or type(c['indexCount']) is not int or c['indexCount']!=len(edges):return False
   if not all(c[k] is True for k in ('actualBufferVerified','pixelsVerified')):return False
   if type(c['actualLineWidth']) not in (int,float) or not math.isfinite(c['actualLineWidth']) or c['actualLineWidth']!=1:return False
   if not isinstance(c['samples'],list) or len(c['samples'])!=len(REGIONS):return False
   for sample,(name,x,y) in zip(c['samples'],REGIONS):
    expected=name in ('bottom','right','top') or (name=='closing_left' and kind=='loop')
    if sample['name']!=name or type(sample['x']) is not int or sample['x']!=x or type(sample['y']) is not int or sample['y']!=y or sample['expectedLit'] is not expected or sample['passed'] is not True:return False
    rgba=sample['rgba']
    if not isinstance(rgba,list) or len(rgba)!=36 or not all(type(v) is int and 0<=v<=255 for v in rgba):return False
    any_lit=False;all_clear=True
    for i in range(0,36,4):
     pixel=rgba[i:i+4];clear=pixel==[0,0,0,255];lit=all(abs(pixel[j]-value)<=1 for j,value in enumerate((68,136,204))) and pixel[3]==255
     if not clear and not lit:return False
     any_lit=any_lit or lit;all_clear=all_clear and clear
    if (not any_lit if expected else not all_clear):return False
  return True
 except (KeyError,TypeError,ValueError,AttributeError):return False

def inactive_path_proof_cleared(g):
 try:
  p=g['linePathProbe'];return all(p[k] is False for k in ('verified','stateRestored','contextRestored')) and p['cases']==[] and p['error']=='' and p['source']==SOURCE
 except (KeyError,TypeError,AttributeError):return False

def app_path_evidence_matches(app):
    if not isinstance(app,dict):
        return False
    result=app.get('result')
    if not isinstance(result,str) or not result.startswith('passed:'):
        return False
    try:
        checks=app['checks']
        if not isinstance(checks,list) or not all(isinstance(c,dict) for c in checks):
            return False
        named=[c for c in checks if c.get('name')=='original_line_path_gpu_probe']
        if len(named)!=1:
            return False
        sample=named[0]
        snapshots=list(graphic_snapshots(app))
        if sample['passed'] is not True or not path_probe_matches(sample['details']) or not snapshots:
            return False
        active=0
        for path,g in snapshots:
            if g.get('surfaceReady') is True and g.get('initialized') is True:
                active+=1
                if not path_probe_matches(g):
                    return False
            else:
                document=(bool(path) and isinstance(path[0],str) and path[0].startswith('options_document_')) or (len(path)>1 and path[0]=='checks' and type(path[1]) is int and checks[path[1]]['name'].startswith('options_document_'))
                if not (document and g.get('surfaceReady') is False and g.get('initialized') is False and inactive_path_proof_cleared(g)):
                    return False
        return active>0
    except (KeyError,TypeError,AttributeError):
        return False
