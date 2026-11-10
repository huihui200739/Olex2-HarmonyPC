"""Synthetic resource bits/format contracts only; not target font observations."""
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from olex_font_resource_fixtures import STYLES,POINTS,glyph_payload,make_archive,write_fixtures
from olex_font_resource_window_validation import IDENTITY,resource_metrics,resource_frame,validate_source
from vector_font_state_window_validation import IDENTITY as PRIOR_IDENTITY

class OlexFontResourceTests(unittest.TestCase):
 def bits(self):
  width,height=5,7;data=bytearray(width*height*32)
  for code in b'Ab09':
   for x,y in ((1,2),(3,5)):
    index=(code*height+y)*width+x;data[index//8]|=1<<(index%8)
  return data,width,height
 def case(self):
  projection=[0.0]*16;projection[0]=projection[5]=2/64;projection[10]=-1.;projection[12]=projection[13]=-1.;projection[15]=1.
  return dict(text='Ab09',fixed=False,origin=[32,32],projection=projection,modelview=[1. if i in (0,5,10,15) else 0. for i in range(16)])
 def test_exact23_guard_preserves_prior21(self):
  self.assertEqual(IDENTITY['versionName'],'0.22.23');self.assertEqual(PRIOR_IDENTITY['versionName'],'0.22.21')
 def test_lsb_row_major_metrics_are_independent(self):
  bits,w,h=self.bits();ref=resource_metrics(bits,w,h)
  self.assertEqual(ref['glyphs'][ord('A')],[1,2,3,5]);self.assertEqual(ref['masks'][ord('b')],[(1,2),(3,5)])
  self.assertEqual((ref['max_width'],ref['max_height'],ref['leftmost'],ref['char_offset'],ref['bitmap_height']),(2,5,1,2,6))
 def test_original_empty_glyph_fallback_is_preserved(self):
  bits,w,h=self.bits();ref=resource_metrics(bits,w,h);self.assertEqual(ref['glyphs'][32],[0,2,6,2])
 def test_original_bitmap_height_plus_one_not_native_raster_height(self):
  bits,w,h=self.bits();ref=resource_metrics(bits,w,h);frame,ink=resource_frame(ref,self.case(),64,64)
  self.assertEqual(ink,8)
  self.assertEqual(frame[(36*64+32)*4:(36*64+32)*4+4],b'\xff\x00\x00\xff')
  self.assertEqual(frame[(33*64+34)*4:(33*64+34)*4+4],b'\xff\x00\x00\xff')
  self.assertEqual(frame[(35*64+32)*4:(35*64+32)*4+4],b'\xff\xff\xff\xff')
 def test_incomplete_bit_array_rejected(self):
  bits,w,h=self.bits()
  with self.assertRaises(ValueError):resource_metrics(bits[:-1],w,h)
 def test_nonpositive_and_boolean_cell_dimensions_rejected(self):
  for w,h in ((0,7),(5,0),(True,7),(2049,7)):
   with self.assertRaises(ValueError):resource_metrics(b'',w,h)
 def test_blank_synthetic_geometry_not_accepted_as_display_proof(self):
  with self.assertRaises(ValueError):resource_metrics(bytes(5*7*32),5,7)
 def test_changed_integer_draw_origin_rejected(self):
  bits,w,h=self.bits();case=self.case();case['origin']=[33,32]
  with self.assertRaises(ValueError):resource_frame(resource_metrics(bits,w,h),case,64,64)
 def test_fixed_width_not_misrepresented_by_variable_fixture(self):
  bits,w,h=self.bits();case=self.case();case['fixed']=True
  with self.assertRaises(ValueError):resource_frame(resource_metrics(bits,w,h),case,64,64)
 def test_independent_zip_has_all_original_style_point_entries(self):
  for stored,streamed,absolute in ((False,False,True),(True,False,False),(False,True,True)):
   data=make_archive(stored=stored,streamed=streamed,absolute_offsets=absolute);self.assertTrue(data.startswith(b'ofnt'))
   with zipfile.ZipFile(io.BytesIO(data)) as archive:
    self.assertEqual(len(archive.namelist()),16)
    for style in STYLES:
     for points in POINTS:self.assertEqual(archive.read(style+str(points)),glyph_payload(points,style))
 def test_payload_dimensions_and_lsb_size(self):
  for style in STYLES:
   for points in POINTS:
    payload=glyph_payload(points,style);w,h=struct.unpack_from('<HH',payload);self.assertEqual(len(payload),4+w*h*32)
 def test_fixture_write_is_exclusive_and_labeled(self):
  with tempfile.TemporaryDirectory(prefix='isolated-font-fixture-') as temp:
   folder=Path(temp)/'fixtures';data=write_fixtures(folder)
   self.assertFalse(data['target_executed']);self.assertFalse(data['full_port_complete'])
   for name,row in data['files'].items():self.assertEqual(hashlib.sha256((folder/name).read_bytes()).hexdigest(),row['sha256'])
   with self.assertRaises(FileExistsError):write_fixtures(folder)
 def test_unknown_synthetic_styles_and_sizes_rejected(self):
  for points,style in ((19,'nr__'),(18,'unknown')):
   with self.assertRaises(ValueError):glyph_payload(points,style)
 def test_frozen23_actual_manifest_chain_is_current(self):
  # A local source-input assertion, never an emulator or complete-port claim.
  self.assertEqual(validate_source(ROOT/'out/gx-application-evaluation/source23'),ROOT/'out/gx-application-evaluation/source23')
 def test_resource_profile_does_not_relax_old_six_request_verifier(self):
  from original_vector_font_window_validation import text as prior_native_text
  self.assertIn("legacy['font_requests']==run*6",prior_native_text)
  from olex_font_resource_window_validation import _namespace
  self.assertIn("legacy['font_requests']==run*9",_namespace['text'])
 def test_source22_failed_call_and_source23_fix_remain_separate(self):
  name='native/original_resource_font_window_probe.cpp'
  a=(ROOT/'out/gx-application-evaluation/source22'/name).read_text();b=(ROOT/'out/gx-application-evaluation/source23'/name).read_text()
  old='font.DrawRasterText(vec3d(32,32,0),olxstr("Ab09"),state);'
  self.assertEqual(a.count(old),1);self.assertEqual(b,a.replace(old,'glRasterPos2i(32,32);font.DrawRasterText(olxstr("Ab09"),state);',1))

if __name__=='__main__':unittest.main()
