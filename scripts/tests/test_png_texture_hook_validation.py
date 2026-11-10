"""Recorded synthetic-image target observation and isolated validator mutations."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from gx_application_validation import validate_texture_hook
from png_texture_hook_validation import validate_png_texture_hook
FIXTURES=Path(__file__).with_name('fixtures')
class PngTextureHookValidationTests(unittest.TestCase):
 def setUp(self):
  self.hook=json.loads((FIXTURES/'gx_png_virtual_hook_observation.json').read_text())['texture_hook']
  self.fixtures=json.loads((FIXTURES/'gx_synthetic_images.json').read_text())['fixtures']
 def validate(self):return validate_png_texture_hook(self.hook,self.fixtures,require_resources=True,resource_proof='owned-memory')
 def reject(self,change):
  change(self.hook)
  with self.assertRaises(ValueError):self.validate()
 def test_recorded_actual_virtual_seam(self):
  v=self.validate();self.assertEqual(v['decoded_rgb_images'],15);self.assertEqual(v['transparent_hidden_rgb_images'],3);self.assertFalse(v['original_texture_gl_upload_tested']);self.assertFalse(v['complete_application_backend'])
 def test_legacy_imagekit_validation_cannot_accept_png(self):
  with self.assertRaises(ValueError):validate_texture_hook(self.hook,self.fixtures,require_resources=True,resource_proof='owned-memory')
 def test_missing_explicit_backend(self):self.reject(lambda v:v.pop('backend'))
 def test_old_opaque_backend_identity(self):self.reject(lambda v:v.update(backend='imagekit-opaque'))
 def test_no_process_count_weakening(self):self.reject(lambda v:v['cycles'][0].update(fd_after=v['cycles'][0]['fd_before']+1))
 def test_lost_hidden_rgb(self):self.reject(lambda v:v['cycles'][0]['images'][1]['rgb'].__setitem__(0,0))
 def test_transparency_is_not_rejected(self):self.reject(lambda v:v['cycles'][0]['images'][1].update(status='unsupported-transparency',accepted=False))
 def test_premultiplied_layout_not_accepted(self):self.reject(lambda v:v['cycles'][0]['images'][0].update(format=3,stride=8))
 def test_callbacks_must_release(self):self.reject(lambda v:v['cycles'][0]['images'][0]['png_decoder'].update(releases=0))
 def test_no_sdk_handle_claim(self):self.reject(lambda v:v['cycles'][0]['images'][0]['resource_audit'].update(handles=[dict(name='source')]))
 def test_dimension_guard_stays(self):self.reject(lambda v:v['cycles'][0]['images'][3].update(status='decoded',accepted=True))
 def test_no_source_metadata_invention(self):self.reject(lambda v:v['cycles'][0]['images'][0]['png_decoder'].update(source_color_type=6))
 def test_owner_must_be_cleared(self):self.reject(lambda v:v['cycles'][0].update(singleton_cleared=0))
 def test_scene_must_be_destroyed(self):self.reject(lambda v:v['cycles'][0].update(scene_destroyed=0))
 def test_missing_cycle(self):self.reject(lambda v:v['cycles'].pop())
if __name__=='__main__':unittest.main()
