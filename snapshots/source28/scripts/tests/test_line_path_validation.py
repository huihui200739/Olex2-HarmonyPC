"""Explicitly synthetic isolated topology/pixel records, never real app results."""
import copy
import hashlib
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from line_path_validation import SOURCE,path_probe_matches,inactive_path_proof_cleared,app_path_evidence_matches
class OriginalLinePathValidationTests(unittest.TestCase):
 def fixture(self):
  out={'synthetic':True,'linePathProbe':dict(verified=True,stateRestored=True,contextRestored=True,source=SOURCE,error='',cases=[])}
  regions=[('bottom',32,8),('right',56,32),('top',32,56),('closing_left',8,32),('interior',32,32),('outside',2,32),('outside_closing_band',5,32),('inside_closing_band',11,32)]
  for kind,edges in [('strip',[0,1,1,2,2,3]),('loop',[0,1,1,2,2,3,3,0])]:
   c=dict(kind=kind,indexSha256=hashlib.sha256(struct.pack('<'+'I'*len(edges),*edges)).hexdigest(),inputVertices=4,indexCount=len(edges),actualLineWidth=1,actualBufferVerified=True,pixelsVerified=True,actualIndices=edges,samples=[])
   for name,x,y in regions:
    lit=name in ['bottom','right','top'] or (name=='closing_left' and kind=='loop');rgba=[0,0,0,255]*9
    if lit:rgba[16:20]=[68,136,204,255]
    c['samples'].append(dict(name=name,x=x,y=y,expectedLit=lit,passed=True,rgba=rgba))
   out['linePathProbe']['cases'].append(c)
  return out
 def test_synthetic_two_distinct_paths_pass(self):self.assertTrue(path_probe_matches(self.fixture()))
 def test_strip_must_not_close(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['actualIndices'] += [3,0];self.assertFalse(path_probe_matches(g))
 def test_loop_must_close_last_to_first(self):
  g=self.fixture();g['linePathProbe']['cases'][1]['actualIndices'][-2:]=[0,3];self.assertFalse(path_probe_matches(g))
 def test_actual_index_digest_required(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['indexSha256']='0'*64;self.assertFalse(path_probe_matches(g))
 def test_bool_index_rejected(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['actualIndices'][1]=True;self.assertFalse(path_probe_matches(g))
 def test_declared_topology_count_must_match(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['indexCount']=8;self.assertFalse(path_probe_matches(g))
 def test_missing_expected_line_pixel_rejected(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['rgba']=[0,0,0,255]*9;self.assertFalse(path_probe_matches(g))
 def test_filled_interior_rejected(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][4]['rgba']=[68,136,204,255]*9;self.assertFalse(path_probe_matches(g))
 def test_loop_closing_pixel_required(self):
  g=self.fixture();g['linePathProbe']['cases'][1]['samples'][3]['rgba']=[0,0,0,255]*9;self.assertFalse(path_probe_matches(g))
 def test_strip_closing_pixel_rejected(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][3]['rgba']=[68,136,204,255]*9;self.assertFalse(path_probe_matches(g))
 def test_unknown_pixel_color_rejected_even_in_lit_region(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['rgba'][:4]=[255,0,0,255];self.assertFalse(path_probe_matches(g))
 def test_declared_pixel_expectation_cannot_override_reference(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][4]['expectedLit']=True;self.assertFalse(path_probe_matches(g))
 def test_sample_coordinate_must_match_fixed_region(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['x']=31;self.assertFalse(path_probe_matches(g))
 def test_readback_length_required(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['rgba'].pop();self.assertFalse(path_probe_matches(g))
 def test_bool_pixel_rejected(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['rgba'][0]=False;self.assertFalse(path_probe_matches(g))
 def test_alpha_must_be_opaque(self):
  g=self.fixture();g['linePathProbe']['cases'][0]['samples'][0]['rgba'][3]=0;self.assertFalse(path_probe_matches(g))
 def test_width_one_only(self):
  for value in [True,2,0,float('nan'),float('inf')]:
   g=self.fixture();g['linePathProbe']['cases'][0]['actualLineWidth']=value;self.assertFalse(path_probe_matches(g))
 def test_all_gpu_state_and_context_flags_required(self):
  for key in ['verified','stateRestored','contextRestored']:
   g=self.fixture();g['linePathProbe'][key]=False;self.assertFalse(path_probe_matches(g))
 def test_exact_case_inventory_and_source_required(self):
  g=self.fixture();g['linePathProbe']['cases'].pop();self.assertFalse(path_probe_matches(g))
  g=self.fixture();g['linePathProbe']['source']='synthetic unsupported source';self.assertFalse(path_probe_matches(g))
 def test_inactive_has_no_current_gpu_proof(self):
  g=self.fixture();p=g['linePathProbe'];p.update(verified=False,stateRestored=False,contextRestored=False,cases=[]);self.assertTrue(inactive_path_proof_cleared(g))
  p['verified']=True;self.assertFalse(inactive_path_proof_cleared(g));self.assertFalse(path_probe_matches(g))
 def app_fixture(self):
  g=self.fixture();g.update(meshKind='synthetic',wireframe=False,surfaceReady=True,initialized=True)
  return dict(synthetic=True,result='passed: isolated synthetic fixture only',checks=[dict(name='original_line_path_gpu_probe',passed=True,details=g)])
 def inactive_fixture(self):
  g=self.fixture();g.update(meshKind='synthetic',wireframe=False,surfaceReady=False,initialized=False);g['linePathProbe'].update(verified=False,stateRestored=False,contextRestored=False,cases=[]);return g
 def test_failed_app_result_rejected(self):
  a=self.app_fixture();a['result']='failed: isolated synthetic';self.assertFalse(app_path_evidence_matches(a))
 def test_malformed_app_result_rejected(self):
  for value in [None,False,{},42]:
   a=self.app_fixture();a['result']=value;self.assertFalse(app_path_evidence_matches(a))
 def test_duplicate_named_probe_rejected(self):
  a=self.app_fixture();a['checks'].append(copy.deepcopy(a['checks'][0]));self.assertFalse(app_path_evidence_matches(a))
 def test_failed_named_probe_rejected(self):
  a=self.app_fixture();a['checks'][0]['passed']=False;self.assertFalse(app_path_evidence_matches(a))
 def test_every_active_graphic_snapshot_required(self):
  a=self.app_fixture();g=copy.deepcopy(a['checks'][0]['details']);g['linePathProbe']['verified']=False;a['other_state']={'graphics':g};self.assertFalse(app_path_evidence_matches(a))
 def test_inactive_non_document_page_rejected(self):
  a=self.app_fixture();a['other_state']={'graphics':self.inactive_fixture()};self.assertFalse(app_path_evidence_matches(a))
 def test_document_root_and_named_check_inactive_allowed(self):
  a=self.app_fixture();a['options_document_exported']={'graphics':self.inactive_fixture()};a['checks'].append(dict(name='options_document_exported',passed=True,details={'graphics':self.inactive_fixture()}));self.assertTrue(app_path_evidence_matches(a))
 def test_mixed_surface_lifecycle_rejected(self):
  a=self.app_fixture();g=self.inactive_fixture();g['surfaceReady']=True;a['options_document_exported']={'graphics':g};self.assertFalse(app_path_evidence_matches(a))
if __name__=='__main__':unittest.main()
