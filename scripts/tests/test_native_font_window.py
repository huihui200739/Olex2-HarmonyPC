"""Isolated synthetic pixel math only; no NativeDrawing, emulator or real font."""
import sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from native_font_window_validation import inspect_cells,expected_frame
class NativeFontPixelMathTests(unittest.TestCase):
 def raw(self):
  raw=bytearray([255]*(4*5*3*256))
  for code in b'Ab09':
   for x,y in ((1,1),(2,1),(2,2)):
    i=(code*4*5+y*4+x)*3;raw[i:i+3]=b'\x00\x00\x00'
  return bytes(raw)
 def case(self):return dict(text='Ab09',origin=[32,32],fixed=False)
 def test_all256_source_metrics_and_blank_spacing(self):
  ref=inspect_cells(self.raw(),4,5)
  self.assertEqual((ref['max_width'],ref['max_height'],ref['leftmost'],ref['char_offset']),(1,2,1,2))
  self.assertEqual(ref['metrics'][65],[1,1,2,2]);self.assertEqual(ref['metrics'][32],[0,1,8,1]);self.assertEqual(len(ref['metrics']),256)
 def test_binary_ink_layout_and_advance(self):
  ref=inspect_cells(self.raw(),4,5);pixels,ink=expected_frame(ref,self.case(),128,64)
  self.assertEqual(ink,12);self.assertEqual(len(pixels),128*64*4)
  for cursor in (0,4,8,12):
   for x,y in ((32+cursor,33),(33+cursor,33),(33+cursor,32)):
    i=(y*128+x)*4;self.assertEqual(pixels[i:i+4],b'\xff\x00\x00\xff')
  self.assertEqual(pixels[:4],b'\xff\xff\xff\xff')
 def test_cell_input_not_mutated(self):
  raw=self.raw();before=bytes(raw);inspect_cells(raw,4,5);self.assertEqual(raw,before)
 def test_incorrect_source_byte_count_rejected(self):
  with self.assertRaises(ValueError):inspect_cells(self.raw()[:-1],4,5)
 def test_non_integer_dimensions_rejected(self):
  with self.assertRaises(ValueError):inspect_cells(self.raw(),4.0,5)
 def test_oversized_dimensions_rejected_without_allocation(self):
  with self.assertRaises(ValueError):inspect_cells(b'',4097,1)
 def test_all_blank_native_font_fails(self):
  with self.assertRaises(ValueError):inspect_cells(bytes([255])*(4*5*3*256),4,5)
 def test_unexpected_draw_origin_rejected(self):
  case=self.case();case['origin']=[0,0]
  with self.assertRaises(ValueError):expected_frame(inspect_cells(self.raw(),4,5),case,128,64)
 def test_clipped_reference_is_not_a_success(self):
  with self.assertRaises(ValueError):expected_frame(inspect_cells(self.raw(),4,5),self.case(),40,64)
 def test_fixed_spacing_not_silently_accepted(self):
  case=self.case();case['fixed']=True
  with self.assertRaises(ValueError):expected_frame(inspect_cells(self.raw(),4,5),case,128,64)
 def test_visible_blank_glyph_is_not_a_success(self):
  ref=inspect_cells(self.raw(),4,5);ref['masks'][65]=[]
  with self.assertRaises(ValueError):expected_frame(ref,self.case(),128,64)
if __name__=='__main__':unittest.main()
