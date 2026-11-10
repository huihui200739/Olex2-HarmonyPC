"""Isolated synthetic recorder-header tests; never device records or qualification."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('isolated_font25_mixed_recorder',
    ROOT / 'scripts/record-font-raster-flush-mixed-progress.py')
recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder)


class FontRasterFlushMixedProgressTests(unittest.TestCase):
    def setUp(self):
        self.source = ROOT / 'out/ISOLATED-SYNTHETIC-SOURCE-NOT-A-BUILD'
        self.package = dict(path='isolated.hap', bytes=1, sha256='not-a-real-package')
        self.verification = dict(passed=True, isolated_synthetic_model=True)
        self.font = dict(passed=True, checks=445, isolated_synthetic_model=True)
        self.ui = dict(passed=True, total_ui_checks=212, isolated_synthetic_model=True)
        self.current = dict(passed=True, isolated_synthetic_model=True,
            platform_image_audit=dict(pids=list(range(1, 8)), isolated_synthetic_model=True),
            startup_audit=dict(actual_starts=12, isolated_synthetic_model=True))
        self.previous = dict(status='in_progress', passed=False, milestone_complete=False,
            full_port_complete=False, functional_completion_percent=None,
            current_build_candidate=dict(identity=copy.deepcopy(recorder.IDENTITY),
                hap=copy.deepcopy(self.package), source=str(self.source.relative_to(ROOT)),
                verification=copy.deepcopy(self.verification)),
            font_window=dict(independent_checks=445, complete_font_support=False),
            current_ui=dict(status='passed-limited-212-check-diagnostic-ui', qualification=copy.deepcopy(self.ui)),
            platform_image_audit=copy.deepcopy(self.current['platform_image_audit']),
            startup_audit=copy.deepcopy(self.current['startup_audit']),
            mixed_routes=dict(status='not-yet-qualified-for-candidate25'))

    def validate(self):
        recorder.validate_current_base(self.previous, self.package, self.source,
            self.verification, self.font, self.ui, self.current)

    def test_isolated_header_model_is_not_actual_qualification(self):
        self.validate()
        self.assertFalse(self.previous['full_port_complete'])

    def test_completion_claim_rejected(self):
        for key, value in (('full_port_complete', True), ('functional_completion_percent', 100),
                           ('milestone_complete', True), ('passed', True), ('status', 'complete')):
            old = self.previous[key];self.previous[key] = value
            with self.assertRaisesRegex(ValueError, 'overall completion'):
                self.validate()
            self.previous[key] = old

    def test_wrong_candidate_source_or_package_binding_rejected(self):
        original = copy.deepcopy(self.previous['current_build_candidate'])
        for key, value in (('identity', dict(recorder.IDENTITY, versionName='0.22.21')),
                           ('hap', dict(self.package, sha256='isolated-different-hash')),
                           ('source', 'out/OTHER-ISOLATED-SOURCE'), ('verification', dict(passed=False))):
            self.previous['current_build_candidate'] = dict(original, **{key: value})
            with self.assertRaisesRegex(ValueError, 'source/HAP/build'):
                self.validate()

    def test_font_scope_or_gate_count_cannot_be_broadened(self):
        self.previous['font_window']['complete_font_support'] = True
        with self.assertRaisesRegex(ValueError, 'finite font proof'):
            self.validate()

    def test_partial_or_changed_ui_not_inherited(self):
        self.ui['total_ui_checks'] = 211
        with self.assertRaisesRegex(ValueError, 'raw212 UI'):
            self.validate()

    def test_resource_audit_must_match_revalidated_raw_evidence(self):
        self.previous['platform_image_audit']['isolated_synthetic_model'] = False
        with self.assertRaisesRegex(ValueError, 'all7PID/12-start'):
            self.validate()

    def test_all_seven_pids_and_twelve_starts_required(self):
        self.current['platform_image_audit']['pids'].pop()
        self.previous['platform_image_audit'] = copy.deepcopy(self.current['platform_image_audit'])
        with self.assertRaisesRegex(ValueError, 'all7PID/12-start'):
            self.validate()
        self.current['platform_image_audit']['pids'].append(7)
        self.current['startup_audit']['actual_starts'] = 11
        self.previous['platform_image_audit'] = copy.deepcopy(self.current['platform_image_audit'])
        self.previous['startup_audit'] = copy.deepcopy(self.current['startup_audit'])
        with self.assertRaisesRegex(ValueError, 'all7PID/12-start'):
            self.validate()

    def test_existing_mixed_qualification_cannot_be_silently_replaced(self):
        self.previous['mixed_routes']['status'] = 'isolated-other-state'
        with self.assertRaisesRegex(ValueError, 'unqualified current mixed'):
            self.validate()


if __name__ == '__main__':
    unittest.main()
