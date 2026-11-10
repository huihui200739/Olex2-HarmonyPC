"""Synthetic mixed-route planning and isolated verifier guards, not device proof."""
import ast
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import native_font_mixed_validation as prior
import font_raster_flush_window_validation as single
import vector_font_state_window_validation as old_single
from persistent_gx_window_validation import SOURCE, validate_lifetime
from font_raster_flush_mixed_validation import (
    IDENTITY, RECIPE_FILES, action_plan, expected_counters, validate_lifetime_mixed,
    font_profile, validate_font, resource_stages_complete)
from font_raster_flush_mixed_progress_validation import validate_header


class FontRasterFlushMixedTests(unittest.TestCase):
    def proof(self, lease=4, requests=15):
        return dict(unique_owner=True, owner_unchanged=True, scene_disabled=True, scene_unbound=True,
            cleanup_before_gl4es_close=True, scene_destroyed=0, font_requests=requests, cleanup_failed=False,
            binding_before=lease-1, binding_after=lease, detachment_before=lease-1, detachment_after=lease,
            owner_address=101, renderer_address=102, scene_address=103)

    def snapshots(self):
        return [dict(applicationRuntime=dict(source=SOURCE)) for _ in range(2)]

    def lifetime(self, proof, lease=4, requests=15):
        return validate_lifetime_mixed(proof, 'isolated-synthetic-pid', 1,
            self.snapshots(), lease, requests)[0]

    def header(self):
        return dict(passed=True, identity=copy.deepcopy(IDENTITY), target='127.0.0.1:5555',
            hap_sha256='isolated-test-hash-not-device', orders=[dict(first_route='original'), dict(first_route='load')],
            full_port_complete=False, functional_completion_percent=None, full_ui_qualification=False)

    def test_exact25_identity_keeps19_and21_unchanged(self):
        self.assertEqual(IDENTITY['versionName'], '0.22.25')
        self.assertEqual(prior.IDENTITY['versionName'], '0.22.19')
        self.assertEqual(single.IDENTITY, IDENTITY)
        self.assertEqual(old_single.IDENTITY['versionName'], '0.22.21')

    def test_fifteen_requests_exact_cumulative_plan(self):
        plan = expected_counters(action_plan('original'))
        self.assertEqual([x['expected_font_requests'] for x in plan], [0, 0, 15, 15, 15, 30, 30, 30, 45])
        self.assertEqual([x['global_lease'] for x in plan], list(range(1, 10)))
        for route in ('original', 'load', 'font'):
            self.assertEqual([x['run'] for x in plan if x['route'] == route], [1, 2, 3])

    def test_final_font_closes45_in_each_order(self):
        for first in ('original', 'load'):
            last = expected_counters(action_plan(first))[-1]
            self.assertEqual((last['route'], last['expected_font_requests']), ('font', 45))

    def test_all_six_directed_pairs_covered(self):
        pairs = set()
        for first in ('original', 'load'):
            plan = action_plan(first)
            pairs.update(zip(plan, plan[1:]))
        self.assertEqual(pairs, {(a, b) for a in ('font', 'original', 'load')
                                for b in ('font', 'original', 'load') if a != b})

    def test_exact_lifetime_ten_gates_preserved(self):
        result = self.lifetime(self.proof())
        self.assertEqual(len(result), 10)
        self.assertTrue(all(x['passed'] for x in result), result)

    def test_old_three_request_count_rejected(self):
        self.assertFalse(all(x['passed'] for x in self.lifetime(self.proof(requests=3))))

    def test_extra_font_requests_rejected(self):
        self.assertFalse(all(x['passed'] for x in self.lifetime(self.proof(requests=30))))

    def test_route_local_run_cannot_replace_global_lease(self):
        self.assertFalse(all(x['passed'] for x in self.lifetime(self.proof(), lease=2)))

    def test_original_single_route_zero_font_guard_stays_strict(self):
        result, _ = validate_lifetime(self.proof(1, 15), 'isolated-synthetic-pid', 1, self.snapshots())
        self.assertFalse(all(x['passed'] for x in result))

    def test_isolated_font_profile_changes_only_global_lease_equation(self):
        before = single._namespace['_namespace']['_namespace']['_namespace']
        profile = font_profile(6)
        original_text = single._namespace['_namespace']['_namespace']['text']
        old = "legacy['binding_before']==legacy['detachment_before']==run-1 and legacy['binding_after']==legacy['detachment_after']==run"
        new = "legacy['binding_before']==legacy['detachment_before']==EXPECTED_GLOBAL_LEASE-1 and legacy['binding_after']==legacy['detachment_after']==EXPECTED_GLOBAL_LEASE"
        self.assertEqual(original_text.count(old), 1)
        self.assertEqual(profile['native_mixed_source'], original_text.replace(old, new, 1))
        self.assertIn("legacy['font_requests']==run*15", profile['native_mixed_source'])
        self.assertIs(single._namespace['_namespace']['_namespace']['_namespace'], before)
        self.assertNotIn('EXPECTED_GLOBAL_LEASE', before)
        self.assertEqual(profile['_namespace']['_namespace']['_namespace']['_namespace']['EXPECTED_GLOBAL_LEASE'], 6)

    def test_isolated_mask_profile_does_not_restore_wrong_boolean_gate(self):
        profile = font_profile(3)
        gpu = dict(color_write=[0]*4, color_write_raw_boolean=[0]*4, color_write_integer=[1]*4)
        self.assertTrue(profile['_namespace']['validated_mask'](gpu))
        self.assertEqual(gpu['color_write'], [0]*4)
        gpu['color_write_integer'] = [0]*4
        self.assertFalse(profile['_namespace']['validated_mask'](gpu))

    def test_missing_raw_mask_not_waived_in_mixed_profile(self):
        with self.assertRaisesRegex(ValueError, 'Missing or invalid'):
            font_profile(3)['_namespace']['validated_mask'](dict(color_write=[0]*4, color_write_integer=[1]*4))

    def test_nonpositive_and_boolean_lease_rejected(self):
        for lease in (0, -1, True, 3.0):
            with self.assertRaisesRegex(ValueError, 'positive global lease'): font_profile(lease)

    def test_font_request_preflight_rejects_old_or_noninteger_count(self):
        with tempfile.TemporaryDirectory(prefix='isolated-mixed-count-') as temp:
            folder = Path(temp)
            (folder/'received-artifacts.json').write_text(json.dumps(dict(run=1)))
            for count in (None, 3, 6, 9, 14, 16, True, 15.0):
                with self.assertRaisesRegex(ValueError, 'fifteen genuine requests'):
                    validate_font(folder, 3, source=ROOT/'out/gx-application-evaluation/source25',
                                  expected_font_requests=count)

    def isolated_resource_result(self):
        from resource_font_resample_reference import CASES
        return dict(passed=True, isolated_synthetic_protocol_model=True,
            resource_cases=[dict(isolated=True)]*3,
            resource_mask_observations=[dict(actual_integer=[1]*4)]*3,
            resample_cases=[dict(label=spec[0]) for spec in CASES],
            resample_mask_observations=[dict(actual_integer=[1]*4)]*6)

    def test_resource_stage_model_is_not_runtime_observation(self):
        self.assertTrue(resource_stages_complete(self.isolated_resource_result()))

    def test_missing_original_resource_stage_rejected(self):
        value=self.isolated_resource_result();value['resource_cases'].pop()
        self.assertFalse(resource_stages_complete(value))

    def test_resampling_stage_order_must_remain_exact(self):
        value=self.isolated_resource_result();value['resample_cases'].reverse()
        self.assertFalse(resource_stages_complete(value))

    def test_any_resource_integer_mask_failure_rejected(self):
        for key in ('resource_mask_observations','resample_mask_observations'):
            value=self.isolated_resource_result();value[key][0]=dict(actual_integer=[0]*4)
            self.assertFalse(resource_stages_complete(value))

    def test_prior19_header_cannot_inherit_qualification(self):
        value = self.header()
        value['identity'] = prior.IDENTITY
        with self.assertRaisesRegex(ValueError, 'identity/HAP'): validate_header(value, value['hap_sha256'])

    def test_wrong_emulator_header_rejected(self):
        value = self.header()
        value['target'] = 'unrelated-device'
        with self.assertRaisesRegex(ValueError, 'identity/HAP'): validate_header(value, value['hap_sha256'])

    def test_incomplete_order_protocol_rejected(self):
        value = self.header()
        value['orders'].pop()
        with self.assertRaisesRegex(ValueError, 'opposite three-route orders'): validate_header(value, value['hap_sha256'])

    def test_full_software_overclaim_rejected(self):
        value = self.header()
        value['full_port_complete'] = True
        with self.assertRaisesRegex(ValueError, 'partial scope'): validate_header(value, value['hap_sha256'])

    def test_all_exact_recipe_files_exist_and_parse(self):
        self.assertEqual(len(RECIPE_FILES), len(set(RECIPE_FILES)))
        for name in RECIPE_FILES:
            ast.parse((ROOT/'scripts'/name).read_text())

    def test_unknown_route_rejected(self):
        with self.assertRaises(ValueError): expected_counters(['unknown'])
        with self.assertRaises(ValueError): action_plan('font')


if __name__ == '__main__':
    unittest.main()
