"""Isolated font geometry/reference gates; synthetic inputs are not target observations."""
import copy
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import original_vector_font_window_validation as current
import native_font_raster_window_validation as prior

class OriginalVectorFontTests(unittest.TestCase):
 def table(self):
  table=[[-1]*112 for _ in range(95)]
  for row in table:row[:2]=[0,16]
  table[ord('A')-32][2:6]=[1,2,3,4]
  return table
 def source(self,table=None):
  table=self.table() if table is None else table
  return 'int gl_font_simplex[95][112] = {'+',\n'.join('/* synthetic glyph */'+','.join(map(str,row)) for row in table)+'};'
 def test_exact20_and_prior19_identity_preserved(self):
  self.assertEqual(current.IDENTITY['versionName'],'0.22.20');self.assertEqual(prior.IDENTITY['versionName'],'0.22.19')
 def test_parse_complete_synthetic_table(self):self.assertEqual(current.source_table(self.source()),self.table())
 def test_comments_do_not_become_coordinates(self):
  self.assertEqual(current.source_table(self.source().replace('synthetic glyph','Ascii 7000')),self.table())
 def test_missing_table_rejected(self):
  with self.assertRaises(ValueError):current.source_table('no original table')
 def test_short_table_rejected(self):
  with self.assertRaises(ValueError):current.source_table(self.source(self.table()[:-1]))
 def test_non_initializer_syntax_rejected(self):
  with self.assertRaises(ValueError):current.source_table(self.source().replace('0,16','name,16',1))
 def test_unpaired_current_table_sentinel_rejected(self):
  table=self.table();table[0][3]=1
  with self.assertRaises(ValueError):current.source_table(self.source(table))
 def test_original_negative_descender_coordinates_are_not_sentinels(self):
  table=self.table();table[0][2:6]=[1,-7,2,-2]
  self.assertEqual(current.source_table(self.source(table)),table)
 def test_y_minus_one_with_real_x_is_a_valid_stroke(self):
  table=self.table();table[0][2:6]=[1,-1,2,0]
  self.assertEqual(current.source_table(self.source(table)),table)
 def test_preserves_original_minus_one_metric_initialization(self):
  metrics=current.original_metrics(self.table());self.assertEqual(metrics['glyphs'][ord('A')],[-1,-1,3,4]);self.assertEqual(metrics['max_width'],4);self.assertEqual(metrics['max_height'],6)
 def test_space_uses_original_global_maximum_not_replaced_advance(self):
  metrics=current.original_metrics(self.table());self.assertEqual(metrics['glyphs'][32],[0,0,2,5]);self.assertEqual(metrics['char_offset'],0)
 def test_unimplemented_codepoints_remain_uninitialized_metrics(self):
  metrics=current.original_metrics(self.table());self.assertEqual(metrics['glyphs'][200],[-1,-1,-1,-1])
 def test_actual_window_strict_red_white(self):
  self.assertEqual(current.colored_pixels(b'\xff\xff\xff\xff'+b'\xff\0\0\xff',2,1),1)
 def test_no_pixel_tolerance_added(self):
  with self.assertRaises(ValueError):current.colored_pixels(b'\xfe\0\0\xff',1,1)
 def test_wrong_actual_byte_count_rejected(self):
  with self.assertRaises(ValueError):current.colored_pixels(b'\xff\0\0\xff',2,1)
 def test_requires_bound_frozen_source(self):
  with self.assertRaises(ValueError):current.validate(Path('/synthetic-not-actual'))
 def test_native_request_count_is_exact_six_not_omitted(self):
  self.assertIn("legacy['font_requests']==run*6",current.text);self.assertNotIn("legacy['font_requests']==run*3",current.text)

if __name__=='__main__':unittest.main()
