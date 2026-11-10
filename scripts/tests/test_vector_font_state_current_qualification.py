"""Isolated synthetic audit-protocol tests; never target observations or qualification."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import vector_font_state_current_qualification as audit


class VectorFontStateCurrentQualificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='isolated-vector21-audit-', dir=ROOT / 'out')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.app_path = self.root / 'app.json'
        self.platform_path = self.root / 'platform/result.json'
        self.startup_path = self.root / 'startup/result.json'
        self.pixels = b'ISOLATED SYNTHETIC PROTOCOL TEST: NOT PNG OR DEVICE DATA\n'
        self.fixture_sha = hashlib.sha256(self.pixels).hexdigest()
        self.fixtures = dict(fixtures=[dict(file='synthetic-rgb-2.png', sha256=self.fixture_sha)])
        self.ui = dict(passed=True, total_ui_checks=212, full_port_complete=False)
        self.hap_sha = 'isolated-test-package-not-a-device-hash'
        self.app = dict(scope='isolated test only', observations=[dict(pid=n) for n in range(1001, 1008)])
        self.write(self.app_path, self.app)
        controls = dict(resource_proof='owned-memory', texture_backend='libpng-unassociated-rgb',
                        post_decode_control_required=True, complete_descriptor_metadata_scan_passed=False,
                        passed=True, target='127.0.0.1:5555')
        self.platform = dict(controls, application=dict(identity=copy.deepcopy(audit.IDENTITY),
            hap_sha256=self.hap_sha, sha256=audit.digest(self.app_path), ui=self.ui),
            current_pid=1007, processes=[])
        self.startup = dict(controls, identity=copy.deepcopy(audit.IDENTITY), hap=dict(sha256=self.hap_sha),
                            owned_resource_audit_required=True, long_lived_runtime_initialization_required=True, runs=[])
        self.gates = {name: dict(passed=True, isolated_synthetic_model=True) for name in (
            'lifecycle', 'image_capabilities', 'owned_resources', 'no_decoder_control',
            'post_decode_control', 'texture_hook')}
        self.gates['owned_resources']['complete_descriptor_metadata_scan_passed'] = False
        for pid in range(1001, 1008):
            folder = self.platform_path.parent / ('pid-' + str(pid))
            self.write(folder / 'result.json', dict(pid=pid, execution_completed=True,
                                                   isolated_checks=self.gates))
            (folder / 'synthetic-rgb-2.png').write_bytes(self.pixels)
            (folder / '合成颜色-2.png').write_bytes(self.pixels)
            files = {}
            for name in ('result.json', 'synthetic-rgb-2.png', '合成颜色-2.png'):
                meta = audit.bound(folder / name)
                files[name] = dict(bytes=meta['bytes'], sha256=meta['sha256'], target_sha256=meta['sha256'])
            self.platform['processes'].append(dict(pid=pid, files=files, **copy.deepcopy(self.gates)))
        self.snapshot = json.loads((ROOT / 'scripts/tests/fixtures/texture_context_runtime_startup.json').read_text())
        self.snapshot['installedPackage'].update(versionName='0.22.21', versionCode=222100)
        owner = audit.validate_runtime_snapshot(self.snapshot, audit.IDENTITY)
        self.assertTrue(owner['passed'])
        for number in range(1, 13):
            folder = self.startup_path.parent / ('run-' + str(number))
            pid = 2000 + number
            self.write(folder / 'native-result.json', dict(pid=pid, execution_completed=True,
                                                         isolated_checks=self.gates))
            self.write(folder / 'runtime-status.json', self.snapshot)
            (folder / 'runtime-process-report.log').write_text('OLEX2_APP_REPORT ' + json.dumps(self.snapshot) + '\n')
            checks = {('images' if name == 'image_capabilities' else name): copy.deepcopy(value)
                      for name, value in self.gates.items()}
            native_sha = audit.digest(folder / 'native-result.json')
            self.startup['runs'].append(dict(run=number, pid=pid, passed=True, native_result_sha256=native_sha,
                target_sha256=native_sha, runtime_owner=owner,
                runtime_status_sha256=audit.digest(folder / 'runtime-status.json'),
                runtime_log_sha256=audit.digest(folder / 'runtime-process-report.log'), **checks))
        self.profile(self.platform, self.platform_path, audit.collection_source(), 'collect-gx-texture-context-evaluation.py')
        self.profile(self.startup, self.startup_path, audit.startup_source(), 'run-texture-context-startup-audit.py')
        real_load = audit.load
        fixture_path = ROOT / 'scripts/tests/fixtures/gx_synthetic_images.json'
        self.addCleanup(patch.stopall)
        patch.object(audit, 'load', side_effect=lambda p: copy.deepcopy(self.fixtures)
                     if Path(p) == fixture_path else real_load(p)).start()
        patch.object(audit, 'platform_checks', side_effect=lambda raw, fixtures: raw['isolated_checks']).start()

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')

    def profile(self, report, path, text, baseline):
        folder = path.parent
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'executed-profile.py').write_text(text)
        names = ('vector_font_state_tool_profiles.py', 'vector_font_state_application_validation.py',
                 'vector_font_state_startup_validation.py', 'texture_context_application_validation.py',
                 'texture_context_startup_validation.py')
        report['profile_dependencies'] = {}
        for name in names:
            (folder / name).write_bytes((ROOT / 'scripts' / name).read_bytes())
            report['profile_dependencies'][name] = audit.digest(folder / name)
        report['execution_profile'] = dict(candidate='0.22.21', baseline=baseline,
            source_file='executed-profile.py', source_sha256=audit.digest(folder / 'executed-profile.py'))

    def run_audit(self):
        self.write(self.platform_path, self.platform)
        self.write(self.startup_path, self.startup)
        return audit.validate_current_qualifications(self.app_path, self.ui, self.platform_path,
                                                     self.startup_path, self.hap_sha)

    def test_isolated_protocol_pass_is_not_full_port_completion(self):
        result = self.run_audit()
        self.assertTrue(result['passed'])
        self.assertFalse(result['full_port_complete'])
        self.assertIsNone(result['functional_completion_percent'])
        self.assertFalse(result['platform_image_audit']['complete_descriptor_metadata_scan_passed'])
        self.assertFalse(result['startup_audit']['complete_descriptor_metadata_scan_passed'])
        self.assertEqual(result['startup_audit']['actual_long_lived_owner_startup_checks'], 120)

    def test_prior_candidate_identity_cannot_be_inherited(self):
        self.platform['application']['identity']['versionName'] = '0.22.19'
        with self.assertRaisesRegex(ValueError, 'identity differs'): self.run_audit()

    def test_mismatched_package_bytes_fail(self):
        self.startup['hap']['sha256'] = 'different'
        with self.assertRaisesRegex(ValueError, 'identity differs'): self.run_audit()

    def test_partial_ui_rejected(self):
        self.ui['total_ui_checks'] = 211
        with self.assertRaisesRegex(ValueError, 'Full current212'): self.run_audit()

    def test_missing_ui_process_rejected(self):
        self.platform['processes'].pop()
        with self.assertRaisesRegex(ValueError, 'seven actual UI PIDs'): self.run_audit()

    def test_duplicate_ui_process_rejected(self):
        self.platform['processes'][-1] = copy.deepcopy(self.platform['processes'][0])
        with self.assertRaisesRegex(ValueError, 'seven actual UI PIDs'): self.run_audit()

    def test_missing_retained_png_cannot_hide_in_passed_field(self):
        del self.platform['processes'][0]['files']['synthetic-rgb-2.png']
        with self.assertRaisesRegex(ValueError, 'exact synthetic resource'): self.run_audit()

    def test_received_byte_change_fails(self):
        (self.platform_path.parent / 'pid-1001/synthetic-rgb-2.png').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'bytes changed'): self.run_audit()

    def test_self_consistent_changed_png_still_fails_fixed_reference(self):
        path = self.platform_path.parent / 'pid-1001/synthetic-rgb-2.png'
        path.write_bytes(b'changed')
        meta = audit.bound(path)
        self.platform['processes'][0]['files'][path.name] = dict(bytes=meta['bytes'],
            sha256=meta['sha256'], target_sha256=meta['sha256'])
        with self.assertRaisesRegex(ValueError, 'independent PNG bytes differ'): self.run_audit()

    def test_raw_resource_gate_recomputed(self):
        path = self.platform_path.parent / 'pid-1001/result.json'
        raw = json.loads(path.read_text())
        raw['isolated_checks']['owned_resources']['passed'] = False
        self.write(path, raw)
        meta = audit.bound(path)
        self.platform['processes'][0]['files'][path.name].update(bytes=meta['bytes'],
            sha256=meta['sha256'], target_sha256=meta['sha256'])
        with self.assertRaisesRegex(ValueError, 'resource gate changed or failed'): self.run_audit()

    def test_exact_twelve_starts_required(self):
        self.startup['runs'].pop()
        with self.assertRaisesRegex(ValueError, 'Exactly12'): self.run_audit()

    def test_repeated_startup_pid_rejected(self):
        self.startup['runs'][1]['pid'] = self.startup['runs'][0]['pid']
        with self.assertRaisesRegex(ValueError, 'PID/ordinal'): self.run_audit()

    def test_runtime_owner_revalidated_from_raw_snapshot(self):
        path = self.startup_path.parent / 'run-1/runtime-status.json'
        value = copy.deepcopy(self.snapshot)
        value['applicationRuntime']['initialized'] = False
        self.write(path, value)
        self.startup['runs'][0]['runtime_status_sha256'] = audit.digest(path)
        with self.assertRaisesRegex(ValueError, 'owner startup evidence'): self.run_audit()

    def test_live_pid_log_must_contain_matching_runtime_snapshot(self):
        path = self.startup_path.parent / 'run-1/runtime-process-report.log'
        path.write_text('isolated empty log\n')
        self.startup['runs'][0]['runtime_log_sha256'] = audit.digest(path)
        with self.assertRaisesRegex(ValueError, 'explicit live PID capture'): self.run_audit()

    def test_changed_executed_profile_fails_even_with_rehashed_metadata(self):
        path = self.platform_path.parent / 'executed-profile.py'
        path.write_text(path.read_text() + '\n# isolated test change\n')
        self.platform['execution_profile']['source_sha256'] = audit.digest(path)
        with self.assertRaisesRegex(ValueError, 'exact21 profile changed'): self.run_audit()

    def test_changed_dependency_fails_even_with_rehashed_metadata(self):
        name = 'vector_font_state_startup_validation.py'
        path = self.platform_path.parent / name
        path.write_text(path.read_text() + '\n# isolated test change\n')
        self.platform['profile_dependencies'][name] = audit.digest(path)
        with self.assertRaisesRegex(ValueError, 'dependency changed'): self.run_audit()

    def test_missing_profile_dependency_rejected(self):
        del self.startup['profile_dependencies']['vector_font_state_startup_validation.py']
        with self.assertRaisesRegex(ValueError, 'dependency set differs'): self.run_audit()

    def test_omitted_resource_control_rejected(self):
        self.platform['post_decode_control_required'] = False
        with self.assertRaisesRegex(ValueError, 'owned-memory/PNG/post-decode'): self.run_audit()

    def test_wrong_profile_baseline_rejected(self):
        self.startup['execution_profile']['baseline'] = 'unrelated.py'
        with self.assertRaisesRegex(ValueError, 'filename/baseline'): self.run_audit()

    def test_full_descriptor_ownership_overclaim_rejected(self):
        self.platform['complete_descriptor_metadata_scan_passed'] = True
        with self.assertRaisesRegex(ValueError, 'descriptor metadata summary differs'): self.run_audit()

    def test_process_id_walker_matches_collector_pid_suffixes(self):
        self.assertEqual(audit.process_ids(dict(pid=1, active_pid='2', nested=[dict(prior_pid=3)])), {1, 2, 3})


class VectorFontStateRecorderArgumentTests(unittest.TestCase):
    """Only parser relationships; these synthetic values never reach device or records."""
    @classmethod
    def setUpClass(cls):
        import importlib.util
        spec = importlib.util.spec_from_file_location('isolated_vector21_recorder',
            ROOT / 'scripts/record-vector-font-state-progress.py')
        cls.recorder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.recorder)

    def args(self, **kwargs):
        from types import SimpleNamespace
        return SimpleNamespace(**dict(dict(app_report=None, ui_tools=None,
            platform_report=None, startup_report=None), **kwargs))

    def test_missing_paired_ui_tools_rejected(self):
        with self.assertRaisesRegex(ValueError, 'both completed UI'):
            self.recorder.validate_audit_arguments(self.args(app_report='isolated-test'))

    def test_missing_paired_platform_report_rejected(self):
        with self.assertRaisesRegex(ValueError, 'both current platform'):
            self.recorder.validate_audit_arguments(self.args(startup_report='isolated-test'))

    def test_resource_qualification_without_ui_rejected(self):
        with self.assertRaisesRegex(ValueError, 'completed current UI'):
            self.recorder.validate_audit_arguments(self.args(platform_report='isolated-platform',
                startup_report='isolated-startup'))

    def test_font_only_arguments_still_supported(self):
        self.recorder.validate_audit_arguments(self.args())

    def test_mixed_without_current_resources_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Mixed qualification requires'):
            self.recorder.validate_audit_arguments(self.args(app_report='isolated-ui',
                ui_tools='isolated-tools', mixed_report='isolated-mixed'))

    def test_mixed_with_current_ui_and_resource_arguments_supported(self):
        self.recorder.validate_audit_arguments(self.args(app_report='isolated-ui',
            ui_tools='isolated-tools', platform_report='isolated-platform',
            startup_report='isolated-startup', mixed_report='isolated-mixed'))

    def test_complete_argument_shape_supported(self):
        self.recorder.validate_audit_arguments(self.args(app_report='isolated-ui',
            ui_tools='isolated-tools', platform_report='isolated-platform', startup_report='isolated-startup'))


if __name__ == '__main__':
    unittest.main()
