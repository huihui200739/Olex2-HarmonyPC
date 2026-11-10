import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from png_texture_validation import validate_png_observations,expected_cases
FIXTURE=Path(__file__).with_name('fixtures')/'gx_png_host_observation.json'
class PngTextureValidationTests(unittest.TestCase):
 def setUp(self):self.raw=json.loads(FIXTURE.read_text())
 def reject(self,change):
  change(self.raw)
  with self.assertRaises(ValueError):validate_png_observations(self.raw)
 def test_recorded_host_observation(self):
  r=validate_png_observations(self.raw);self.assertEqual(r['decoded_images'],56);self.assertEqual(r['observed_libpng_callback_allocations'],467);self.assertFalse(r['imagekit_executed']);self.assertFalse(r['full_process_metadata_scan_passed'])
 def test_exact_case_set(self):self.assertEqual(len(expected_cases()),56)
 def test_missing_image(self):self.reject(lambda r:r['images'].pop())
 def test_duplicate_image(self):self.reject(lambda r:r['images'].__setitem__(1,copy.deepcopy(r['images'][0])))
 def test_renamed_case(self):self.reject(lambda r:r['images'][0].update(name='renamed.png'))
 def test_wrong_version(self):self.reject(lambda r:r.update(decoder_version='1.6.58'))
 def test_false_pass(self):self.reject(lambda r:r.update(passed=False))
 def test_bool_check_count(self):self.reject(lambda r:r.update(checks=True))
 def test_wrong_check_count(self):self.reject(lambda r:r.update(checks=623))
 def test_missing_hidden_rgb_case_count(self):self.reject(lambda r:r.update(hidden_rgb_images=33))
 def test_wrong_adam7_count(self):self.reject(lambda r:r.update(adam7_images=28))
 def test_gpu_scope(self):self.reject(lambda r:r.update(real_gpu_execution=True))
 def test_full_port_scope(self):self.reject(lambda r:r.update(full_port_complete=True))
 def test_pixel_size(self):self.reject(lambda r:r['images'][0].update(rgb_bytes=191))
 def test_source_color(self):self.reject(lambda r:r['images'][0].update(source_color_type=6))
 def test_source_depth(self):self.reject(lambda r:r['images'][0].update(source_bit_depth=16))
 def test_interlace(self):self.reject(lambda r:r['images'][0].update(source_interlace=1))
 def test_live_callback(self):self.reject(lambda r:r['images'][0].update(live_allocations=1))
 def test_unreleased_callback(self):self.reject(lambda r:r['images'][0].update(releases=0))
 def test_failed_close(self):self.reject(lambda r:r['images'][0]['input_resources']['file'].update(close_code=-1))
 def test_imagekit_claim(self):self.reject(lambda r:r['images'][0]['input_resources'].update(handles=[dict(name='source')]))
 def test_full_scan_claim(self):self.reject(lambda r:r['images'][0]['input_resources'].update(balanced=True))
 def test_missing_encoded_owner(self):self.reject(lambda r:r['images'][0]['input_resources'].update(buffer_released_after_handles=False))
 def test_unicode_filename_case(self):self.reject(lambda r:r['images'][-1].update(unicode_path=False))
if __name__=='__main__':unittest.main()
