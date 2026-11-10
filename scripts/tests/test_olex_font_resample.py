"""Isolated synthetic/source-input tests only; these are not device observations."""
import hashlib,importlib.util,io,json,struct,sys,tempfile,unittest,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from olex_font_resample_window_validation import IDENTITY,validate_source
from olex_font_resource_window_validation import IDENTITY as PRIOR_IDENTITY
from resource_font_resample_reference import CASES,inspect_resource_rgb,resource_rgb_frame
from olex_font_resample_fixtures import POINTS,STYLES,make_archive,glyph_payload,write_fixtures
from olex_font_resource_fixtures import glyph_payload as old_payload

class IsolatedResourceResamplingTests(unittest.TestCase):
 def cells(self):
  w,h=5,7;raw=bytearray(w*h*3*256)
  for code in b'Ab09':
   for x,y in ((1,2),(3,5)):raw[((code*h+y)*w+x)*3]=255
  return raw,w,h
 def case(self,fixed=False):
  projection=[0.0]*16;projection[0]=projection[5]=2/64;projection[10]=-1.;projection[12]=projection[13]=-1.;projection[15]=1.
  return dict(text='Ab09',fixed=fixed,origin=[32,32],projection=projection,modelview=[1. if i in (0,5,10,15) else 0. for i in range(16)])
 def test_exact24_keeps_previous23_identity(self):
  self.assertEqual(IDENTITY['versionName'],'0.22.24');self.assertEqual(PRIOR_IDENTITY['versionName'],'0.22.23')
 def test_variable_black_background_rgb_metrics(self):
  raw,w,h=self.cells();ref=inspect_resource_rgb(raw,w,h,False)
  self.assertEqual(ref['glyphs'][65],[1,2,3,5]);self.assertEqual((ref['max_width'],ref['max_height'],ref['leftmost'],ref['char_offset']),(2,5,1,2))
 def test_fixed_empty_glyph_uses_original_max_width(self):
  raw,w,h=self.cells();self.assertEqual(inspect_resource_rgb(raw,w,h,True)['glyphs'][32],[0,2,2,2])
 def test_variable_empty_glyph_uses_original_bounded_advance(self):
  raw,w,h=self.cells();self.assertEqual(inspect_resource_rgb(raw,w,h,False)['glyphs'][32],[0,2,8,2])
 def test_original_fixed_and_variable_text_width_differ(self):
  raw,w,h=self.cells();self.assertEqual(inspect_resource_rgb(raw,w,h,False)['text_width'],18);self.assertEqual(inspect_resource_rgb(raw,w,h,True)['text_width'],8)
 def test_rgb_bitmap_height_is_max_height_not_bitarray_plus_one(self):
  raw,w,h=self.cells();ref=inspect_resource_rgb(raw,w,h,False);frame,ink=resource_rgb_frame(ref,self.case(),64,64)
  self.assertEqual(ink,8);self.assertEqual(frame[(35*64+32)*4:(35*64+32)*4+4],b'\xff\x00\x00\xff')
  self.assertEqual(frame[(32*64+34)*4:(32*64+34)*4+4],b'\xff\x00\x00\xff')
 def test_fixed_advance_changes_actual_reference_layout(self):
  raw,w,h=self.cells();a,_=resource_rgb_frame(inspect_resource_rgb(raw,w,h,False),self.case(),64,64);b,_=resource_rgb_frame(inspect_resource_rgb(raw,w,h,True),self.case(True),64,64);self.assertNotEqual(a,b)
 def test_fixed_contract_cannot_be_relabelled(self):
  raw,w,h=self.cells()
  with self.assertRaises(ValueError):resource_rgb_frame(inspect_resource_rgb(raw,w,h,False),self.case(True),64,64)
 def test_incomplete_rgb_array_rejected(self):
  raw,w,h=self.cells()
  with self.assertRaises(ValueError):inspect_resource_rgb(raw[:-1],w,h,False)
 def test_empty_geometry_not_display_evidence(self):
  with self.assertRaises(ValueError):inspect_resource_rgb(bytes(5*7*3*256),5,7,False)
 def test_nonpositive_boolean_oversized_dimensions_rejected(self):
  for w,h in ((0,7),(5,0),(True,7),(2049,7)):
   with self.assertRaises(ValueError):inspect_resource_rgb(b'',w,h,False)
 def test_original_integer_origin_required(self):
  raw,w,h=self.cells();case=self.case();case['origin']=[33,32]
  with self.assertRaises(ValueError):resource_rgb_frame(inspect_resource_rgb(raw,w,h,False),case,64,64)
 def test_all32_synthetic_entries_are_independent_standard_zip(self):
  self.assertEqual(POINTS,(8,18,27,72))
  with zipfile.ZipFile(io.BytesIO(make_archive())) as archive:
   self.assertEqual(len(archive.namelist()),32)
   for style in STYLES:
    for points in POINTS:
     payload=archive.read(style+str(points));self.assertEqual(payload,glyph_payload(points,style));w,h=struct.unpack_from('<HH',payload);self.assertEqual(len(payload),4+w*h*32)
 def test_frozen18_27_synthetic_entry_bytes_unchanged(self):
  for style in STYLES:
   for points in (18,27):self.assertEqual(glyph_payload(points,style),old_payload(points,style))
 def test_new_fixture_output_is_separate_and_exclusive(self):
  with tempfile.TemporaryDirectory(prefix='isolated-resample-fixture-') as temporary:
   output=Path(temporary)/'new';d=write_fixtures(output);self.assertEqual(len(d['entries']),32);self.assertFalse(d['target_executed'])
   self.assertTrue((output/'olex_font_resample_fixture.h').is_file());self.assertEqual(set(d['files']),{'synthetic-resample-deflate.fnt','synthetic-resample-stored-relative.fnt','synthetic-resample-streamed.fnt'})
   with self.assertRaises(FileExistsError):write_fixtures(output)
 def test_six_explicit_resampling_cases_include_both_filters_and_identity(self):
  self.assertEqual([v[2] for v in CASES],[7,31,46,31,96,72]);self.assertEqual(CASES[4][1],'fib_')
 def test_exact24_merged_source_including_resample_flags(self):
  path=validate_source(ROOT/'out/gx-application-evaluation/source24');text=(path/'app/entry/src/main/cpp/CMakeLists.txt').read_text();self.assertIn('PROPERTIES COMPILE_OPTIONS "-ffp-contract=off"',text)
 def test_original_resource23_nine_request_guard_is_not_relaxed(self):
  from olex_font_resource_window_validation import _namespace
  self.assertIn("legacy['font_requests']==run*9",_namespace['text'])
  from olex_font_resample_window_validation import _namespace as current
  self.assertIn("legacy['font_requests']==run*15",current['_namespace']['text'])
 def test_fixed_reference_method_recipe_matches_exact_original_input(self):
  p=ROOT/'scripts/prepare-resource-resample-wx-reference.py';spec=importlib.util.spec_from_file_location('isolated_fixed_wx_recipe',p);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  source=ROOT/'out/gx-application-evaluation/wx-image-v3.2.6-reference.cpp';self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),module.EXPECTED)
  self.assertTrue(all(module.extract(source.read_text(),marker) for marker in module.MARKERS))
 def test_all6_fixed_reference_outputs_have_all256_bounded_cells(self):
  data=json.loads((ROOT/'out/gx-application-evaluation/font-resample-reference24/result.json').read_text());self.assertEqual(len(data['cases']),6)
  for case in data['cases']:
   raw=(ROOT/case['expected']['path']).read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),case['expected']['sha256']);self.assertEqual(len(raw),case['width']*case['height']*3*256)


 def test_new25_identity_does_not_rewrite_frozen24(self):
  from font_raster_flush_window_validation import IDENTITY as fixed,validate_source as validate25
  self.assertEqual(fixed['versionName'],'0.22.25');self.assertEqual(IDENTITY['versionName'],'0.22.24');validate25(ROOT/'out/gx-application-evaluation/source25')
 def test_platform_flush_is_minimal_glyph_body_change_only(self):
  a=(ROOT/'out/gx-application-evaluation/source24/upstream/glib/glfont.cpp').read_bytes();b=(ROOT/'out/gx-application-evaluation/source25/upstream/glib/glfont.cpp').read_bytes();eol=b'\r\n' if b'\r\n' in a else b'\n'
  block=eol.join([b'#ifdef OLEX2_PLATFORM_RASTER_GLYPH_FLUSH',b'  // Retire this bitmap batch before the next glyph can overlap its padding.',b'  // This preserves transparent zero bits with the fixed GL4ES raster backend.',b'  olx_gl::flush();',b'#endif',b''])
  self.assertEqual(b.count(block),1);self.assertEqual(b.replace(block,b'',1),a)
  self.assertIn('OLEX2_PLATFORM_RASTER_GLYPH_FLUSH=1',(ROOT/'out/gx-application-evaluation/source25/app/entry/src/main/cpp/CMakeLists.txt').read_text())
 def test_original95_vector_table_is_unmodified_by_flush(self):
  from original_vector_font_window_validation import source_table
  a=(ROOT/'out/gx-application-evaluation/source24/upstream/glib/glfont.cpp').read_text();b=(ROOT/'out/gx-application-evaluation/source25/upstream/glib/glfont.cpp').read_text();self.assertEqual(source_table(a),source_table(b))
 def test_shader_stage_filter_distinguishes_large_scaled_and_restored(self):
  import re
  names=['actual-shader-run-1-resample-large-'+value+'.txt' for value in ('0','1','scaled-0','scaled-1','restored-0','restored-1')]
  pattern=r'actual-shader-run-1-'+re.escape('resample-large')+r'-[0-9]+[.]txt'
  self.assertEqual([name for name in names if re.fullmatch(pattern,name)],names[:2])

if __name__=='__main__':unittest.main()
