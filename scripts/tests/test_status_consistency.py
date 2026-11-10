#!/usr/bin/env python3
"""Regression checks using isolated, explicitly synthetic copies of local evidence.

Never modifies the actual app, runtime reports, SDK, or simulator.
"""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class IsolatedSyntheticStatusTests(unittest.TestCase):
    def setUp(self):
        self.owned = tempfile.TemporaryDirectory(prefix='status-regression-', dir=ROOT / 'out')
        self.addCleanup(self.owned.cleanup)
        self.root = Path(self.owned.name)
        paths = {
            'scripts/check-adaptation-status.py', 'scripts/build_provenance.py',
            'scripts/build_provenance_graphics.py','scripts/vendor_sources.py',
            'scripts/original_renderer_integration_validation.py','scripts/original_ui_checks.json',
            'scripts/matrix_validation.py', 'reports/original-matrix-status.json',
            'scripts/polygon_validation.py', 'reports/original-polygon-status.json',
            'scripts/desktop_command_validation.py', 'reports/original-desktop-command-status.json',
            'scripts/application_runtime_validation.py', 'reports/original-application-runtime-status.json',
            'scripts/options_document_validation.py', 'reports/options-document-status.json',
            'scripts/line_validation.py', 'reports/original-line-status.json',
            'scripts/line_path_validation.py', 'reports/original-line-path-status.json',
            'scripts/original_log_validation.py', 'reports/original-log-status.json',
            'scripts/desktop_shell_validation.py', 'reports/desktop-shell-status.json',
            'out/app/entry-default-unsigned.provenance.json', 'app/AppScope/app.json5',
            'upstream/sdl/esphere.h', 'reports/app/last-run.json',
            'reports/app/package-verification.json', 'reports/emulator/last-run.json',
            'reports/baseline-status.json', 'reports/mesh-adapter-status.json',
            'reports/submission-status.json', 'reports/primitive-adapter-status.json',
            'reports/source-integrity.json', 'reports/upstream-helper-extraction.json',
            'reports/clean-build-status.json', 'source.lock.json',
            'reports/desktop-state-status.json', 'reports/port-coverage.json', 'reports/color-adapter-status.json', 'reports/material-adapter-status.json', 'reports/original-triangle-status.json',
            'app/entry/src/main/ets/model/DesktopState.ts',
            'scripts/tests/desktop-state.test.mjs', 'reports/app/persistence-model-tests.log',
        }
        color = json.loads((ROOT / 'reports/color-adapter-status.json').read_text())
        paths.update(item['path'] for item in color['sources'])
        paths.update([color['host']['color_log'], color['host']['build_log']])
        material = json.loads((ROOT / 'reports/material-adapter-status.json').read_text())
        paths.update(item['path'] for item in material['sources'])
        paths.add(material['host']['material_log'])
        paths.add(material['commands']['host_log'])
        paths.add(material['lit']['host_log'])
        triangle = json.loads((ROOT / 'reports/original-triangle-status.json').read_text())
        paths.update(item['path'] for item in triangle['sources'])
        paths.update([triangle['host']['test_log'],triangle['host']['build_log']])
        matrix = json.loads((ROOT / 'reports/original-matrix-status.json').read_text())
        paths.update(item['path'] for item in matrix['sources'])
        paths.update([matrix['host']['test_log'],matrix['host']['build_log']])
        polygon=json.loads((ROOT/'reports/original-polygon-status.json').read_text())
        paths.update(item['path'] for item in polygon['sources'])
        paths.update([polygon['host']['test_log'],polygon['host']['build_log']])
        commands=json.loads((ROOT/'reports/original-desktop-command-status.json').read_text())
        paths.update(item['path'] for item in commands['sources'])
        paths.update([commands['host']['test_log'],commands['host']['build_log']])
        application=json.loads((ROOT/'reports/original-application-runtime-status.json').read_text())
        paths.update(item['path'] for item in application['sources'])
        paths.update([application['host']['test_log'],application['host']['build_log']])
        documents=json.loads((ROOT/'reports/options-document-status.json').read_text())
        paths.update(item['path'] for item in documents['sources'])
        paths.update([documents['host']['test_log'],documents['host']['build_log'],documents['model']['evidence']])
        lines=json.loads((ROOT/'reports/original-line-status.json').read_text())
        paths.update(item['path'] for item in lines['sources'])
        paths.update([lines['host']['test_log'],lines['host']['build_log'],lines['ohos']['test_log']])
        paths.add('reports/original-core-runtime-status.json')
        core=json.loads((ROOT/'reports/original-core-runtime-status.json').read_text())
        paths.update(item['path'] for item in core['sources'])
        paths.update([core['host']['test_log'],core['host']['build_log']])
        paths_report=json.loads((ROOT/'reports/original-line-path-status.json').read_text())
        paths.update(item['path'] for item in paths_report['sources'])
        paths.update([paths_report['host']['test_log'],paths_report['host']['build_log'],paths_report['ohos']['test_log']])
        original_log=json.loads((ROOT/'reports/original-log-status.json').read_text())
        paths.update(item['path'] for item in original_log['sources'])
        paths.update([original_log['host']['test_log'],original_log['host']['build_log'],original_log['ohos']['test_log']])
        shell=json.loads((ROOT/'reports/desktop-shell-status.json').read_text())
        paths.update(item['path'] for item in shell['sources'])
        paths.add(shell['model']['evidence'])
        desktop = json.loads((ROOT / 'reports/desktop-state-status.json').read_text())
        paths.add(desktop['model']['evidence'])
        baseline = json.loads((ROOT / 'reports/baseline-status.json').read_text())
        paths.update(item['path'] for item in baseline['artifacts'])
        clean = json.loads((ROOT / 'reports/clean-build-status.json').read_text())
        paths.update(item['path'] for item in clean['artifacts'])
        for pointer in ('reports/app/last-run.json', 'reports/emulator/last-run.json'):
            active = json.loads((ROOT / pointer).read_text())
            paths.add(active['report_directory'] + '/result.json')
            if 'build_provenance' in active:
                paths.update(active['build_provenance'][key] for key in ('retained_hap', 'retained_provenance', 'retained_inputs'))
        # Capture the same source snapshot in each isolated local fixture so
        # provenance checks remain real file/hash comparisons, never mocked green.
        import zipfile
        with zipfile.ZipFile(ROOT / 'out/app/entry-default-unsigned.hap') as package:
            manifest = json.loads(package.read('resources/rawfile/OLEX2-BUILD-INPUTS.json'))
        paths.update(item['path'] for item in manifest['files'] if item['origin'] == 'workspace')
        paths.update(item['path'] for item in manifest['metadata'])
        # Schema 2 binds source archives and actual unstripped library outputs.
        evidence_roots=[ROOT/'out/app/build-evidence']
        active=json.loads((ROOT/'reports/app/last-run.json').read_text())
        if active.get('build_provenance',{}).get('verification',{}).get('schema_version')==2:
            evidence_roots.append(ROOT/active['report_directory']/'build-evidence')
        for evidence_root in evidence_roots:
            paths.update(str(item.relative_to(ROOT)) for item in evidence_root.rglob('*') if item.is_file())
        for path in paths:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / path, target)

    def change(self, path, edit):
        target = self.root / path
        value = json.loads(target.read_text())
        edit(value)
        target.write_text(json.dumps(value))

    def run_checker(self, expected_code, failed_check=None):
        run = subprocess.run([sys.executable, str(self.root / 'scripts/check-adaptation-status.py')],
                             capture_output=True, text=True, check=False)
        self.assertEqual(run.returncode, expected_code, run.stdout + run.stderr)
        report = json.loads((self.root / 'reports/adaptation-consistency.json').read_text())
        self.assertEqual(report['passed'], expected_code == 0)
        if failed_check:
            self.assertIn(failed_check, [c['name'] for c in report['checks'] if not c['passed']])

    def test_unchanged_isolated_copy_passes(self):
        self.run_checker(0)

    def test_synthetic_stale_artifact_hash_is_rejected(self):
        self.change('reports/baseline-status.json',
                    lambda r: r['artifacts'][0].update(sha256='0' * 64))
        baseline=json.loads((self.root/'reports/baseline-status.json').read_text())
        self.run_checker(1, 'baseline_artifact:'+baseline['artifacts'][0]['path'])

    def test_synthetic_stale_version_is_rejected(self):
        self.change('reports/submission-status.json', lambda r: r.update(version='0.0.0-synthetic'))
        self.run_checker(1, 'source_package_summary_versions_match')

    def test_synthetic_missing_input_events_are_rejected(self):
        pointer = 'reports/app/last-run.json'
        evidence = json.loads((self.root / pointer).read_text())['report_directory'] + '/result.json'
        for path in (pointer, evidence):
            self.change(path, lambda r: r['checks'][next(i for i, c in enumerate(r['checks']) if c['name'] == 'injected_mouse_wheel_zoom')]['details'].update(axisEvents=0))
        self.run_checker(1, 'injected_input_evidence_present')

    def test_synthetic_same_process_restart_is_rejected(self):
        pointer = 'reports/app/last-run.json'
        evidence = json.loads((self.root / pointer).read_text())['report_directory'] + '/result.json'
        def edit(report):
            details = next(c['details'] for c in report['checks'] if c['name'] == 'workspace_cold_restart_new_process')
            details['new_pid'] = details['prior_pid']
        for path in (pointer, evidence):
            self.change(path, edit)
        self.run_checker(1, 'cold_restart_uses_distinct_process')

    def test_synthetic_wrong_color_pixels_are_rejected(self):
        # Explicitly synthetic copies only; change both pointers and the sample
        # summary to ensure independent pixel math, not simple equality, rejects it.
        pointer = 'reports/app/last-run.json'
        evidence = json.loads((self.root / pointer).read_text())['report_directory'] + '/result.json'
        name = 'color_primitive_0_preset_1'
        def edit(report):
            g = next(c['details'] for c in report['checks'] if c['name'] == name)
            g.update(readbackR=14, readbackG=20, readbackB=26)
        for path in (pointer, evidence):
            self.change(path, edit)
        self.change('reports/color-adapter-status.json',
                    lambda r: r['app']['samples'][name].update(readbackR=14, readbackG=20, readbackB=26))
        self.run_checker(1, name + '_actual_pixel_matches')

    def test_synthetic_changed_native_color_source_is_rejected(self):
        with (self.root / 'native/upstream_color.cpp').open('a') as f:
            f.write('\n// isolated synthetic mutation\n')
        self.run_checker(1, 'color_native_tests_match_source_and_target')

    def test_synthetic_unchanged_material_pixels_are_rejected(self):
        pointer='reports/app/last-run.json'
        evidence=json.loads((self.root / pointer).read_text())['report_directory']+'/result.json'
        prefix='material_primitive_0_color_0'
        def edit(report):
            checks={c['name']:c for c in report['checks']}
            classic=checks[prefix+'_preset_0']['details']
            fields=['readbackR','readbackG','readbackB','rowMeanScale','rowMinScale','rowMaxScale']
            patch={k:classic[k] for k in fields}
            checks[prefix+'_preset_1']['details'].update(patch)
            checks[prefix+'_lighting_response']['details'][1].update(patch)
        for path in (pointer,evidence): self.change(path,edit)
        copied=json.loads((self.root / pointer).read_text())
        soft=next(c['details'] for c in copied['checks'] if c['name']==prefix+'_preset_1')
        response=next(c['details'] for c in copied['checks'] if c['name']==prefix+'_lighting_response')
        def edit_summary(r):
            r['app']['samples'][prefix+'_preset_1']=soft
            r['app']['lighting_responses'][prefix+'_lighting_response']=response
        self.change('reports/material-adapter-status.json',edit_summary)
        self.run_checker(1,prefix+'_lighting_response')

    def test_synthetic_changed_material_source_is_rejected(self):
        with (self.root/'native/upstream_material.cpp').open('a') as f:
            f.write('\n// isolated synthetic source change\n')
        self.run_checker(1,'material_native_source_and_assertions_match')

    def test_synthetic_clipped_controls_are_rejected(self):
        pointer='reports/app/last-run.json'
        evidence=json.loads((self.root / pointer).read_text())['report_directory']+'/result.json'
        def edit(report):
            d=next(c['details'] for c in report['checks'] if c['name']=='small_window_view_controls_fully_visible')
            d['reset'][3]=d['panel'][3]+10
        for path in (pointer,evidence): self.change(path,edit)
        self.run_checker(1,'small_window_controls_inside_panel')

    def test_synthetic_missing_actual_material_commands_are_rejected(self):
        pointer='reports/app/last-run.json'
        evidence=json.loads((self.root/pointer).read_text())['report_directory']+'/result.json'
        name='original_material_commands_drive_uniforms'
        def edit(report):
            next(c['details'] for c in report['checks'] if c['name']==name)['materialCommandCount']=0
        for path in (pointer,evidence): self.change(path,edit)
        self.change('reports/material-adapter-status.json',lambda r:r['commands']['app_sample'].update(materialCommandCount=0))
        self.run_checker(1,'material_commands_drive_actual_uniforms_and_pixels')

    def test_synthetic_changed_portable_command_source_is_rejected(self):
        with (self.root/'upstream/glib/materialcommands.h').open('a') as f:
            f.write('\n// isolated synthetic command flow change\n')
        self.run_checker(1,'material_native_source_and_assertions_match')

    def test_synthetic_wrong_target_command_case_count_is_rejected(self):
        pointer='reports/emulator/last-run.json'
        evidence=json.loads((self.root/pointer).read_text())['report_directory']+'/result.json'
        for path in (pointer,evidence):
            self.change(path,lambda r:r['upstream_material_commands'].update(flag_state_cases_passed=0))
        self.change('reports/material-adapter-status.json',lambda r:r['commands']['ohos'].update(flag_state_cases_passed=0))
        self.run_checker(1,'material_command_host_and_target_assertions_match')

    def test_synthetic_glossy_wrong_raw_pixels_are_rejected(self):
        pointer='reports/app/last-run.json';evidence=json.loads((self.root/pointer).read_text())['report_directory']+'/result.json'
        name='glossy_primitive_0_color_0'
        def edit(report):next(c['details'] for c in report['checks'] if c['name']==name)['specularSamples'][1][3]=0
        for path in (pointer,evidence):self.change(path,edit)
        self.change('reports/material-adapter-status.json',lambda r:r['lit']['samples'][name]['specularSamples'][1].__setitem__(3,0))
        self.run_checker(1,name+'_reference_pixels_match')

    def test_synthetic_glossy_shininess_not_applied_is_rejected(self):
        pointer='reports/app/last-run.json';evidence=json.loads((self.root/pointer).read_text())['report_directory']+'/result.json';name='glossy_primitive_0_color_0'
        for path in (pointer,evidence):self.change(path,lambda r:next(c['details'] for c in r['checks'] if c['name']==name).update(shininess=0))
        self.change('reports/material-adapter-status.json',lambda r:r['lit']['samples'][name].update(shininess=0))
        self.run_checker(1,name+'_reference_pixels_match')

    def test_synthetic_glossy_same_process_restart_is_rejected(self):
        pointer='reports/app/last-run.json';evidence=json.loads((self.root/pointer).read_text())['report_directory']+'/result.json'
        def edit(r):
            p=next(c['details'] for c in r['checks'] if c['name']=='glossy_cold_restart_new_process');p['new_pid']=p['prior_pid']
        for path in (pointer,evidence):self.change(path,edit)
        self.run_checker(1,'glossy_new_process_restores_actual_material_pixels')

    def test_synthetic_full_port_claim_is_rejected(self):
        self.change('reports/baseline-status.json', lambda r: r.update(full_port_complete=True))
        self.run_checker(1)


    def test_synthetic_original_window_same_pid_across_cold_starts_is_rejected(self):
        pointer='reports/app/last-run.json'
        active=json.loads((self.root/pointer).read_text())
        prior=next(c['details']['pid'] for c in active['checks'] if c['name']=='shared_original_initial')
        for path in (pointer,active['report_directory']+'/result.json'):
            self.change(path,lambda r:next(c['details'] for c in r['checks'] if c['name']=='shared_original_after-application-runtime-restart').update(pid=prior))
        self.run_checker(1,'original_synthetic_window_summary_and_continuity_match')

    def test_synthetic_missing_provenance_summary_is_rejected(self):
        self.change('reports/app/package-verification.json',
                    lambda r: r.update(build_provenance_verified=False))
        self.run_checker(1, 'build_provenance_summary_matches_direct_check')

    def test_synthetic_mismatched_build_hap_digest_is_rejected(self):
        self.change('out/app/entry-default-unsigned.provenance.json',
                    lambda r: r['hap'].update(sha256='0' * 64))
        self.run_checker(1, 'build_provenance_matches_current_hap_and_sources')

    def test_synthetic_wrong_native_compilation_count_is_rejected(self):
        self.change('reports/app/package-verification.json',
                    lambda r: r['build_provenance'].update(compiled_native_sources=1))
        self.run_checker(1, 'build_provenance_summary_matches_direct_check')


    def test_synthetic_missing_retained_build_inputs_are_rejected(self):
        app = json.loads((self.root / 'reports/app/last-run.json').read_text())
        (self.root / app['build_provenance']['retained_inputs']).write_text('isolated changed capture')
        self.run_checker(1, 'runtime_retains_exact_tested_build_artifacts')


    def change_triangle_upload(self, edit):
        pointer='reports/app/last-run.json'
        current=json.loads((self.root/pointer).read_text())
        evidence=current['report_directory']+'/result.json'
        name='original_triangle_branch_drives_actual_upload'
        for path in (pointer,evidence):
            self.change(path,lambda r: edit(next(c['details'] for c in r['checks'] if c['name']==name)))
        self.change('reports/original-triangle-status.json',lambda r: edit(r['app']['samples'][name]))

    def test_synthetic_wrong_original_normal_count_is_rejected(self):
        self.change_triangle_upload(lambda g:g.update(originalTriangleNormalCommands=0))
        self.run_checker(1,'original_triangle_upload:original_triangle_branch_drives_actual_upload')

    def test_synthetic_unimplemented_gpu_color_commands_are_rejected(self):
        self.change_triangle_upload(lambda g:g.update(originalTriangleColorCommands=1))
        self.run_checker(1,'original_triangle_upload:original_triangle_branch_drives_actual_upload')

    def test_synthetic_original_source_equivalence_claim_is_rejected(self):
        self.change('reports/original-triangle-status.json',lambda r:r.update(original_flow_preserved_exactly=False))
        self.run_checker(1,'original_triangle_source_and_wrapper_snapshots_match')

    def test_synthetic_gpu_color_scope_overclaim_is_rejected(self):
        self.change('reports/original-triangle-status.json',lambda r:r.update(gpu_vertex_color_tracking_implemented=True))
        self.run_checker(1,'original_triangle_target_scope_not_overstated')

    def test_synthetic_original_branch_case_count_is_rejected(self):
        self.change('reports/original-triangle-status.json',lambda r:r['host'].update(branch_state_cases_passed=1))
        self.run_checker(1,'original_triangle_host_and_target_assertions_match')

    def test_synthetic_original_adapter_source_drift_is_rejected(self):
        path=self.root/'native/primitive_triangles.cpp'
        path.write_text(path.read_text()+'\n// isolated synthetic drift\n')
        self.run_checker(1,'original_triangle_source_and_wrapper_snapshots_match')

    def test_synthetic_missing_matrix_sample_is_rejected(self):
        self.change('reports/original-matrix-status.json',lambda r:r['app']['samples'].pop('native_window_resize'))
        self.run_checker(1,'original_matrix_sample_inventory_matches_scope')

    def test_synthetic_matrix_scope_overclaim_is_rejected(self):
        self.change('reports/original-matrix-status.json',lambda r:r.update(scale_shear_gpu_tested=True))
        self.run_checker(1,'original_matrix_scope_not_overstated')

    def test_synthetic_matrix_static_getter_thread_safety_claim_is_rejected(self):
        self.change('reports/original-matrix-status.json',lambda r:r.update(legacy_basis_globally_thread_safe=True))
        self.run_checker(1,'original_matrix_scope_not_overstated')

    def test_synthetic_matrix_pose_count_is_rejected(self):
        self.change('reports/original-matrix-status.json',lambda r:r['host'].update(independent_camera_poses_passed=1))
        self.run_checker(1,'original_matrix_host_and_target_assertions_match')

    def test_synthetic_matrix_source_drift_is_rejected(self):
        path=self.root/'native/original_matrices.cpp';path.write_text(path.read_text()+'\n// isolated synthetic drift\n')
        self.run_checker(1,'original_matrix_exports_and_source_snapshots_match')

    def test_synthetic_matrix_gpu_readback_corruption_is_rejected(self):
        name='original_basis_drives_actual_matrix_uniforms'
        def edit(g):g['mvpMatrix'][0]+=.1
        pointer='reports/app/last-run.json'
        current=json.loads((self.root/pointer).read_text())
        evidence=current['report_directory']+'/result.json'
        for path in (pointer,evidence):
            self.change(path,lambda r:edit(next(c['details'] for c in r['checks'] if c['name']==name)))
        self.change('reports/original-matrix-status.json',lambda r:edit(r['app']['samples'][name]))
        self.run_checker(1,'original_matrix_sample:'+name)

    def test_synthetic_polygon_glu_scope_overclaim_is_rejected(self):
        self.change('reports/original-polygon-status.json',lambda r:r.update(full_glu_implemented=True))
        self.run_checker(1,'original_polygon_scope_not_overstated')

    def test_synthetic_polygon_holes_scope_overclaim_is_rejected(self):
        self.change('reports/original-polygon-status.json',lambda r:r.update(holes_supported=True))
        self.run_checker(1,'original_polygon_scope_not_overstated')

    def test_synthetic_polygon_source_drift_is_rejected(self):
        path=self.root/'native/primitive_polygons.cpp';path.write_text(path.read_text()+'\n// isolated synthetic drift\n')
        self.run_checker(1,'original_polygon_source_and_snapshots_match')

    def test_synthetic_polygon_wrong_case_count_is_rejected(self):
        self.change('reports/original-polygon-status.json',lambda r:r['host'].update(independent_topology_poses_passed=1))
        self.run_checker(1,'original_polygon_host_and_target_assertions_match')

    def test_synthetic_polygon_missing_sample_is_rejected(self):
        self.change('reports/original-polygon-status.json',lambda r:r['app']['samples'].pop('primitive_2_detail_0_solid'))
        self.run_checker(1,'original_polygon_sample_inventory_matches_scope')

    def test_synthetic_polygon_fan_notch_fill_is_rejected(self):
        name='original_polygon_branch_and_concave_gpu_tessellation'
        def edit(g):g['polygonProbeSamples'][3][3:]=[64,191,128,255]
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(next(c['details'] for c in r['checks'] if c['name']==name)))
        self.change('reports/original-polygon-status.json',lambda r:edit(r['app']['samples'][name]))
        self.run_checker(1,'original_polygon_sample:'+name)


    def change_synthetic_command_sample(self,name,edit):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(next(c['details'] for c in r['checks'] if c['name']==name)))
        self.change('reports/original-desktop-command-status.json',lambda r:edit(r['app']['samples'][name]))

    def test_synthetic_command_missing_sample_is_rejected(self):
        self.change('reports/original-desktop-command-status.json',lambda r:r['app']['samples'].pop('desktop_command_chain'))
        self.run_checker(1,'original_desktop_command_sample_inventory_matches_scope')

    def test_synthetic_command_callback_count_is_rejected(self):
        name='desktop_command_chain'
        self.change_synthetic_command_sample(name,lambda r:r['desktopCommand'].update(dispatched=1))
        self.run_checker(1,'original_desktop_command_sample:'+name)

    def test_synthetic_command_intent_instead_of_actual_view_is_rejected(self):
        name='desktop_command_view'
        self.change_synthetic_command_sample(name,lambda r:r['graphics'].update(yaw=32))
        self.run_checker(1,'original_desktop_command_sample:'+name)

    def test_synthetic_command_failed_chain_mutation_is_rejected(self):
        name='desktop_command_failed_chain_unchanged'
        self.change_synthetic_command_sample(name,lambda r:r['graphics'].update(pitch=-22))
        self.run_checker(1,'original_desktop_command_sample:'+name)

    def test_synthetic_command_failed_chain_commit_is_rejected(self):
        name='desktop_command_failed_chain_unchanged'
        self.change_synthetic_command_sample(name,lambda r:r['desktopCommand'].update(applied=True))
        self.run_checker(1,'original_desktop_command_sample:'+name)

    def test_synthetic_command_source_drift_is_rejected(self):
        p=self.root/'native/desktop_commands.cpp';p.write_text(p.read_text()+'\n// isolated synthetic source drift\n')
        self.run_checker(1,'original_desktop_command_sources_match')

    def test_synthetic_command_scope_overclaims_are_rejected(self):
        self.change('reports/original-desktop-command-status.json',lambda r:r.update(complete_original_command_library_integrated=True))
        self.run_checker(1,'original_desktop_command_scope_not_overstated')

    def test_synthetic_command_history_replay_claim_is_rejected(self):
        self.change('reports/original-desktop-command-status.json',lambda r:r.update(command_history_replayed=True))
        self.run_checker(1,'original_desktop_command_scope_not_overstated')

    def test_synthetic_command_combination_count_is_rejected(self):
        self.change('reports/original-desktop-command-status.json',lambda r:r['host'].update(independent_view_combinations_passed=1))
        self.run_checker(1,'original_desktop_command_host_and_target_match')

    def test_synthetic_command_literal_observation_is_rejected(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        def edit(o):o['desktop_command_detail']['actual_committed_text']='pc.detail2'
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(r['desktop_command_input_observations']))
        self.change('reports/original-desktop-command-status.json',lambda r:edit(r['app']['input_observations']))
        self.run_checker(1,'original_desktop_command_literal_input_observations_match')


    def change_synthetic_application_sample(self,name,edit):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(next(c['details'] for c in r['checks'] if c['name']==name)))
        self.change('reports/original-application-runtime-status.json',lambda r:edit(r['app']['samples'][name]))

    def test_synthetic_application_missing_platform_patch_is_rejected(self):
        self.change('source.lock.json',lambda r:r['patches'].pop())
        self.run_checker(1,'material_command_flow_matches_original_source')

    def test_synthetic_application_missing_sample_is_rejected(self):
        self.change('reports/original-application-runtime-status.json',lambda r:r['app']['samples'].pop('application_function_config_dir'))
        self.run_checker(1,'original_application_function_inventory_matches_scope')

    def test_synthetic_application_registry_count_is_rejected(self):
        name='application_function_config_dir'
        self.change_synthetic_application_sample(name,lambda r:r['applicationRuntime'].update(originalFunctionCount=123))
        self.run_checker(1,'original_application_function_sample:'+name)

    def test_synthetic_application_bad_license_is_rejected(self):
        name='application_function_config_dir'
        self.change_synthetic_application_sample(name,lambda r:r['applicationRuntime'].update(licenseSha256='0'*64))
        self.run_checker(1,'original_application_function_sample:'+name)

    def test_synthetic_application_setting_without_file_change_is_rejected(self):
        name='application_function_hide_hints';current=json.loads((self.root/'reports/app/last-run.json').read_text());sha=current['application_function_before_samples'][name]['applicationRuntime']['optionsSha256']
        self.change_synthetic_application_sample(name,lambda r:r['applicationRuntime'].update(optionsSha256=sha))
        self.run_checker(1,'original_application_function_sample:'+name)

    def test_synthetic_application_scope_overclaim_is_rejected(self):
        self.change('reports/original-application-runtime-status.json',lambda r:r.update(all_original_application_functions_runtime_tested=True))
        self.run_checker(1,'original_application_runtime_scope_not_overstated')

    def test_synthetic_application_resource_tree_overclaim_is_rejected(self):
        self.change('reports/original-application-runtime-status.json',lambda r:r.update(complete_original_resource_tree_integrated=True))
        self.run_checker(1,'original_application_runtime_scope_not_overstated')

    def test_synthetic_application_global_environment_change_is_rejected(self):
        self.change('reports/original-application-runtime-status.json',lambda r:r.update(process_environment_modified=True))
        self.run_checker(1,'original_application_runtime_scope_not_overstated')

    def test_synthetic_application_hash_counter_source_drift_is_rejected(self):
        p=self.root/'upstream/sdl/ehashed.h';p.write_text(p.read_text()+'\n// isolated synthetic source drift\n')
        self.run_checker(1,'original_application_runtime_sources_match')

    def test_synthetic_application_wrong_round_trip_count_is_rejected(self):
        self.change('reports/original-application-runtime-status.json',lambda r:r['host'].update(original_option_round_trips_passed=1))
        self.run_checker(1,'original_application_runtime_host_and_target_match')

    def test_synthetic_application_replayed_history_is_rejected(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        def edit(r):r['application_runtime_after_restart']['applicationFunction']['sequence']=13
        for path in (pointer,current['report_directory']+'/result.json'):self.change(path,edit)
        self.change('reports/original-application-runtime-status.json',lambda r:r['app']['after_restart']['applicationFunction'].update(sequence=13))
        self.run_checker(1,'original_application_runtime_cold_process_recovery_matches')

    def test_synthetic_application_literal_input_is_rejected(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        def edit(o):o['application_function_config_dir']['actual_committed_text']='app.ConfigDir'
        for path in (pointer,current['report_directory']+'/result.json'):self.change(path,lambda r:edit(r['application_function_input_observations']))
        self.change('reports/original-application-runtime-status.json',lambda r:edit(r['app']['input_observations']))
        self.run_checker(1,'original_application_runtime_literal_input_observations_match')


    def test_synthetic_document_full_workflow_overclaim_is_rejected(self):
        self.change('reports/options-document-status.json',lambda r:r.update(complete_application_document_open_save_supported=True))
        self.run_checker(1,'options_document_scope_not_overstated')

    def test_synthetic_document_external_atomicity_overclaim_is_rejected(self):
        self.change('reports/options-document-status.json',lambda r:r.update(external_destination_atomicity_claimed=True))
        self.run_checker(1,'options_document_scope_not_overstated')

    def test_synthetic_document_wrong_native_count_is_rejected(self):
        self.change('reports/options-document-status.json',lambda r:r['host'].update(assertions_passed=1))
        self.run_checker(1,'options_document_host_and_target_match')

    def test_synthetic_document_missing_system_sample_is_rejected(self):
        self.change('reports/options-document-status.json',lambda r:r['app']['samples'].pop('options_document_imported_snapshot'))
        self.run_checker(1,'options_document_inventory_matches_scope')

    def test_synthetic_document_source_drift_is_rejected(self):
        p=self.root/'app/entry/src/main/ets/model/OptionsDocumentTransfer.ts';p.write_text(p.read_text()+'\n// isolated synthetic source drift\n')
        self.run_checker(1,'options_document_sources_and_utf8_patch_match')

    def test_synthetic_document_replayed_operation_is_rejected(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:r['options_document_after_restart']['optionsDocument'].update(sequence=4))
        self.change('reports/options-document-status.json',lambda r:r['app']['after_restart']['optionsDocument'].update(sequence=4))
        self.run_checker(1,'options_document_distinct_process_restore_without_replay')

    def test_synthetic_active_view_cannot_hide_as_an_inactive_document_page(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:r['application_function_before_samples']['application_function_base_dir']['graphics'].update(surfaceReady=False,initialized=False))
        self.run_checker(1,'inactive_graphics_only_document_page')

    def test_synthetic_inactive_surface_gpu_overclaim_is_rejected(self):
        pointer='reports/app/last-run.json';current=json.loads((self.root/pointer).read_text())
        for path in (pointer,current['report_directory']+'/result.json'):
            self.change(path,lambda r:r['options_document_exported']['graphics'].update(matrixUniformVerified=True))
        self.run_checker(1,'inactive_surface_graphics_are_explicitly_unverified')

    def test_synthetic_line_gpu_width_overclaim_is_rejected(self):
        self.change('reports/original-line-status.json',lambda r:r.update(gpu_wide_line_state_implemented=True))
        self.run_checker(1,'original_line_scope_not_overstated')

    def test_synthetic_line_native_count_is_rejected(self):
        self.change('reports/original-line-status.json',lambda r:r['host'].update(assertions_passed=1))
        self.run_checker(1,'original_line_host_and_target_assertions_match')

    def test_synthetic_line_source_drift_is_rejected(self):
        p=self.root/'native/primitive_lines.cpp';p.write_text(p.read_text()+'\n// isolated synthetic source drift\n')
        self.run_checker(1,'original_line_sources_and_exact_upstream_flow_match')

    def test_synthetic_line_ebo_digest_change_is_rejected(self):
        current=json.loads((self.root/'reports/app/last-run.json').read_text())
        for path in ('reports/app/last-run.json',current['report_directory']+'/result.json'):
            self.change(path,lambda r:next(c['details'] for c in r['checks'] if c['name']=='mesh_wireframe_mode').update(originalLineIndexSha256='0'*64))
        self.run_checker(1,'all_retained_active_graphic_states_have_original_line_ebo_validation')

    def test_synthetic_line_inactive_gpu_proof_is_rejected(self):
        current=json.loads((self.root/'reports/app/last-run.json').read_text())
        for path in ('reports/app/last-run.json',current['report_directory']+'/result.json'):
            self.change(path,lambda r:r['options_document_exported']['graphics'].update(originalLineBufferVerified=True))
        self.run_checker(1,'inactive_surfaces_have_no_current_line_buffer_proof')

    def test_synthetic_line_stale_app_count_is_rejected(self):
        self.change('reports/original-line-status.json',lambda r:r['app'].update(checks_passed=1))
        self.run_checker(1,'original_line_app_evidence_matches_current_package')

    def test_synthetic_line_target_digest_log_change_is_rejected(self):
        lines=json.loads((self.root/'reports/original-line-status.json').read_text())
        p=self.root/lines['ohos']['test_log'];p.write_text(p.read_text().replace('LINE_INDICES sphere 0','LINE_INDICES sphere 1',1))
        self.run_checker(1,'original_line_24_cpu_index_contracts_independently_validated')


    def test_synthetic_core_macro_overcount_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r['app']['baseline']['applicationRuntime'].__setitem__('originalCoreMacroCount',136))
        self.run_checker(1,'original_core_actual_app_snapshot_inventory_matches')

    def test_synthetic_core_command_exposure_overclaim_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r.__setitem__('all_core_registry_commands_exposed_to_ui',True))
        self.run_checker(1,'original_core_scope_not_overstated')

    def test_synthetic_core_registry_behavior_overclaim_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r.__setitem__('all_core_registry_commands_runtime_tested',True))
        self.run_checker(1,'original_core_scope_not_overstated')

    def test_synthetic_core_cpp_source_drift_is_rejected(self):
        path=self.root/'cmake/Olex2Core.cmake';path.write_text(path.read_text()+'\n# isolated synthetic drift\n')
        self.run_checker(1,'original_core_sources_match')

    def test_synthetic_core_production_source_omission_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r['compiled_core_sources'].pop())
        self.run_checker(1,'original_core_production_compilation_matches')

    def test_synthetic_core_lifecycle_overcount_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r['host'].__setitem__('repeated_lifecycle_cases_passed',9))
        self.run_checker(1,'original_core_host_and_target_lifecycle_match')

    def test_synthetic_core_snapshot_inventory_drift_is_rejected(self):
        self.change('reports/original-core-runtime-status.json',lambda r:r['app'].__setitem__('runtime_snapshot_count',0))
        self.run_checker(1,'original_core_actual_app_snapshot_inventory_matches')

    def change_path_probe(self, edit):
        current=json.loads((self.root/'reports/app/last-run.json').read_text())
        for path in ('reports/app/last-run.json',current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(next(c['details']['linePathProbe'] for c in r['checks'] if c['name']=='original_line_path_gpu_probe')))

    def test_synthetic_path_actual_ebo_digest_is_rejected(self):
        self.change_path_probe(lambda p:p['cases'][0].__setitem__('indexSha256','0'*64))
        self.run_checker(1,'all_active_line_path_gpu_probes_match_independent_ebo_pixels')

    def test_synthetic_path_strip_false_closing_pixels_are_rejected(self):
        self.change_path_probe(lambda p:p['cases'][0]['samples'][3].__setitem__('rgba',[68,136,204,255]*9))
        self.run_checker(1,'all_active_line_path_gpu_probes_match_independent_ebo_pixels')

    def test_synthetic_path_inactive_gpu_proof_is_rejected(self):
        current=json.loads((self.root/'reports/app/last-run.json').read_text())
        for path in ('reports/app/last-run.json',current['report_directory']+'/result.json'):
            self.change(path,lambda r:r['options_document_exported']['graphics']['linePathProbe'].__setitem__('verified',True))
        self.run_checker(1,'inactive_line_path_gpu_proof_is_cleared')

    def test_synthetic_path_native_count_is_rejected(self):
        self.change('reports/original-line-path-status.json',lambda r:r['host'].__setitem__('assertions_passed',181511))
        self.run_checker(1,'original_line_path_host_and_target_match')

    def test_synthetic_path_source_drift_is_rejected(self):
        p=self.root/'native/primitive_line_paths.cpp';p.write_text(p.read_text()+'\n// isolated synthetic source drift\n')
        self.run_checker(1,'original_line_path_sources_and_flow_match')

    def test_synthetic_path_stale_sample_inventory_is_rejected(self):
        self.change('reports/original-line-path-status.json',lambda r:r['app'].__setitem__('active_graphic_snapshots',0))
        self.run_checker(1,'original_line_path_saved_sample_inventory_matches')

    def test_synthetic_path_gpu_width_overclaim_is_rejected(self):
        self.change('reports/original-line-path-status.json',lambda r:r.__setitem__('gpu_wide_line_state_implemented',True))
        self.run_checker(1,'original_line_path_scope_not_overstated')

    def test_synthetic_path_gpu_color_overclaim_is_rejected(self):
        self.change('reports/original-line-path-status.json',lambda r:r.__setitem__('gpu_vertex_color_tracking_implemented',True))
        self.run_checker(1,'original_line_path_scope_not_overstated')

    def test_synthetic_log_source_drift_is_rejected(self):
        p=self.root/'native/original_log.cpp';p.write_text(p.read_text()+'\n// isolated synthetic drift\n')
        self.run_checker(1,'original_log_source_snapshot_matches')

    def test_synthetic_log_native_count_is_rejected(self):
        self.change('reports/original-log-status.json',lambda r:r['host'].__setitem__('assertions_passed',1653))
        self.run_checker(1,'original_log_host_and_target_notifications_match')

    def change_log_sample(self,name,edit):
        current=json.loads((self.root/'reports/app/last-run.json').read_text())
        for path in ('reports/app/last-run.json',current['report_directory']+'/result.json'):
            self.change(path,lambda r:edit(next(c['details'] for c in r['checks'] if c['name']==name)))

    def test_synthetic_log_visible_text_change_is_rejected(self):
        self.change_log_sample('original_log_macros_visible_in_ui',lambda d:d.__setitem__('visible_text','synthetic fabricated catalog'))
        self.run_checker(1,'original_log_actual_ui_text_and_clear_restart_match')

    def test_synthetic_log_clear_sequence_reset_is_rejected(self):
        self.change_log_sample('original_log_clear_preserves_sequence',lambda d:d['after'].__setitem__('posted',0))
        self.run_checker(1,'original_log_actual_ui_text_and_clear_restart_match')

    def test_synthetic_log_same_process_restart_is_rejected(self):
        self.change_log_sample('original_log_cold_restart_without_replay',lambda d:d.__setitem__('new_pid',d['prior_pid']))
        self.run_checker(1,'original_log_actual_ui_text_and_clear_restart_match')

    def test_synthetic_log_budget_overclaim_is_rejected(self):
        self.change('reports/original-log-status.json',lambda r:r['limits_config'].__setitem__('max_entries',257))
        self.run_checker(1,'original_log_budgets_and_call_boundary_match')

    def test_synthetic_log_global_thread_safety_overclaim_is_rejected(self):
        self.change('reports/original-log-status.json',lambda r:r.__setitem__('global_legacy_thread_safety_claimed',True))
        self.run_checker(1,'original_log_scope_not_overstated')

    def test_synthetic_log_third_core_macro_exposure_is_rejected(self):
        self.change('reports/original-log-status.json',lambda r:r['exposed_original_core_metadata_macros'].append('core.SyntheticExtra'))
        self.run_checker(1,'original_log_scope_not_overstated')

    def test_synthetic_shell_stale_installed_identity_rejected(self):
        self.change('reports/app/last-run.json',lambda a:a['startup_report']['installedPackage'].__setitem__('versionName','0.14.0'))
        self.run_checker(1,'desktop_shell_all_installed_identities_match')

    def test_synthetic_shell_stale_visible_version_rejected(self):
        self.change_log_sample('installed_package_version_visible',lambda d:d.__setitem__('visible_text','开发验证版  /  0.14.0'))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

    def test_synthetic_shell_truncated_command_label_rejected(self):
        self.change_log_sample('desktop_command_labels_visible',lambda d:d['view'].__setitem__('text','执…'))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

    def test_synthetic_shell_wrong_enter_interpreter_rejected(self):
        self.change_log_sample('desktop_enter_application_interpreter',lambda d:d['after']['desktopCommand'].__setitem__('sequence',d['before']['desktopCommand']['sequence']+1))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

    def test_synthetic_shell_enter_sequence_not_advanced_rejected(self):
        self.change_log_sample('desktop_enter_view_interpreter',lambda d:d['after']['desktopCommand'].__setitem__('sequence',d['before']['desktopCommand']['sequence']))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

    def test_synthetic_shell_enter_settings_changed_rejected(self):
        self.change_log_sample('desktop_enter_application_interpreter',lambda d:d['after']['applicationRuntime'].__setitem__('commandHints',not d['before']['applicationRuntime']['commandHints']))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

    def test_synthetic_shell_physical_keyboard_overclaim_rejected(self):
        self.change('reports/desktop-shell-status.json',lambda d:d.__setitem__('physical_keyboard_verified',True))
        self.run_checker(1,'desktop_shell_scope_not_overstated')

    def test_synthetic_shell_fake_drawing_paused_status_rejected(self):
        self.change_log_sample('drawing_paused_visible_in_records',lambda d:d.__setitem__('visible_text','像素读回通过'))
        self.run_checker(1,'desktop_shell_actual_enter_identity_layout_pause_match')

if __name__ == '__main__':
    unittest.main(verbosity=2)
