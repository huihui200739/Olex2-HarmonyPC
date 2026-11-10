"""Synthetic driver-query shape guards only, not emulator observations."""
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from vector_font_state_window_validation import IDENTITY,validated_mask
import original_vector_font_window_validation as prior

class VectorFontStateProfileTests(unittest.TestCase):
 def gpu(self):return dict(color_write=[0]*4,color_write_raw_boolean=[0]*4,color_write_integer=[1]*4)
 def test_exact21_does_not_mutate_failed20_profile(self):
  self.assertEqual(IDENTITY['versionName'],'0.22.21');self.assertEqual(prior.IDENTITY['versionName'],'0.22.20')
 def test_legacy_boolean_is_not_falsified_to_match_integer(self):
  gpu=self.gpu();self.assertTrue(validated_mask(gpu));self.assertEqual(gpu['color_write'],[0]*4)
 def test_raw_boolean_true_cannot_override_false_integer_state(self):
  gpu=self.gpu();gpu['color_write_raw_boolean']=[1]*4;gpu['color_write_integer']=[0]*4;self.assertFalse(validated_mask(gpu))
 def test_missing_integer_state_rejected(self):
  gpu=self.gpu();del gpu['color_write_integer']
  with self.assertRaises(ValueError):validated_mask(gpu)
 def test_missing_legacy_boolean_rejected(self):
  gpu=self.gpu();del gpu['color_write']
  with self.assertRaises(ValueError):validated_mask(gpu)
 def test_missing_raw_boolean_rejected(self):
  gpu=self.gpu();del gpu['color_write_raw_boolean']
  with self.assertRaises(ValueError):validated_mask(gpu)
 def test_non_boolean_integer_values_rejected(self):
  gpu=self.gpu();gpu['color_write_integer'][0]=2
  with self.assertRaises(ValueError):validated_mask(gpu)
 def test_truncated_vector_rejected(self):
  gpu=self.gpu();gpu['color_write_integer']=[1]
  with self.assertRaises(ValueError):validated_mask(gpu)

if __name__=='__main__':unittest.main()
