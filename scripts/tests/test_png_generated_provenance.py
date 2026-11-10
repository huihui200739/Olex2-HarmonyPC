"""Isolated synthetic validation records, not observations of an actual build."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from build_provenance_png import GENERATED_PNG_HEADERS,generated_header_spec,validate_generated_headers
class PngGeneratedProvenanceTests(unittest.TestCase):
 def setUp(self):
  self.files={name:dict(path=name,bytes=3,sha256='1'*64) for _,recipes in GENERATED_PNG_HEADERS.values() for name in recipes}
  self.records=[]
  for suffix in sorted(GENERATED_PNG_HEADERS):
   name='app/entry/.cxx/default/default/debug/arm64-v8a/'+suffix;generator,recipes=generated_header_spec(name,self.files)
   self.records.append(dict(path=name,bytes=10,sha256='2'*64,generator=generator,recipe_inputs=recipes))
 def test_legacy_no_generated_inputs(self):self.assertEqual(validate_generated_headers([],{}),{})
 def test_explicit_fixed_headers(self):self.assertEqual(len(validate_generated_headers(self.records,self.files)),2)
 def test_source_root_not_generated(self):
  with self.assertRaises(ValueError):generated_header_spec('native/pnglibconf.h',self.files)
 def test_unknown_header_not_ignored(self):
  with self.assertRaises(ValueError):generated_header_spec('app/entry/.cxx/default/debug/arm64-v8a/unknown/other.h',self.files)
 def test_incomplete_pair(self):
  with self.assertRaises(ValueError):validate_generated_headers(self.records[:1],self.files)
 def test_missing_recipe(self):
  self.files.pop('cmake/Olex2PinnedPng.cmake')
  with self.assertRaises(ValueError):validate_generated_headers(self.records,self.files)
 def test_recipe_bytes_changed(self):
  self.files['vendor/zlib/zconf.h']['sha256']='3'*64
  with self.assertRaises(ValueError):validate_generated_headers(self.records,self.files)
 def test_wrong_generator(self):
  self.records[0]['generator']='untracked'
  with self.assertRaises(ValueError):validate_generated_headers(self.records,self.files)
 def test_no_parent_segments(self):
  with self.assertRaises(ValueError):generated_header_spec('app/entry/.cxx/default/../pinned-zlib/zconf.h',self.files)
if __name__=='__main__':unittest.main()
