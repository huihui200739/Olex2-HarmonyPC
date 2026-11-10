"""Actual read-only observation baseline + labeled synthetic validator cases."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from gx_application_validation import validate_lifecycle,validate_image_observations,validate_texture_hook
HERE=Path(__file__).resolve().parent/'fixtures'
class GraphicsApplicationValidationTests(unittest.TestCase):
    def setUp(self):
        self.observation=json.loads((HERE/'gx_platform_observation.json').read_text())
        self.fixtures=json.loads((HERE/'gx_synthetic_images.json').read_text())['fixtures']
    def test_actual_lifecycle(self):
        self.assertEqual(validate_lifecycle(self.observation['lifecycle'])['cycles'],3)
    def test_actual_images_classified_not_full_feature(self):
        result=validate_image_observations(self.observation['images'],self.fixtures)
        self.assertEqual(result['opaque_color_matches'],4)
        self.assertFalse(result['native_rgb888_supported'])
        self.assertFalse(result['transparent_original_rgb_preserved'])
        self.assertFalse(result['complete_texture_backend'])
    def test_synthetic_missing_lifecycle_cycle_rejected(self):
        self.observation['lifecycle']['cycles'].pop()
        with self.assertRaises(ValueError):validate_lifecycle(self.observation['lifecycle'])
    def test_synthetic_false_singleton_rejected(self):
        self.observation['lifecycle']['cycles'][1]['singleton_cleared']=0
        with self.assertRaises(ValueError):validate_lifecycle(self.observation['lifecycle'])
    def test_synthetic_boolean_counter_rejected(self):
        self.observation['lifecycle']['cycles'][0]['scene_destroyed']=True
        with self.assertRaises(ValueError):validate_lifecycle(self.observation['lifecycle'])
    def test_synthetic_changed_registration_rejected(self):
        self.observation['lifecycle']['cycles'][0]['macro_count']=194
        with self.assertRaises(ValueError):validate_lifecycle(self.observation['lifecycle'])
    def test_synthetic_missing_observation_rejected(self):
        self.observation['images'].pop()
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_duplicate_observation_rejected(self):
        self.observation['images'].append(copy.deepcopy(self.observation['images'][0]))
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_opaque_color_change_rejected(self):
        self.observation['images'][1]['observation']['bytes'][0]^=1
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_invented_alpha_metadata_rejected(self):
        self.observation['images'][1]['observation']['alpha_type']=7
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_missing_row_bytes_rejected(self):
        self.observation['images'][1]['observation']['read_size']=4
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_unknown_rgb_failure_rejected(self):
        self.observation['images'][0]['observation']['error']='some error'
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)
    def test_synthetic_wrong_transparent_rgb_rejected(self):
        self.observation['images'][3]['observation']['bytes'][4]=241
        with self.assertRaises(ValueError):validate_image_observations(self.observation['images'],self.fixtures)

class TextureHookValidationTests(unittest.TestCase):
    def setUp(self):
        self.hook=json.loads((HERE/'gx_texture_hook_observation.json').read_text())
        self.fixtures=json.loads((HERE/'gx_synthetic_images.json').read_text())['fixtures']
    def test_actual_native_virtual_hook(self):
        result=validate_texture_hook(self.hook,self.fixtures)
        self.assertEqual(result['opaque_decodes'],12)
        self.assertEqual(result['explicit_unsupported_or_missing'],9)
        self.assertTrue(result['own_descriptor_counts_balanced'])
        self.assertTrue(result['unicode_filename_tested'])
        self.assertFalse(result['original_texture_gl_upload_tested'])
        self.assertFalse(result['complete_application_backend'])
    def assertSyntheticChangeRejected(self):
        with self.assertRaises(ValueError):validate_texture_hook(self.hook,self.fixtures)
    def test_synthetic_descriptor_change_rejected(self):
        self.hook['cycles'][0]['fd_after']+=1;self.assertSyntheticChangeRejected()
    def test_synthetic_singleton_not_released_rejected(self):
        self.hook['cycles'][0]['singleton_cleared']=0;self.assertSyntheticChangeRejected()
    def test_synthetic_transparency_silently_accepted_rejected(self):
        image=self.hook['cycles'][0]['images'][1];image['accepted']=True;image['status']='decoded';self.assertSyntheticChangeRejected()
    def test_synthetic_transposed_colors_rejected(self):
        pixels=self.hook['cycles'][0]['images'][0]['rgb'];pixels[0],pixels[6]=pixels[6],pixels[0];self.assertSyntheticChangeRejected()
    def test_synthetic_unicode_image_change_rejected(self):
        self.hook['cycles'][0]['images'][5]['rgb'][0]^=1;self.assertSyntheticChangeRejected()
    def test_synthetic_missing_file_accepted_rejected(self):
        self.hook['cycles'][0]['images'][6]['accepted']=True;self.assertSyntheticChangeRejected()
    def test_synthetic_duplicate_image_rejected(self):
        images=self.hook['cycles'][0]['images'];images[1]=copy.deepcopy(images[0]);self.assertSyntheticChangeRejected()
    def test_synthetic_retained_rgb_on_failure_rejected(self):
        self.hook['cycles'][0]['images'][1]['rgb']=[17,31,89];self.assertSyntheticChangeRejected()
    def test_synthetic_boolean_dimension_rejected(self):
        self.hook['cycles'][0]['images'][2]['width']=True;self.assertSyntheticChangeRejected()
    def test_synthetic_missing_cycle_rejected(self):
        self.hook['cycles'].pop();self.assertSyntheticChangeRejected()
if __name__=='__main__':unittest.main()
