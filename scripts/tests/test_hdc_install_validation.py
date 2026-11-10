"""Isolated synthetic acknowledgement-parser tests, never installation observations."""
import ast
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from hdc_install_validation import validate_install_output
class HdcInstallValidationTests(unittest.TestCase):
    def success(self):return '[Info]App install path:/isolated-synthetic-test.hap msg:install bundle successfully. \nAppMod finish\n'
    def test_explicit_success_marker_accepted(self):validate_install_output(self.success())
    def test_nonzero_exit_rejected_even_with_success_marker(self):
        with self.assertRaises(ValueError):validate_install_output(self.success(),1)
    def test_zero_exit_insufficient_disk_error_rejected(self):
        text='[Info]App install path:/isolated.hap msg:error: failed to install bundle. code:9568288 error: install failed due to insufficient disk memory. \nAppMod finish\n'
        with self.assertRaisesRegex(ValueError,'insufficient disk'):validate_install_output(text,0)
    def test_fail_prefix_rejected(self):
        with self.assertRaises(ValueError):validate_install_output('[Fail] App install failed\n')
    def test_empty_or_finish_only_output_not_a_success(self):
        for text in ('','AppMod finish','sent 100%'):
            with self.assertRaises(ValueError):validate_install_output(text)
    def test_success_text_with_error_is_rejected(self):
        with self.assertRaises(ValueError):validate_install_output(self.success()+'error: synthetic contradiction')
    def test_v2_profile_compiles_without_changing_existing_runner(self):
        for name in ('run-font-family-window-smoke.py','run-font-family-window-smoke-v2.py'):ast.parse((ROOT/'scripts'/name).read_text())
        old=(ROOT/'scripts/run-font-family-window-smoke.py').read_text()
        self.assertNotIn('validate_install_output',old)
    def test_v2_preserves_complete_original_recipe_except_guard_and_own_recipe_names(self):
        old=(ROOT/'scripts/run-font-family-window-smoke.py').read_text()
        old=old.replace('from font_family_window_validation import IDENTITY,validate','from font_family_window_validation import IDENTITY,validate\nfrom hdc_install_validation import validate_install_output',1)
        marker="  if run.returncode or any(s.lstrip().startswith('[Fail]') for s in text.splitlines()):raise RuntimeError(text)"
        old=old.replace(marker,"  if args and args[0]=='install':validate_install_output(text,run.returncode)\n"+marker,1)
        old=old.replace("('run-font-family-window-smoke.py',","('run-font-family-window-smoke-v2.py','hdc_install_validation.py',",1)
        self.assertEqual((ROOT/'scripts/run-font-family-window-smoke-v2.py').read_text(),old)
