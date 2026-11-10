"""Independent expectations for the fixed, explicitly scoped pc.* UI samples.

This is not another production command parser. It checks the known test inputs
against actual application reports, while the existing pixel/matrix gates remain.
"""
import math
SOURCE='upstream/sdl/macrolib.cpp + library.cpp + function.cpp + exparse: original TEMacroLib parser and TLibrary macro dispatch -> explicit pc.* view adapters'
BASE=dict(meshKind='sphere',meshGeneration=3,wireframe=False,colorPreset=0,materialPreset=0,yaw=32,pitch=-22,distance=4.4)
# name, literal UI input, dispatched callbacks, accepted, expected cumulative edits
SPECS=(
 ('desktop_command_help','pc.help',1,True,{}),
 ('desktop_command_mesh',"PC.MESH 'torus'",1,True,dict(meshKind='torus')),
 ('desktop_command_detail','pc.detail 2',1,True,dict(meshGeneration=2)),
 ('desktop_command_wire','pc.wire on',1,True,dict(wireframe=True)),
 ('desktop_command_color','pc.color amber',1,True,dict(colorPreset=1)),
 ('desktop_command_material','pc.material flat',1,True,dict(materialPreset=2)),
 ('desktop_command_view','pc.view 41 -18 4.8',1,True,dict(yaw=41,pitch=-18,distance=4.8)),
 ('desktop_command_chain','pc.mesh disks >> pc.detail 1 >> pc.wire off >> pc.color sky >> pc.material classic',5,True,dict(meshKind='disks',meshGeneration=1,wireframe=False,colorPreset=2,materialPreset=0)),
 ('desktop_command_unknown_rejected','pc.missing',0,False,{}),
 ('desktop_command_bad_argument_rejected','pc.detail 9',1,False,{}),
 ('desktop_command_failed_chain_unchanged','pc.reset >> pc.detail 9',2,False,{}),
 ('desktop_command_reset','pc.reset',1,True,dict(yaw=32,pitch=-22,distance=4.4)),
 ('desktop_command_enter_submit','pc.wire toggle',1,True,dict(wireframe=True)),
)
SAMPLES={};state=dict(BASE)
for seq,(name,text,count,accepted,edits) in enumerate(SPECS,1):
 state.update(edits);SAMPLES[name]=dict(sequence=seq,input=text,dispatched=count,passed=accepted,changed=accepted and name!='desktop_command_help',graphics=dict(state))


def expected_view_matches(graphics,expected):
 if not isinstance(graphics,dict):return False
 for key,value in expected.items():
  actual=graphics.get(key)
  if key in ('yaw','pitch','distance'):
   if type(actual) not in (int,float) or not math.isfinite(actual) or abs(actual-value)>.0001:return False
  elif type(actual) is not type(value) or actual!=value:return False
 return True


def command_sample_matches(name,report):
 if name not in SAMPLES or not isinstance(report,dict):return False
 expected=SAMPLES[name];command=report.get('desktopCommand')
 if not isinstance(command,dict) or command.get('source')!=SOURCE:return False
 for key in ('sequence','input','dispatched','passed','changed'):
  if type(command.get(key)) is not type(expected[key]) or command[key]!=expected[key]:return False
 if type(command.get('applied')) is not bool or command['applied']!=expected['changed']:return False
 if not isinstance(command.get('error'),str) or bool(command['error'])==expected['passed']:return False
 if not isinstance(command.get('message'),str):return False
 if expected['passed'] and not command['message']:return False
 if name=='desktop_command_help' and 'not the complete Olex2 command library' not in command['message']:return False
 return expected_view_matches(report.get('graphics'),expected['graphics'])


def command_recovery_matches(app):
 try:
  checks={c['name']:c for c in app['checks']};before=app['desktop_command_state_before_restart'];after=app['desktop_command_state_after_restart']
  names=('desktop_command_surface_recreated','desktop_command_survives_surface_recreation','desktop_command_state_saved','desktop_command_cold_restart_new_process','desktop_command_restored_after_restart')
  if not all(checks[k]['passed'] is True for k in names):return False
  pids=checks['desktop_command_cold_restart_new_process']['details'];a,b=pids['prior_pid'],pids['new_pid']
  if not isinstance(a,str) or not isinstance(b,str) or not a.isdigit() or not b.isdigit() or a==b:return False
  if before['workspaceState']['saveStatus']!='saved' or before['workspaceState']['schemaVersion']!=4 or after['workspaceState']['restored'] is not True:return False
  surface=checks['desktop_command_survives_surface_recreation']['details']
  if not command_sample_matches('desktop_command_enter_submit',before) or not command_sample_matches('desktop_command_enter_submit',surface):return False
  if surface['graphics']['materialCanonical']!=before['graphics']['materialCanonical']:return False
  if not expected_view_matches(after['graphics'],SAMPLES['desktop_command_enter_submit']['graphics']):return False
  if before['graphics']['materialCanonical']!=after['graphics']['materialCanonical']:return False
  # View preferences are restored, not command history or a falsely replayed
  # command. A fresh process must have no previous command execution record.
  fresh=after['desktopCommand']
  expected_fresh=dict(sequence=0,input='',passed=False,changed=False,applied=False,dispatched=0,source='',message='',error='')
  return all(type(fresh.get(key)) is type(value) and fresh[key]==value for key,value in expected_fresh.items())
 except (KeyError,TypeError,AttributeError):return False
