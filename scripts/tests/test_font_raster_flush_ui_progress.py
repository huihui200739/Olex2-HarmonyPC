"""Isolated synthetic recorder-protocol tests; never device data or qualification."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('isolated_font25_ui_recorder',
    ROOT / 'scripts/record-font-raster-flush-ui-progress.py')
recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder)


class FontRasterFlushUiProgressScopeTests(unittest.TestCase):
    def setUp(self):
        self.source = ROOT / 'out/ISOLATED-SYNTHETIC-SOURCE-NOT-A-BUILD'
        self.package = dict(path='isolated-synthetic.hap', bytes=1, sha256='not-a-real-package')
        self.verification = dict(passed=True, isolated_synthetic_protocol_model=True)
        self.previous = dict(status='in_progress', passed=False, milestone_complete=False,
            full_port_complete=False, functional_completion_percent=None,
            current_build_candidate=dict(identity=copy.deepcopy(recorder.IDENTITY),
                hap=copy.deepcopy(self.package), source=str(self.source.relative_to(ROOT)),
                verification=copy.deepcopy(self.verification)),
            font_window=dict(actual_rounds=3, independent_checks=445,
                complete_font_support=False, full_wx_runtime_verified=False))

    def validate(self):
        recorder.validate_previous(self.previous, self.package, self.source, self.verification)

    def test_isolated_protocol_shape_accepts_no_runtime_or_completion_claim(self):
        self.validate()
        self.assertFalse(self.previous['full_port_complete'])

    def test_completion_claim_rejected(self):
        for key, value in (('full_port_complete', True), ('functional_completion_percent', 100),
                           ('milestone_complete', True), ('passed', True), ('status', 'complete')):
            with self.subTest(key=key):
                old = self.previous[key]
                self.previous[key] = value
                with self.assertRaisesRegex(ValueError, 'overall completion'):
                    self.validate()
                self.previous[key] = old

    def test_prior_candidate_identity_rejected(self):
        self.previous['current_build_candidate']['identity']['versionName'] = '0.22.24'
        with self.assertRaisesRegex(ValueError, 'source/HAP/build'):
            self.validate()

    def test_package_byte_binding_rejected(self):
        self.previous['current_build_candidate']['hap']['sha256'] = 'different-isolated-value'
        with self.assertRaisesRegex(ValueError, 'source/HAP/build'):
            self.validate()

    def test_source_binding_rejected(self):
        self.previous['current_build_candidate']['source'] = 'out/OTHER-ISOLATED-SOURCE'
        with self.assertRaisesRegex(ValueError, 'source/HAP/build'):
            self.validate()

    def test_build_verification_binding_rejected(self):
        self.previous['current_build_candidate']['verification']['passed'] = False
        with self.assertRaisesRegex(ValueError, 'source/HAP/build'):
            self.validate()

    def test_finite_font_gate_count_rejected(self):
        self.previous['font_window']['independent_checks'] = 444
        with self.assertRaisesRegex(ValueError, 'finite font scope'):
            self.validate()

    def test_broader_font_qualification_claim_rejected(self):
        for key in ('complete_font_support', 'full_wx_runtime_verified'):
            with self.subTest(key=key):
                self.previous['font_window'][key] = True
                with self.assertRaisesRegex(ValueError, 'finite font scope'):
                    self.validate()
                self.previous['font_window'][key] = False


class FontRasterFlushUiProgressReplayTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='isolated-font25-replay-model-', dir=ROOT / 'out')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.tools = self.root / 'tools'
        self.replay = self.root / 'out/gx-application-evaluation/font-raster-flush-ui-tools25-replay-attempt1'
        self.report = self.root / 'isolated-replay.json'
        names = ['scripts/isolated-recipe-%02d.py' % n for n in range(65)]
        extra = ['tool-profile.json', 'host-catalog-reference.json', 'reports/app/persistent-gx-host-catalog.log']
        manifest = json.dumps(dict(files={name: dict(isolated_synthetic_test=True) for name in names}))
        self.rows = []
        for name in names + extra:
            data = (manifest if name == 'tool-profile.json' else
                    'ISOLATED SYNTHETIC REPLAY MODEL; NOT DEVICE OR BUILD DATA\n').encode()
            for folder in (self.tools, self.replay):
                path = folder / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            self.rows.append(dict(path=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
        self.value = dict(passed=True, all_original_bytes_equal=True, recipe_files=65,
            metadata_files=3, total_files=68, full_port_complete=False, files=self.rows)

    def validate(self):
        self.report.write_text(json.dumps(self.value))
        with patch.object(recorder, 'ROOT', self.root):
            return recorder.validate_replay(self.report, self.tools)

    def test_synthetic_exact_replay_returns_only_bound_files_not_runtime_qualification(self):
        evidence = self.validate()
        self.assertEqual(len(evidence), 137)
        self.assertFalse(self.value['full_port_complete'])

    def test_changed_replay_bytes_rejected(self):
        (self.replay / self.rows[0]['path']).write_text('ISOLATED DIFFERENCE\n')
        with self.assertRaisesRegex(ValueError, 'replay bytes changed'):
            self.validate()

    def test_recorded_hash_mismatch_rejected(self):
        self.rows[0]['sha256'] = 'isolated-wrong-hash'
        with self.assertRaisesRegex(ValueError, 'replay bytes changed'):
            self.validate()

    def test_missing_inventory_entry_rejected(self):
        self.value['files'] = self.rows[:-1]
        with self.assertRaisesRegex(ValueError, 'inventory differs'):
            self.validate()

    def test_wrong_recipe_count_rejected(self):
        self.value['recipe_files'] = 64
        with self.assertRaisesRegex(ValueError, '65\\+3'):
            self.validate()


if __name__ == '__main__':
    unittest.main()
