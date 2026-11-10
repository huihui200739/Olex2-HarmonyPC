"""Isolated synthetic observations; these do not qualify any real process."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from persistent_gx_window_validation import SOURCE,ROOT,validate_lifetime,original_profile,replace_once
class PersistentGXValidation(unittest.TestCase):
 def setUp(self):
  self.proof=dict(unique_owner=True,owner_unchanged=True,scene_disabled=True,scene_unbound=True,cleanup_before_gl4es_close=True,scene_destroyed=0,font_requests=0,cleanup_failed=False,binding_before=0,binding_after=1,detachment_before=0,detachment_after=1,owner_address=100,renderer_address=200,scene_address=300)
  self.snapshots=[{'applicationRuntime':{'source':SOURCE}} for _ in range(2)]
 def valid(self,proof=None,run=1,previous=None,snapshots=None):
  c,_=validate_lifetime(self.proof if proof is None else proof,'123',run,self.snapshots if snapshots is None else snapshots,previous)
  self.assertEqual(len(c),10);return all(r['passed'] for r in c)
 def test_initial(self):self.assertTrue(self.valid())
 def test_missing_fields(self):
  for key in self.proof:
   with self.subTest(key=key):
    p=copy.deepcopy(self.proof);del p[key];self.assertFalse(self.valid(p))
 def test_false_owner_or_bound_scene(self):
  for key in ('unique_owner','owner_unchanged','scene_disabled','scene_unbound','cleanup_before_gl4es_close'):
   with self.subTest(key=key):
    p=copy.deepcopy(self.proof);p[key]=False;self.assertFalse(self.valid(p))
 def test_integer_is_not_boolean(self):
  for key in ('unique_owner','owner_unchanged','scene_disabled','scene_unbound','cleanup_before_gl4es_close'):
   p=copy.deepcopy(self.proof);p[key]=1;self.assertFalse(self.valid(p))
 def test_destroyed_or_font_claim(self):
  for key in ('scene_destroyed','font_requests'):
   for value in (1,False,'0',-1):
    with self.subTest(key=key,value=value):
     p=copy.deepcopy(self.proof);p[key]=value;self.assertFalse(self.valid(p))
 def test_cleanup_failure(self):
  p=copy.deepcopy(self.proof);p['cleanup_failed']=True;self.assertFalse(self.valid(p))
 def test_lease_counts(self):
  for key in ('binding_before','binding_after','detachment_before','detachment_after'):
   for value in (False,None,-1,1.0,3):
    with self.subTest(key=key,value=value):
     p=copy.deepcopy(self.proof);p[key]=value;self.assertFalse(self.valid(p))
 def test_run_ordinal_required(self):
  for run in (False,True,0,2,'1'):self.assertFalse(self.valid(run=run))
 def test_distinct_real_addresses(self):
  for key in ('owner_address','renderer_address','scene_address'):
   for value in (0,True,'100',None,-1):
    p=copy.deepcopy(self.proof);p[key]=value;self.assertFalse(self.valid(p))
  p=copy.deepcopy(self.proof);p['renderer_address']=100;self.assertFalse(self.valid(p))
 def test_actual_source_each_side(self):
  for index in (0,1):
   snapshots=copy.deepcopy(self.snapshots);snapshots[index]['applicationRuntime']['source']='TXApp'
   self.assertFalse(self.valid(snapshots=snapshots))
 def test_consecutive_owners(self):
  p=copy.deepcopy(self.proof);p.update(binding_before=1,binding_after=2,detachment_before=1,detachment_after=2)
  previous=dict(passed=True,pid='123',run=1,addresses={k:p[k] for k in ('owner_address','renderer_address','scene_address')})
  self.assertTrue(self.valid(p,2,previous))
  for key,value in [('passed',False),('pid','124'),('run',0),('addresses',{})]:
   old=copy.deepcopy(previous);old[key]=value;self.assertFalse(self.valid(p,2,old))
 def test_replayed_initial_or_absent_previous(self):
  self.assertFalse(self.valid(previous=dict(passed=True,pid='123',run=0,addresses={})))
 def test_profile_exact_changes(self):
  original=(ROOT/'scripts/original_renderer_validation.py').read_text()
  expected=original.replace('len(adapted)==2','len(adapted)==6').replace("r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38","r.get('originalCoreMacroCount')==193 and r.get('originalCoreFunctionCount')==46").replace("c.get(key)==1,c.get(key))","c.get(key)==(0 if key=='scene_destroyed' else 1),c.get(key))")
  self.assertEqual(original_profile(),expected)
 def test_profile_duplicate_or_missing_fails(self):
  for value in ('','x x'):
   with self.assertRaises(ValueError):replace_once(value,'x','y')
if __name__=='__main__':unittest.main()
