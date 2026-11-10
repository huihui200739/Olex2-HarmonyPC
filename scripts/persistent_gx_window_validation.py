"""Explicit lifecycle profile; original pixels/matrices/host-state gates remain."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE='upstream/gxlib/gxapp.cpp: original TGXApp empty startup with one long-lived renderer and platform scene (core metadata catalogs separately exposed); upstream/sdl/bapp.cpp: original TBasicApp directories and ExportLibrary functions + settingsfile.cpp: original settings format; platform-owned private directories and atomic SaveOptions override'
def replace_once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected baseline shape: '+old[:80])
 return text.replace(old,new,1)
def original_profile():
 p=ROOT/'scripts/original_renderer_validation.py';s=p.read_text()
 s=replace_once(s,'len(adapted)==2','len(adapted)==6')
 s=replace_once(s,"r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38","r.get('originalCoreMacroCount')==193 and r.get('originalCoreFunctionCount')==46")
 old="for key in ('start_draw','end_draw','object_orient','get_dimensions','scene_destroyed'):check(name+' genuine callback '+key,c.get(key)==1,c.get(key))"
 new="for key in ('start_draw','end_draw','object_orient','get_dimensions','scene_destroyed'):check(name+' genuine callback '+key,c.get(key)==(0 if key=='scene_destroyed' else 1),c.get(key))"
 s=replace_once(s,old,new)
 return s

def validate_lifetime(proof,pid,run,snapshots,previous=None):
 checks=[]
 def check(name,value,detail=None):checks.append(dict(name=name,passed=bool(value),detail=detail))
 for key in ('unique_owner','owner_unchanged','scene_disabled','scene_unbound','cleanup_before_gl4es_close'):
  check('persistent GX '+key,proof.get(key) is True)
 check('persistent GX no premature destruction or font claims',type(proof.get('scene_destroyed')) is int and proof['scene_destroyed']==0 and type(proof.get('font_requests')) is int and proof['font_requests']==0 and proof.get('cleanup_failed') is False)
 integer=lambda x:type(x) is int and x>=0
 check('one balanced context lease per actual run',type(run) is int and run>0 and all(integer(proof.get(k)) for k in ('binding_before','binding_after','detachment_before','detachment_after')) and proof['binding_after']==proof['binding_before']+1==run and proof['detachment_before']==proof['binding_before'] and proof['detachment_after']==proof['binding_after'])
 addresses={k:proof.get(k) for k in ('owner_address','renderer_address','scene_address')}
 check('three distinct live original object addresses',all(type(v) is int and v>0 for v in addresses.values()) and len(set(addresses.values()))==3,addresses)
 check('actual persistent GX runtime source both sides',all(s['applicationRuntime'].get('source')==SOURCE for s in snapshots))
 check('same live owners across consecutive runs',previous is None and run==1 or previous is not None and previous.get('passed') is True and previous.get('pid')==pid and previous.get('run')==run-1 and previous.get('addresses')==addresses)
 return checks,addresses

def validate(folder,before,after,identity,previous=None,create_previews=True):
 folder,before,after=map(Path,(folder,before,after))
 namespace={'__file__':str(ROOT/'scripts/original_renderer_validation.py'),'__name__':'persistent_gx_original_profile'}
 exec(compile(original_profile(),namespace['__file__'],'exec'),namespace)
 original=namespace['validate'](folder,before,after,identity,create_previews=create_previews)
 manifest=json.loads((folder/'received-artifacts.json').read_text());run=manifest['run'];pid=manifest['pid']
 integrated=json.loads((folder/f'shared-original-integrated-run-{run}.json').read_text())
 proof=integrated['original']['legacy']['persistent_gx']
 snapshots=[json.loads((p/'native-verification.json').read_text()) for p in (before,after)]
 checks,addresses=validate_lifetime(proof,pid,run,snapshots,previous)
 return dict(passed=original['passed'] and len(original['checks'])==126 and all(c['passed'] for c in checks),original=original,checks=checks,pid=pid,run=run,addresses=addresses,full_port_complete=False,functional_completion_percent=None,scope='Unique long-lived empty TGXApp/renderer/scene, actual surface leases and preserved synthetic window/host checks only; not full application qualification.')
