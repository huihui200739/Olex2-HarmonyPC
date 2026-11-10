"""Isolated three-route ordinal/guard tests; no target or genuine runtime evidence."""
import ast
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from persistent_gx_window_validation import SOURCE,validate_lifetime
from native_font_mixed_validation import IDENTITY,action_plan,expected_counters,validate_lifetime_mixed

class NativeFontMixedTests(unittest.TestCase):
 def proof(self,lease,requests):
  return dict(unique_owner=True,owner_unchanged=True,scene_disabled=True,scene_unbound=True,
   cleanup_before_gl4es_close=True,scene_destroyed=0,font_requests=requests,cleanup_failed=False,
   binding_before=lease-1,binding_after=lease,detachment_before=lease-1,detachment_after=lease,
   owner_address=101,renderer_address=102,scene_address=103)
 def snapshots(self):return [dict(applicationRuntime=dict(source=SOURCE)) for _ in range(2)]
 def check(self,proof,run,lease,requests,previous=None):
  result,_=validate_lifetime_mixed(proof,'synthetic-pid',run,self.snapshots(),lease,requests,previous)
  return result
 def test_exact19_identity(self):self.assertEqual(IDENTITY['versionName'],'0.22.19')
 def test_all_three_independent_counters_and_global_leases(self):
  for first in ('original','load'):
   plan=expected_counters(action_plan(first));self.assertEqual(len(plan),9)
   self.assertEqual([x['global_lease'] for x in plan],list(range(1,10)))
   for route in ('original','load','font'):self.assertEqual([x['run'] for x in plan if x['route']==route],[1,2,3])
 def test_both_final_actions_close_exact_font_request_count(self):
  for first in ('original','load'):
   plan=expected_counters(action_plan(first));self.assertEqual(plan[-1]['route'],'font');self.assertEqual(plan[-1]['expected_font_requests'],9)
 def test_all_six_directed_transition_pairs_covered(self):
  pairs=set()
  for first in ('original','load'):
   plan=action_plan(first);pairs.update(zip(plan,plan[1:]))
  self.assertEqual(pairs,{(a,b) for a in ('font','original','load') for b in ('font','original','load') if a!=b})
 def test_exact_cumulative_counts_not_zero_substitution(self):
  plan=expected_counters(action_plan('original'))
  self.assertEqual([x['expected_font_requests'] for x in plan],[0,0,3,3,3,6,6,6,9])
 def test_same_ten_lifetime_gates_with_exact_requests(self):
  checked=self.check(self.proof(4,3),1,4,3);self.assertEqual(len(checked),10);self.assertTrue(all(x['passed'] for x in checked),checked)
 def test_missing_requests_not_waived(self):self.assertFalse(all(x['passed'] for x in self.check(self.proof(4,0),1,4,3)))
 def test_extra_requests_not_waived(self):self.assertFalse(all(x['passed'] for x in self.check(self.proof(4,6),1,4,3)))
 def test_global_lease_not_replaced_with_local_counter(self):self.assertFalse(all(x['passed'] for x in self.check(self.proof(4,3),1,1,3)))
 def test_original_single_route_zero_guard_unchanged(self):
  result,_=validate_lifetime(self.proof(1,3),'synthetic-pid',1,self.snapshots());self.assertFalse(all(x['passed'] for x in result))
 def test_unknown_route_rejected(self):
  with self.assertRaises(ValueError):action_plan('font')
  with self.assertRaises(ValueError):expected_counters(['unknown'])
 def test_three_route_runner_parses_without_execution(self):ast.parse((ROOT/'scripts/run-native-font-mixed-smoke.py').read_text())

if __name__=='__main__':unittest.main()
