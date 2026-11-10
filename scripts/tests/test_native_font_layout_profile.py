"""Isolated exact18 identity profile; no target execution or real glyph fixtures."""
import json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
import native_font_window_validation as prior
import native_font_layout_window_validation as current
class NativeFontLayoutProfileTests(unittest.TestCase):
 def test_exact18_identity_and_prior17_unchanged(self):
  self.assertEqual(current.IDENTITY,dict(bundleName='com.example.olex2harmonypc',versionName='0.22.18',versionCode=221800))
  self.assertEqual(prior.IDENTITY['versionName'],'0.22.17')
 def test_prior17_profile_rejected_before_any_native_artifact(self):
  with tempfile.TemporaryDirectory() as name:
   folder=Path(name);(folder/'received-artifacts.json').write_text(json.dumps(dict(identity=prior.IDENTITY,run=1,files={})))
   with self.assertRaises(ValueError):current.validate(folder)
 def test_exact18_still_requires_original_real_evidence(self):
  with tempfile.TemporaryDirectory() as name:
   folder=Path(name);(folder/'received-artifacts.json').write_text(json.dumps(dict(identity=current.IDENTITY,run=1,files={})))
   with self.assertRaises(FileNotFoundError):current.validate(folder)
 def test_all_analytical_function_bytecode_and_constants_preserved(self):
  self.assertEqual(current.validate.__code__.co_code,prior.validate.__code__.co_code)
  self.assertEqual(current.validate.__code__.co_consts,prior.validate.__code__.co_consts)
if __name__=='__main__':unittest.main()
