"""Isolated exact28 tool guards; no emulator or UI run is performed here."""
import ast,copy,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from font_display_tool_profiles import (IDENTITY,PROFILE_DEPENDENCIES,collection_source,startup_source,
    startup_verifier_source,ui_preparation_source,strict_ui_install_source,once)
from font_display_startup_validation import validate_runtime_snapshot

class FontDisplayToolProfileTests(unittest.TestCase):
    def snapshot(self):
        value=json.loads((ROOT/'scripts/tests/fixtures/texture_context_runtime_startup.json').read_text())
        value['installedPackage']['versionName']='0.22.28';value['installedPackage']['versionCode']=222800
        value['graphics']['fontDisplay']=dict(ready=True,display_id=0,revision=1,dpi_x=240.,dpi_y=240.,density_pixels=2.,source='actual-main-window-display-xDPI-yDPI',fixed_dpi_fallback=False)
        return value
    def test_profiles_compile(self):
        for text in (collection_source(),startup_source(),startup_verifier_source(),ui_preparation_source()):ast.parse(text)
    def test_collection_retains_all_resource_controls(self):
        text=collection_source()
        self.assertIn('a.texture_hook and a.owned_memory_resource_proof and a.png_rgb_backend and a.require_post_decode_control',text)
        self.assertIn('pids=sorted(pids_in(app))',text);self.assertIn('require(digest(path)==expected',text)
        self.assertIn('Only exact candidate28 may be collected',text)
    def test_exact_target_in_both_profiles(self):
        for text in (collection_source(),startup_source()):self.assertIn("if a.target!='127.0.0.1:5555'",text)
    def test_startup_preserves_exact_twelve_and_live_pid_corroboration(self):
        text=startup_source()
        self.assertIn('if a.runs!=12 or not (a.owned_memory_resource_proof and a.png_rgb_backend and a.require_post_decode_control)',text)
        self.assertIn("identity['versionName']=='0.22.28' and identity['versionCode']==222800",text)
        self.assertIn('require(pid()==current',text);self.assertIn('status in parse_app_reports(live_logs)',text)
    def test_startup_strict_install_guard(self):self.assertIn("if args and args[0]=='install':validate_install_output(output,d.returncode)",startup_source())
    def test_executed_sources_and_all_dependencies_retained(self):
        for text in (collection_source(),startup_source()):
            self.assertIn("(report/'executed-profile.py').write_text(_EXECUTED_PROFILE)",text)
            self.assertIn("candidate='0.22.28'",text)
            for name in PROFILE_DEPENDENCIES:self.assertIn(name,text)
    def test_nonidentity_startup_gates_preserved_exactly(self):
        old=(ROOT/'scripts/texture_context_startup_validation.py').read_text()
        expected=old.replace("identity.get('versionName')!='0.22.14' or identity.get('versionCode')!=221400","identity.get('versionName')!='0.22.28' or identity.get('versionCode')!=222800",1).replace('Only candidate14 long-lived owner startup accepted','Only candidate28 long-lived owner startup accepted',1)
        self.assertEqual(startup_verifier_source(),expected)
    def test_preparation_keeps_original_ui_generator_except_three_bound_inputs(self):
        old=(ROOT/'scripts/prepare-texture-context-ui-tools.py').read_text()
        expected=old.replace("identity['versionName']!='0.22.14' or identity['versionCode']!=221400","identity['versionName']!='0.22.28' or identity['versionCode']!=222800",1).replace("source/'texture-context-inputs.json'","source/'font-display-telemetry-inputs.json'",1).replace("manifest=dict(version='0.22.14'","manifest=dict(version='0.22.28'",1)
        self.assertEqual(ui_preparation_source(),expected)
    def test_ui_install_profile_only_adds_a_strict_guard(self):
        text="before\n    install = hdc('install', str(retained_hap))\nafter\n"
        result=strict_ui_install_source(text)
        self.assertIn("    install = hdc('install', str(retained_hap))",result);self.assertIn('validate_install_output(install)',result)
        self.assertTrue(result.startswith('before\n'));self.assertTrue(result.endswith('after\n'))
    def test_isolated_ready_display_keeps_all_eleven_gates(self):
        result=validate_runtime_snapshot(self.snapshot(),IDENTITY)
        self.assertTrue(result['passed']);self.assertEqual(len(result['checks']),11);self.assertFalse(result['full_port_complete']);self.assertFalse(result['actual_display_DPI_qualified'])
    def test_wrong_candidate_rejected(self):
        for version,code in [('0.22.25',222500),('0.22.26',222600),('0.22.27',222700)]:
            with self.assertRaises(ValueError):validate_runtime_snapshot(self.snapshot(),dict(IDENTITY,versionName=version,versionCode=code))
    def test_snapshot_identity_must_match(self):
        value=self.snapshot();value['installedPackage']['versionName']='0.22.25';self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_missing_or_invalid_actual_display_fails(self):
        for key,val in [('ready',False),('fixed_dpi_fallback',True),('dpi_x',0),('revision',0),('source','default96')]:
            value=self.snapshot();value['graphics']['fontDisplay'][key]=val;self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
        value=self.snapshot();del value['graphics']['fontDisplay'];self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_uninitialized_owner_still_fails(self):
        value=self.snapshot();value['applicationRuntime']['initialized']=False;self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_unsaved_workspace_still_fails(self):
        value=self.snapshot();value['workspaceState']['saveStatus']='pending';self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_old_nonempty_original_logs_still_fail(self):
        value=self.snapshot();value['originalLog']['posted']=1;self.assertFalse(validate_runtime_snapshot(value,IDENTITY)['passed'])
    def test_changed_recipe_anchor_rejected(self):
        for text in ('other','x x'):
            with self.assertRaises(ValueError):once(text,'x','y')

if __name__=='__main__':unittest.main()
