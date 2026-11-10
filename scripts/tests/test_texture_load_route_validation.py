"""Isolated mixed-route ordinal models: not actual owner/GPU observations."""
import copy
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from persistent_gx_window_validation import SOURCE,validate_lifetime
from texture_load_window_validation import IDENTITY as ORIGINAL_IDENTITY
from texture_load_route_validation import IDENTITY,action_plan,expected_counters,validate_lifetime_mixed

class MixedRouteProfileTests(unittest.TestCase):
    def proof(self,lease):
        return dict(unique_owner=True,owner_unchanged=True,scene_disabled=True,scene_unbound=True,
            cleanup_before_gl4es_close=True,scene_destroyed=0,font_requests=0,cleanup_failed=False,
            binding_before=lease-1,binding_after=lease,detachment_before=lease-1,detachment_after=lease,
            owner_address=101,renderer_address=102,scene_address=103)
    def snapshots(self):
        return [dict(applicationRuntime=dict(source=SOURCE)) for _ in range(2)]
    def qualified(self,proof,run,lease,previous=None):
        checks,addresses=validate_lifetime_mixed(proof,'test-pid',run,self.snapshots(),lease,previous)
        return all(c['passed'] for c in checks),checks,addresses
    def test_candidate16_profile_does_not_mutate_candidate15(self):
        self.assertEqual((IDENTITY['versionName'],IDENTITY['versionCode']),('0.22.16',221600))
        self.assertEqual((ORIGINAL_IDENTITY['versionName'],ORIGINAL_IDENTITY['versionCode']),('0.22.15',221500))
    def test_load_first_exact_plan(self):
        self.assertEqual(action_plan('load'),('load','original','load','original','load','original'))
        self.assertEqual(expected_counters(action_plan('load')),[
            dict(route='load',run=1,global_lease=1),dict(route='original',run=1,global_lease=2),
            dict(route='load',run=2,global_lease=3),dict(route='original',run=2,global_lease=4),
            dict(route='load',run=3,global_lease=5),dict(route='original',run=3,global_lease=6)])
    def test_original_first_exact_plan(self):
        values=expected_counters(action_plan('original'))
        self.assertEqual([x['route'] for x in values],['original','load']*3)
        self.assertEqual([x['run'] for x in values],[1,1,2,2,3,3])
        self.assertEqual([x['global_lease'] for x in values],list(range(1,7)))
    def test_unknown_first_route_fails(self):
        with self.assertRaises(ValueError):action_plan('unknown')
    def test_unknown_planned_route_fails(self):
        with self.assertRaises(ValueError):expected_counters(['original','unknown'])
    def test_preserves_all_ten_gates_with_correct_global_lease(self):
        passed,checks,_=self.qualified(self.proof(2),1,2)
        self.assertTrue(passed,checks);self.assertEqual(len(checks),10)
        # The frozen single-route profile remains strict and unchanged.
        old,_=validate_lifetime(self.proof(2),'test-pid',1,self.snapshots())
        self.assertFalse(all(c['passed'] for c in old))
    def test_global_counter_cannot_be_substituted_with_route_counter(self):
        self.assertFalse(self.qualified(self.proof(2),1,1)[0])
    def test_different_global_lease_fails(self):
        self.assertFalse(self.qualified(self.proof(2),1,3)[0])
    def test_unbalanced_detachment_fails(self):
        proof=self.proof(2);proof['detachment_after']=1
        self.assertFalse(self.qualified(proof,1,2)[0])
    def test_premature_scene_destruction_fails(self):
        proof=self.proof(2);proof['scene_destroyed']=1
        self.assertFalse(self.qualified(proof,1,2)[0])
    def test_bound_scene_fails(self):
        proof=self.proof(2);proof['scene_unbound']=False
        self.assertFalse(self.qualified(proof,1,2)[0])
    def test_missing_cleanup_before_shutdown_fails(self):
        proof=self.proof(2);proof['cleanup_before_gl4es_close']=False
        self.assertFalse(self.qualified(proof,1,2)[0])
    def test_same_owners_across_route_local_sequence(self):
        first,_,addresses=self.qualified(self.proof(2),1,2)
        previous=dict(passed=first,pid='test-pid',run=1,addresses=addresses)
        passed,checks,_=self.qualified(self.proof(4),2,4,previous)
        self.assertTrue(passed,checks)
    def test_missing_intermediate_route_run_fails(self):
        _,_,addresses=self.qualified(self.proof(2),1,2)
        previous=dict(passed=True,pid='test-pid',run=1,addresses=addresses)
        self.assertFalse(self.qualified(self.proof(6),3,6,previous)[0])
    def test_different_owners_after_another_route_fails(self):
        _,_,addresses=self.qualified(self.proof(2),1,2)
        previous=dict(passed=True,pid='test-pid',run=1,addresses=addresses)
        proof=self.proof(4);proof['renderer_address']=104
        self.assertFalse(self.qualified(proof,2,4,previous)[0])

if __name__=='__main__':unittest.main()
