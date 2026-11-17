"""Isolated synthetic path selection; no HDC, app, build or device operation."""
import ast
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
class DeviceBuildDirectoryTests(unittest.TestCase):
 def setUp(self):
  self.owned=tempfile.TemporaryDirectory(prefix='synthetic-build-selection-',dir=ROOT/'out');self.addCleanup(self.owned.cleanup)
  self.root=Path(self.owned.name);(self.root/'out/chosen').mkdir(parents=True)
  tree=ast.parse((ROOT/'scripts/run-device-smoke.py').read_text())
  function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='select_build_directory')
  context={'ROOT':self.root,'Path':Path}
  exec(compile(ast.fix_missing_locations(ast.Module(body=[function],type_ignores=[])),'actual-device-build-selector','exec'),context)
  self.select=context['select_build_directory']
 def test_explicit_relative_existing_build(self):
  self.assertEqual(self.select(Path('out/chosen')),self.root/'out/chosen')
 def test_explicit_absolute_existing_build(self):
  self.assertEqual(self.select(self.root/'out/chosen'),self.root/'out/chosen')
 def test_missing_build_rejected(self):
  with self.assertRaises(ValueError):self.select(Path('out/missing'))
 def test_existing_outside_build_tree_rejected(self):
  (self.root/'unrelated').mkdir()
  with self.assertRaises(ValueError):self.select(self.root/'unrelated')
 def test_symlink_escape_rejected(self):
  (self.root/'unrelated').mkdir();(self.root/'out/escape').symlink_to(self.root/'unrelated',target_is_directory=True)
  with self.assertRaises(ValueError):self.select(Path('out/escape'))
if __name__=='__main__':unittest.main()
