"""Isolated float32 matrix/pixel model tests, not native font observations."""
import sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from native_font_raster_window_validation import f32,raster_position,expected_frame_with_raster,IDENTITY
from native_font_window_validation import inspect_cells,expected_frame
class NativeFontRasterProfileTests(unittest.TestCase):
 def case(self,w,h):
  projection=[0.0]*16;projection[0]=f32(2/w);projection[5]=f32(2/h);projection[10]=-1.;projection[12]=projection[13]=-1.;projection[15]=1.
  return dict(text='Ab09',fixed=False,origin=[32,32],projection=projection,modelview=[1.0 if i in (0,5,10,15) else 0.0 for i in range(16)])
 def source(self):
  raw=bytearray([255]*(4*5*3*256))
  for code in b'Ab09':
   for x,y in ((1,1),(2,1),(2,2)):
    i=(code*4*5+y*4+x)*3;raw[i:i+3]=b'\0\0\0'
  return inspect_cells(raw,4,5)
 def test_exact19_identity(self):self.assertEqual(IDENTITY['versionName'],'0.22.19')
 def test_fmla_reference_has_the_expected_float32_boundary(self):
  self.assertEqual(raster_position(self.case(934,272),934,272),[31.999984741210938,31.999998092651367])
 def test_power_of_two_viewport_has_no_empirical_shift(self):
  case=self.case(128,64);self.assertEqual(raster_position(case,128,64),[32.,32.])
  self.assertEqual(expected_frame_with_raster(self.source(),case,128,64),expected_frame(self.source(),case,128,64))
 def test_non_power_of_two_pixels_follow_projected_origin(self):
  pixels,ink=expected_frame_with_raster(self.source(),self.case(934,272),934,272);self.assertEqual(ink,12)
  for cursor in (0,4,8,12):
   i=((32*934)+31+cursor)*4;self.assertEqual(pixels[i:i+4],b'\xff\0\0\xff')
 def test_integer_input_case_was_not_replaced_by_pixel_centres(self):
  case=self.case(934,272);case['origin']=[32.5,32.5]
  with self.assertRaises(ValueError):raster_position(case,934,272)
 def test_tampered_projection_rejected(self):
  case=self.case(934,272);case['projection'][0]=.1
  with self.assertRaises(ValueError):raster_position(case,934,272)
 def test_translated_modelview_rejected(self):
  case=self.case(934,272);case['modelview'][12]=.1
  with self.assertRaises(ValueError):raster_position(case,934,272)
 def test_missing_actual_matrices_rejected(self):
  case=self.case(934,272);del case['projection']
  with self.assertRaises(ValueError):raster_position(case,934,272)
if __name__=='__main__':unittest.main()
