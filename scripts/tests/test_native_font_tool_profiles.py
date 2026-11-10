"""Isolated tool-profile tests; no emulator operations or new runtime evidence."""
import ast
import copy
import json
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from native_font_tool_profiles import collection_source,startup_source,startup_verifier_source,once
from native_font_startup_validation import validate_runtime_snapshot
IDENTITY=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.19',versionCode=221900)

class NativeFontToolProfileTests(unittest.TestCase):
    def snapshot(self):
        # The existing real14 snapshot is changed only in isolated test memory;
        # it must never be saved or used as a candidate19 runtime observation.
        value=json.loads((ROOT/'scripts/tests/fixtures/texture_context_runtime_startup.json').read_text())
        value['installedPackage']['versionName']='0.22.19'
        value['installedPackage']['versionCode']=221900
        return value
    def test_collection_profile_compiles(self):
        ast.parse(collection_source())
    def test_startup_profile_compiles(self):
        ast.parse(startup_source());ast.parse(startup_verifier_source())
    def test_collection_preserves_all_resource_controls(self):
        text=collection_source()
        self.assertIn('a.texture_hook and a.owned_memory_resource_proof and a.png_rgb_backend and a.require_post_decode_control',text)
        self.assertIn('pids=sorted(pids_in(app))',text)
        self.assertIn("require(digest(path)==expected",text)
        self.assertIn('Only exact candidate19 may be collected',text)
    def test_startup_keeps_exact_twelve_and_resource_controls(self):
        text=startup_source()
        self.assertIn('if a.runs!=12 or not (a.owned_memory_resource_proof and a.png_rgb_backend and a.require_post_decode_control)',text)
        self.assertIn("identity['versionName']=='0.22.19' and identity['versionCode']==221900",text)
        self.assertIn("require(pid()==current",text)
        self.assertIn('status in parse_app_reports(live_logs)',text)
    def test_exact_executed_source_is_retained(self):
        for text in (collection_source(),startup_source()):
            self.assertIn("(report/'executed-profile.py').write_text(_EXECUTED_PROFILE)",text)
            self.assertIn("source_sha256=hashlib.sha256(_EXECUTED_PROFILE.encode()).hexdigest()",text)
    def test_exact_profile_and_dependency_retention(self):
        for text in (collection_source(),startup_source()):
            self.assertIn("candidate='0.22.19'",text)
            self.assertNotIn("candidate='0.22.15'",text)
            self.assertIn("result['profile_dependencies']",text)
            self.assertIn('native_font_tool_profiles.py',text)
            self.assertIn('native_font_application_validation.py',text)
            self.assertIn('native_font_startup_validation.py',text)
    def test_non_identity_startup_gates_are_exactly_preserved(self):
        old=(ROOT/'scripts/texture_context_startup_validation.py').read_text()
        actual=startup_verifier_source()
        expected=old.replace("identity.get('versionName')!='0.22.14' or identity.get('versionCode')!=221400",
                             "identity.get('versionName')!='0.22.19' or identity.get('versionCode')!=221900",1)
        expected=expected.replace('Only candidate14 long-lived owner startup accepted',
                                  'Only candidate19 long-lived owner startup accepted',1)
        self.assertEqual(actual,expected)
    def test_isolated_candidate19_snapshot_shape_keeps_ten_gates(self):
        result=validate_runtime_snapshot(self.snapshot(),IDENTITY)
        self.assertTrue(result['passed'],result)
        self.assertEqual(len(result['checks']),10)
        self.assertFalse(result['full_port_complete'])
    def test_wrong_package_profile_rejected(self):
        for version,code in (('0.22.14',221400),('0.22.15',221500),('0.22.16',221600),('0.22.18',221800)):
            other=dict(IDENTITY,versionName=version,versionCode=code)
            with self.assertRaises(ValueError):validate_runtime_snapshot(self.snapshot(),other)
    def test_snapshot_must_match_actual_identity(self):
        value=self.snapshot();value['installedPackage']['versionName']='0.22.14'
        self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_uninitialized_owner_fails(self):
        value=self.snapshot();value['applicationRuntime']['initialized']=False
        self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_nonempty_original_logs_fail_startup_gate(self):
        value=self.snapshot();value['originalLog']['posted']=1
        self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_unsaved_workspace_fails(self):
        value=self.snapshot();value['workspaceState']['saveStatus']='pending'
        self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_profile_rejects_ambiguous_source_shape(self):
        with self.assertRaises(ValueError):once('x x','x','y')
        with self.assertRaises(ValueError):once('other','x','y')

if __name__=='__main__':unittest.main()
