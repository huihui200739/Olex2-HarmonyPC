"""Isolated synthetic UI-name sequences; not application runtime evidence."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from persistent_gx_application_validation import ADDED,validate_structure,verify_tool_profile
class PersistentGXApplicationStructure(unittest.TestCase):
 def setUp(self):self.baseline=['synthetic-prior-'+str(i) for i in range(200)];self.names=self.baseline+sorted(ADDED)
 def test_preserves_all_200(self):validate_structure(self.names,self.baseline)
 def test_missing_original(self):
  with self.assertRaises(ValueError):validate_structure(self.names[1:],self.baseline)
 def test_original_reordered(self):
  self.names[1],self.names[2]=self.names[2],self.names[1]
  with self.assertRaises(ValueError):validate_structure(self.names,self.baseline)
 def test_original_renamed(self):
  self.names[1]='synthetic-renamed'
  with self.assertRaises(ValueError):validate_structure(self.names,self.baseline)
 def test_missing_or_renamed_lifetime_gate(self):
  self.names[-1]='synthetic-lifetime'
  with self.assertRaises(ValueError):validate_structure(self.names,self.baseline)
 def test_duplicate_gate(self):
  self.names[-1]=self.names[-2]
  with self.assertRaises(ValueError):validate_structure(self.names,self.baseline)
 def test_extra_gate(self):
  with self.assertRaises(ValueError):validate_structure(self.names+['extra'],self.baseline)
 def test_prior_baseline_count_is_fixed(self):
  with self.assertRaises(ValueError):validate_structure(self.names,self.baseline[:-1])
 def test_lifetime_positions_do_not_delete_prior_order(self):
  names=list(self.baseline)
  for index,name in enumerate(sorted(ADDED)):names.insert(index*20,name)
  validate_structure(names,self.baseline)
if __name__=='__main__':unittest.main()
