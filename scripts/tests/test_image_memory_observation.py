"""Actual retained 0.22.5 observations; synthetic mutations explicitly labeled."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from gx_application_validation import (validate_lifecycle,validate_image_observations,
    validate_texture_hook,inspect_owned_resource_observations,validate_no_decoder_control)
HERE=Path(__file__).resolve().parent/'fixtures'
class MemoryObservationTests(unittest.TestCase):
    def setUp(self):
        self.raw=json.loads((HERE/'gx_memory_observation.json').read_text())
        self.fixtures=json.loads((HERE/'gx_synthetic_images.json').read_text())['fixtures']
    def test_actual_memory_probe_and_virtual_hook(self):
        validate_lifecycle(self.raw['lifecycle'])
        images=validate_image_observations(self.raw['images'],self.fixtures,require_resources=True,resource_proof='owned-memory')
        hook=validate_texture_hook(self.raw['texture_hook'],self.fixtures,require_resources=True,resource_proof='owned-memory')
        self.assertFalse(images['transparent_original_rgb_preserved']);self.assertEqual(hook['opaque_decodes'],12)
        scope=inspect_owned_resource_observations(self.raw,proof='owned-memory')
        self.assertTrue(scope['passed']);self.assertFalse(scope['complete_descriptor_metadata_scan_passed'])
        self.assertFalse(scope['all_sdk_internal_allocations_proven_released'])
    def test_actual_memory_input_cannot_claim_full_scan(self):
        with self.assertRaises(ValueError):validate_image_observations(self.raw['images'],self.fixtures,require_resources=True)
    def test_actual_failed_process_count_is_still_rejected(self):
        raw=json.loads((HERE/'gx_memory_failed_count_observation.json').read_text())
        self.assertTrue(inspect_owned_resource_observations(raw,proof='owned-memory')['passed'])
        with self.assertRaises(ValueError):validate_texture_hook(raw['texture_hook'],self.fixtures,require_resources=True,resource_proof='owned-memory')
    def test_actual_pre_decoder_control_has_no_ownership_attribution(self):
        control=validate_no_decoder_control(self.raw['no_decoder_control']);self.assertEqual(control['samples'],41)
        self.assertFalse(control['descriptor_ownership_attributed'])
    def test_synthetic_handoff_descriptor_rejected(self):
        self.raw['images'][1]['observation']['resource_audit']['sdk_received_file_descriptor']=True
        with self.assertRaises(ValueError):validate_image_observations(self.raw['images'],self.fixtures,require_resources=True,resource_proof='owned-memory')
    def test_synthetic_buffer_lifetime_rejected(self):
        self.raw['texture_hook']['cycles'][0]['images'][0]['resource_audit']['buffer_released_after_handles']=False
        with self.assertRaises(ValueError):validate_texture_hook(self.raw['texture_hook'],self.fixtures,require_resources=True,resource_proof='owned-memory')
    def test_synthetic_process_count_change_not_ignored(self):
        self.raw['texture_hook']['cycles'][0]['fd_after']+=1
        with self.assertRaises(ValueError):validate_texture_hook(self.raw['texture_hook'],self.fixtures,require_resources=True,resource_proof='owned-memory')
    def test_synthetic_missing_memory_resource_audit_rejected(self):
        del self.raw['images'][1]['observation']['resource_audit']
        with self.assertRaises(ValueError):validate_image_observations(self.raw['images'],self.fixtures,require_resources=True,resource_proof='owned-memory')
if __name__=='__main__':unittest.main()
