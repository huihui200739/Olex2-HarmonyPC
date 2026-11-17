#!/usr/bin/env python3
"""Check current local summaries against retained runtime evidence; not a runtime test."""
from datetime import datetime
import hashlib
import json
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance import verify as verify_build_provenance
from line_validation import original_line_upload_matches,inactive_line_proof_cleared,line_command_log_matches,app_line_evidence_matches
from matrix_validation import original_matrices_match
from polygon_validation import polygon_probe_matches,original_polygon_upload_matches
from desktop_command_validation import SAMPLES,command_sample_matches,command_recovery_matches
from application_runtime_validation import SAMPLES as APPLICATION_SAMPLES,app_core_evidence_matches,runtime_snapshots,runtime_matches,function_sample_matches,app_runtime_evidence_matches

ROOT = Path(__file__).resolve().parents[1]
report = {
    'checked_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),
    'scope': 'Local evidence consistency only; does not run the app or certify the full port',
    'checks': [], 'full_port_complete': False, 'physical_pc_verified': False,
}


def local(path):
    resolved = (ROOT / path).resolve()
    if not resolved.is_relative_to(ROOT):
        raise ValueError(f'Path outside this project: {path}')
    return resolved


def load(path):
    return json.loads(local(path).read_text())


def digest(path):
    return hashlib.sha256(local(path).read_bytes()).hexdigest()


def check(name, passed):
    report['checks'].append({'name': name, 'passed': bool(passed)})


def original_triangle_sample_matches(g):
    count=g.get('submittedVertices',0)
    return (type(count) is int and count>0 and count%3==0 and
            g.get('originalTriangleVerified') is True and
            g.get('originalTriangleVertices')==g.get('originalTriangleNormalCommands')==count and
            g.get('originalTriangleColorCommands')==0 and g.get('meshTriangles',0)*3==count and
            g.get('originalTriangleSource')=='upstream/glib/trianglesubmit.h: original TGlPrimitive::Draw sgloTriangles branch' and
            all(g.get(key) is True for key in ('meshValidated','submissionValidated','pixelVerified','geometryVerified','coverageVerified','initialized','surfaceReady')))


def glossy_pixels_match(g,base,highlight):
    if not (g.get('materialPreset')==3 and g.get('materialFlags')==1023 and g.get('shininess')==32 and
            g.get('materialCommandCount')==12 and g.get('materialVerified') is True and
            g.get('specularReferenceVerified') is True and g.get('specularComparedPixels',0)>=4 and
            0<=g.get('specularMaxError',999)<=3 and len(g.get('specularSamples',[]))==4 and
            g.get('geometryColorSamples',0)>=g.get('specularComparedPixels',1) and
            all(abs(g.get(n+c,-1)-v*f)<.00001 for n,f in [('ambient',.3),('diffuse',.4),('specular',.3),('emission',0)] for c,v in zip('RGB',base))):return False
    if highlight and not (g.get('specularChangedPixels',0)>=2 and g.get('specularMaxDelta',0)>2):return False
    prior=-1
    for values in g['specularSamples']:
        if len(values)!=9:return False
        x,diffuse,specular,*rgb=values
        if not (type(x) is int and prior<x<g['width'] and 0<=diffuse<=1.00001 and 0<=specular<=1.00001):return False
        prior=x
        for value,actual,baseline in zip(base,rgb[:3],rgb[3:]):
            predicted_base=value*255*(.3+.4*diffuse)
            predicted=predicted_base+value*255*.3*specular
            if not (type(actual) is int and type(baseline) is int and 0<=actual<=255 and 0<=baseline<=255 and actual+1>=baseline and
                    abs(predicted-actual)<=3.01 and abs(predicted_base-baseline)<=3.01 and
                    max(abs(predicted-actual),abs(predicted_base-baseline))<=g['specularMaxError']+.02 and
                    actual-baseline<=g['specularMaxDelta']+.01):return False
    return True

try:
    app = load('reports/app/last-run.json')
    cli = load('reports/emulator/last-run.json')
    package = load('reports/app/package-verification.json')
    baseline = load('reports/baseline-status.json')
    mesh = load('reports/mesh-adapter-status.json')
    submission = load('reports/submission-status.json')
    primitive = load('reports/primitive-adapter-status.json')
    clean = load('reports/clean-build-status.json')
    desktop = load('reports/desktop-state-status.json')
    coverage = load('reports/port-coverage.json')
    color = load('reports/color-adapter-status.json')
    material = load('reports/material-adapter-status.json')
    triangle = load('reports/original-triangle-status.json')
    matrix = load('reports/original-matrix-status.json')
    polygon = load('reports/original-polygon-status.json')
    commands = load('reports/original-desktop-command-status.json')
    application = load('reports/original-application-runtime-status.json')
    core = load('reports/original-core-runtime-status.json')
    options_document = load('reports/options-document-status.json')
    identity = load('app/AppScope/app.json5')['app']
    checks = {item['name']: item for item in app['checks']}
    version = identity['versionName']
    check('source_package_summary_versions_match', all(v == version for v in (
        package['version'], baseline['native_app_version'], mesh['version'],
        submission['version'], primitive['version'], clean['version'], desktop['version'], color['version'], material['version'], triangle['version'], matrix['version'], polygon['version'], commands['version'], application['version'], options_document['version'])))
    check('current_hap_is_installed_tested_and_summarized',
          package['passed'] and package['matches_installed_and_tested_hap'] and
          len({digest('out/app/entry-default-unsigned.hap'), package['hap_sha256'],
               app['hap']['sha256'], submission['app']['hap_sha256'],
               primitive['app']['hap_sha256'], desktop['app']['hap_sha256'], color['app']['hap_sha256'], material['app']['hap_sha256'], triangle['app']['hap_sha256'], matrix['app']['hap_sha256'], polygon['app']['hap_sha256'], commands['app']['hap_sha256'], application['app']['hap_sha256'], options_document['app']['hap_sha256']}) == 1)
    from original_renderer_integration_validation import summarize_application
    try:
        original_window=summarize_application(app,ROOT,{k:identity[k] for k in ('bundleName','versionName','versionCode')})
        check('original_synthetic_window_summary_and_continuity_match',package.get('original_synthetic_window_continuity_tested') is True and package.get('original_window_integration')==original_window)
    except (ValueError,KeyError,TypeError):
        check('original_synthetic_window_summary_and_continuity_match',False)
    check('app_checks_are_distinct_and_passed', len(checks) == len(app['checks']) == 194 and
          all(item['passed'] for item in app['checks']) and app['result'].startswith('passed:'))
    check('all_summary_app_counts_match', all(n == len(checks) for n in (
        mesh['app_checks_passed'], submission['app']['checks_passed'],
        primitive['app']['checks_passed'], desktop['app']['checks_passed'], color['app']['checks_passed'], material['app']['checks_passed'], triangle['app']['checks_passed'], matrix['app']['checks_passed'], polygon['app']['checks_passed'], commands['app']['checks_passed'], application['app']['checks_passed'], options_document['app']['checks_passed'])))
    for kind in (1, 2, 3):
        for level in (0, 3, 5):
            for mode in ('solid', 'wire'):
                name = f'primitive_{kind}_detail_{level}_{mode}'
                item = checks[name]
                actual = item['details'].get('graphics', item['details'])
                check(name + '_sample_matches', item['passed'] and
                      primitive['app']['samples'][name] == actual)
    controls=checks['small_window_view_controls_fully_visible']['details']
    panel=controls['panel']
    check('small_window_controls_inside_panel',checks['small_window_view_controls_fully_visible']['passed'] and
          all(panel[0]<=b[0]<=b[2]<=panel[2] and panel[1]<=b[1]<=b[3]<=panel[3] for b in (controls['reset'],controls['zoom'])))
    input_names = ('injected_keyboard_rotate', 'injected_keyboard_zoom',
                   'injected_keyboard_wireframe', 'injected_keyboard_primitive',
                   'injected_keyboard_reset', 'injected_mouse_wheel_zoom')
    check('injected_input_evidence_present', all(checks[name]['passed'] for name in input_names)
          and checks['injected_keyboard_rotate']['details']['keyEvents'] > 0
          and checks['injected_mouse_wheel_zoom']['details']['axisEvents'] > 0)
    restart = checks['workspace_cold_restart_new_process']['details']
    check('cold_restart_uses_distinct_process',
          str(restart['prior_pid']).isdigit() and str(restart['new_pid']).isdigit() and
          restart['prior_pid'] != restart['new_pid'] and int(restart['new_pid']) == app['restart_pid'])
    before, after = app['state_before_restart'], app['state_after_restart']
    check('desktop_state_restore_matches_real_reports',
          package['desktop_state_persistence_tested'] is True and
          before['workspaceState']['saveStatus'] == 'saved' and
          after['workspaceState']['restored'] is True and
          all(before['graphics'][k] == after['graphics'][k] for k in ('meshKind', 'meshGeneration', 'wireframe')) and
          all(abs(before['graphics'][k] - after['graphics'][k]) < .0001 for k in ('yaw', 'pitch', 'distance')) and
          desktop['app']['before_restart'] == before and desktop['app']['after_restart'] == after)
    recreated = checks['color_survives_surface_recreation']['details']
    check('color_recreation_has_actual_pixel_evidence',
          checks['color_survives_surface_recreation']['passed'] and
          recreated == color['app']['surface_recreation_sample'] and
          recreated['colorPreset'] == 2 and recreated['colorVerified'] and
          recreated['geometryColorSamples'] > 0 and recreated['coverageVerified'])
    check('color_restores_after_new_process',
          checks['workspace_color_restored_after_restart']['passed'] and
          before['graphics']['colorPreset'] == after['graphics']['colorPreset'] == 1 and
          before['graphics']['colorPacked'] == after['graphics']['colorPacked'] and
          color['app']['restored_color'] == after['graphics'] and
          after['graphics']['colorVerified'] and after['graphics']['geometryColorSamples'] > 0)
    palette = [(0.20,0.80,0.59,1), (0.90,0.38,0.16,1), (0.22,0.52,0.95,1)]
    for kind in range(4):
        for preset in range(3):
            name = f'color_primitive_{kind}_preset_{preset}'
            g = checks[name]['details']
            expected = palette[preset]
            rgb = [v*255 for v in expected[:3]]
            pixel = [g['readback'+c] for c in 'RGB']
            scale = sum(a*b for a,b in zip(rgb,pixel))/sum(v*v for v in rgb)
            check(name + '_actual_pixel_matches',
                  package['upstream_color_pixels_tested'] is True and checks[name]['passed'] and
                  color['app']['samples'][name] == g and g['colorPreset'] == preset and
                  g['meshKind'] == ['sphere','torus','disks','rims'][kind] and
                  g['colorSource'] == color['source'] and
                  all(abs(g['color'+c]-v)<.00001 for c,v in zip('RGBA',expected)) and
                  g['colorVerified'] and g['geometryVerified'] and g['coverageVerified'] and
                  g['geometryColorSamples']>0 and g['readbackA']==255 and .29<=scale<=1.02 and
                  all(abs(p-c*scale)<=3 for p,c in zip(pixel,rgb)))
    check('color_native_tests_match_source_and_target',
          color['ohos']['color_values'] == cli['upstream_color_values'] and
          cli['upstream_color_values']['passed'] is True and
          all(digest(item['path']) == item['sha256'] for item in color['sources']) and
          color['upstream_implementation_unmodified'] is True)
    color_log = local(color['host']['color_log']).read_text()
    color_assertions = [line for line in color_log.splitlines() if line.startswith('PASS ') and
                        'upstream color value adapter only;' not in line]
    check('color_host_and_target_assertions_match',
          len(color_assertions) == color['host']['color_assertions_passed'] ==
          cli['upstream_color_values']['assertions_passed'] and 'FAIL ' not in color_log and
          '100% tests passed, 0 tests failed out of 18' in local(color['host']['build_log']).read_text())
    factors = [(.3,.7,0),(.6,.4,0),(0,0,1)]
    for kind in range(4):
        for color_preset in (range(3) if kind==0 else [0]):
            samples = []
            base = palette[color_preset][:3]
            for preset in range(3):
                name=f'material_primitive_{kind}_color_{color_preset}_preset_{preset}'
                g=checks[name]['details']; samples.append(g)
                check(name + '_values_and_pixels_match',
                      package['upstream_material_pixels_tested'] is True and checks[name]['passed'] and
                      material['app']['samples'][name] == g and g['materialPreset']==preset and
                      g['colorPreset']==color_preset and g['materialSource']==material['source'] and
                      g['materialFlags']==783 and bool(g['materialCanonical']) and g['materialVerified'] and
                      g.get('materialCommandCount')==12 and g.get('materialCommandsSource')==material['commands']['source'] and
                      g['geometryVerified'] and g['coverageVerified'] and g['geometryColorSamples']>0 and
                      [.29,.59,.99][preset]<=g['rowMinScale']<=g['rowMeanScale']<=g['rowMaxScale']<=1.02 and
                      all(abs(g[n+c]-v*f)<.00001 for n,f in zip(('ambient','diffuse','emission'),factors[preset]) for c,v in zip('RGB',base)))
            classic,soft,flat = samples
            response_name=f'material_primitive_{kind}_color_{color_preset}_lighting_response'
            stable=('meshKind','meshGeneration','wireframe','meshRevision','colorPreset','width','height','yaw','pitch','distance','geometryColorSamples')
            # Independently check actual retained RGB as well as native row means.
            check(response_name,
                  checks[response_name]['passed'] and checks[response_name]['details']==samples and
                  material['app']['lighting_responses'][response_name]==samples and
                  all(all(g[k]==classic[k] for k in stable) for g in samples) and
                  abs(soft['rowMeanScale']-(classic['rowMeanScale']*4/7+3/7))<.01 and
                  soft['rowMeanScale']>classic['rowMeanScale']+.01 and
                  abs(flat['rowMeanScale']-1)<.01 and flat['rowMaxScale']-flat['rowMinScale']<.01 and
                  all(abs(soft['readback'+c]-(classic['readback'+c]*4/7+v*255*3/7))<3 for c,v in zip('RGB',base)) and
                  all(abs(flat['readback'+c]-v*255)<2 for c,v in zip('RGB',base)))
    check('material_lifecycle_evidence_matches',
          checks['material_survives_surface_recreation']['passed'] and
          checks['material_survives_surface_recreation']['details']==material['app']['surface_recreation_sample'] and
          material['app']['surface_recreation_sample']['materialPreset']==1 and
          material['app']['surface_recreation_sample']['materialVerified'] and
          checks['workspace_material_restored_after_restart']['passed'] and
          before['graphics']['materialPreset']==after['graphics']['materialPreset']==2 and
          before['graphics']['materialCanonical']==after['graphics']['materialCanonical'] and
          material['app']['restored_material']==after['graphics'] and after['graphics']['materialVerified'])
    material_log=local(material['host']['material_log']).read_text()
    material_assertions=[line for line in material_log.splitlines() if line.startswith('PASS ') and
                         'upstream material value adapter only;' not in line]
    check('material_native_source_and_assertions_match',
          cli['upstream_material_values']['passed'] and material['ohos']['material_values']==cli['upstream_material_values'] and
          len(material_assertions)==material['host']['assertions_passed']==cli['upstream_material_values']['assertions_passed'] and
          all(digest(item['path'])==item['sha256'] for item in material['sources']) and
          material['value_methods_preserved_exactly'] and material['desktop_init_preserved_exactly'])
    command_flow=material['commands']
    source_checks={item['name']:item['passed'] for item in load('reports/source-integrity.json')['checks']}
    command_target=cli['upstream_material_commands']
    check('material_command_flow_matches_original_source',
          command_flow['original_flow_preserved_exactly'] is True and
          source_checks['portable_material_init_preserves_original_command_flow'] and
          len(load('source.lock.json')['patches'])==16 and
          [p['file'] for p in load('source.lock.json')['patches'][-2:]]==['patches/0015-include-explicit-gl-texture-interface.patch','patches/0016-initialize-default-light-exponent.patch'])
    check('material_command_host_and_target_assertions_match',
          command_target['passed'] and command_flow['ohos']==command_target and
          command_target['assertions_passed']==command_flow['assertions_passed']==524360 and
          command_target['flag_state_cases_passed']==command_flow['flag_state_cases_passed']==524288 and
          'PASS upstream material command flow; 524360 assertions; 524288 flag-state cases;' in local(command_flow['host_log']).read_text())
    command_sample=checks['original_material_commands_drive_uniforms']
    check('material_commands_drive_actual_uniforms_and_pixels',
          package['upstream_material_commands_tested'] is True and command_sample['passed'] and
          command_sample['details']==command_flow['app_sample'] and
          command_sample['details']['materialCommandCount']==12 and
          command_sample['details']['materialCommandsSource']==command_flow['source'] and
          command_sample['details']['materialVerified'] is True and
          command_sample['details']['geometryColorSamples']>0)
    lit=material['lit']
    check('lit_material_native_source_and_reference_assertions_match',
          cli['upstream_lit_material']['passed'] and lit['ohos']==cli['upstream_lit_material'] and
          lit['assertions_passed']==cli['upstream_lit_material']['assertions_passed']==137 and
          'PASS lit material and independent row reference; 137 assertions;' in local(lit['host_log']).read_text())
    for kind in range(4):
        for preset in range(3):
            name=f'glossy_primitive_{kind}_color_{preset}'
            sample=checks[name]['details']
            check(name+'_reference_pixels_match',package['glossy_rendering_tested'] and checks[name]['passed'] and
                  lit['samples'][name]==sample and sample['colorPreset']==preset and
                  glossy_pixels_match(sample,palette[preset][:3],kind==0))
    gloss_before=app['glossy_state_before_restart'];gloss_after=app['glossy_state_after_restart']
    gloss_pid=checks['glossy_cold_restart_new_process']['details']
    check('glossy_new_process_restores_actual_material_pixels',
          gloss_pid['prior_pid']!=gloss_pid['new_pid'] and str(gloss_pid['prior_pid']).isdigit() and str(gloss_pid['new_pid']).isdigit() and
          gloss_before==lit['before_restart'] and gloss_after==lit['after_restart'] and
          gloss_before['workspaceState']['saveStatus']=='saved' and gloss_after['workspaceState']['restored'] is True and
          gloss_before['workspaceState']['schemaVersion']==gloss_after['workspaceState']['schemaVersion']==desktop['storage']['schema_version']==4 and
          all(gloss_before['graphics'][k]==gloss_after['graphics'][k] for k in ('meshKind','meshGeneration','wireframe','colorPreset','materialPreset','materialCanonical','yaw','pitch','distance')) and
          glossy_pixels_match(gloss_after['graphics'],palette[1][:3],True))
    cleared=checks['glossy_return_to_classic_clears_state']['details']
    check('glossy_lifecycle_and_state_reset_have_evidence',
          all(checks[n]['passed'] for n in ['glossy_surface_recreated','glossy_survives_surface_recreation','glossy_state_saved','glossy_restored_after_restart','glossy_return_to_classic_clears_state']) and
          glossy_pixels_match(checks['glossy_survives_surface_recreation']['details']['graphics'],palette[1][:3],True) and
          cleared['materialPreset']==0 and cleared['shininess']==0 and cleared['specularSamples']==[] and all(cleared['specular'+c]==0 for c in 'RGB'))
    model = desktop['model']
    model_log = local(model['evidence']).read_text()
    check('desktop_preference_model_test_snapshot_matches',
          digest(model['source']) == model['source_sha256'] and
          digest(model['test_source']) == model['test_source_sha256'] and
          f"# tests {model['cases_passed']}\n" in model_log and
          f"# pass {model['cases_passed']}\n" in model_log and '# fail 0\n' in model_log)
    check('source_build_metric_not_presented_as_functional_completion',
          coverage['functional_completion_percent'] is None and coverage['functional_completion_measured'] is False and
          coverage['selected_upstream_cpp_count'] == len(coverage['selected_sources']) and
          coverage['selected_upstream_cpp_percent'] == round(100 * len(coverage['selected_sources']) / coverage['upstream_tracked_cpp_count'], 2) and
          coverage['upstream_commit'] == baseline['upstream_commit'])
    for summary in (app, cli, baseline, mesh, submission, primitive, package, clean, desktop, coverage, color, material, triangle, matrix, polygon, commands, application, options_document):
        check('no_full_port_or_physical_pc_claim:' + str(len(report['checks'])),
              summary['full_port_complete'] is False and summary['physical_pc_verified'] is False)
    check('original_renderer_still_unverified', all(d['original_olex2_renderer_tested'] is False
          for d in (app, baseline, mesh, submission, primitive, triangle, matrix, polygon, commands, application, options_document)))
    for key, active in (('app', app), ('cli', cli)):
        evidence = active['report_directory'] + '/result.json'
        check(key + '_last_run_matches_retained_evidence', load(evidence) == active)
        references = ([baseline['native_app_report'], mesh['app_evidence'],
                       submission['app']['evidence'], primitive['app']['evidence'], desktop['app']['evidence'], color['app']['evidence'], material['app']['evidence']]
                      if key == 'app' else
                      [baseline['emulator_report'], mesh['cli_evidence'],
                       submission['ohos']['evidence'], primitive['ohos']['evidence'], color['ohos']['evidence'], material['ohos']['evidence']])
        check(key + '_summary_evidence_paths_match', all(p == evidence for p in references))
    for item in baseline['artifacts']:
        check('baseline_artifact:' + item['path'],
              local(item['path']).stat().st_size == item['bytes'] and digest(item['path']) == item['sha256'])
    for item in cli['artifacts']:
        # libc++ comes from the SDK, outside the writable project scope.
        if item['name'] != 'libc++_shared.so':
            check('transferred_cli_artifact:' + item['name'],
                  digest(cli.get('build_directory','out/ohos-arm64')+'/' + item['name']) == item['sha256'])
    tested_hashes = {item['name']: item['sha256'] for item in cli['artifacts']}
    check('clean_build_matches_source_and_target_tested_executables',
          clean['result'].startswith('passed:') and clean['source_lock_sha256'] == digest('source.lock.json') and
          len(clean['artifacts']) == 19 and all(
              item['matches_target_tested_executable'] is True and
              local(item['path']).stat().st_size == item['bytes'] and
              digest(item['path']) == item['sha256'] == tested_hashes[Path(item['path']).name]
              for item in clean['artifacts']))
    check('summary_cli_assertions_match_retained_results',
          mesh['target_mesh_checks'] == cli['upstream_mesh_adapter'] and
          submission['ohos']['shared_submission'] == cli['shared_sphere_submission'] and
          primitive['ohos']['helper'] == cli['upstream_helper_primitives'])
    check('original_triangle_source_and_wrapper_snapshots_match',
          all(digest(item['path'])==item['sha256'] for item in triangle['sources']) and
          triangle['original_flow_preserved_exactly'] is True and
          source_checks['portable_original_triangle_branch_preserved_exactly'])
    check('original_triangle_host_and_target_assertions_match',
          triangle['ohos']['commands']==cli['original_triangle_commands'] and
          triangle['ohos']['commands']['passed'] is True and
          triangle['host']['assertions_passed']==triangle['ohos']['commands']['assertions_passed']==31805 and
          triangle['host']['branch_state_cases_passed']==triangle['ohos']['commands']['branch_state_cases_passed']==31752 and
          'PASS original primitive triangle commands; 31805 assertions; 31752 branch-state cases;' in local(triangle['host']['test_log']).read_text())
    check('original_triangle_desktop_translation_unit_compile_only',
          triangle['host']['desktop_full_glprimitive_translation_unit_compiled'] is True and
          triangle['host']['desktop_runtime_tested'] is False and
          'olex2_desktop_glprimitive_compile.dir/upstream/glib/glprimitive.cpp.o' in local(triangle['host']['build_log']).read_text())
    check('original_triangle_target_scope_not_overstated',
          triangle['full_primitive_class_target_compiled'] is True and
          triangle['gpu_vertex_color_tracking_implemented'] is False and triangle['selection_engine_implemented'] is False)
    check('original_triangle_summary_matches_active_evidence',
          triangle['app']['evidence']==app['report_directory']+'/result.json' and
          triangle['ohos']['evidence']==cli['report_directory']+'/result.json' and
          package['original_triangle_branch_tested'] is True)
    for name, sample in triangle['app']['samples'].items():
        raw=checks[name]['details']
        actual=raw.get('graphics',raw)
        check('original_triangle_upload:'+name,
              checks[name]['passed'] and sample==actual and original_triangle_sample_matches(actual))
    recreated=triangle['app']['surface_recreation_sample']
    check('original_triangle_surface_and_cold_restart_restore_upload',
          recreated==checks['mesh_selection_survives_surface_recreation']['details'] and
          original_triangle_sample_matches(recreated.get('graphics',recreated)) and
          triangle['app']['restored_sample']==app['glossy_state_after_restart']['graphics'] and
          original_triangle_sample_matches(triangle['app']['restored_sample']))
    check('original_matrix_exports_and_source_snapshots_match',
          all(digest(item['path'])==item['sha256'] for item in matrix['sources']) and
          matrix['upstream_commit']==load('source.lock.json')['commit'] and matrix['patch']=='patches/0008-add-owned-original-basis-matrix-exports.patch' and
          matrix['original_export_assignments_preserved_exactly'] is True and matrix['original_basis_implementation_unchanged'] is True and
          all(source_checks.get(name) is True for name in (
            'owned_basis_export_preserves_original_assignments:CopyMData',
            'owned_basis_export_preserves_original_assignments:CopyMDataT',
            'legacy_basis_getter_retains_static_buffer:GetMData',
            'legacy_basis_getter_retains_static_buffer:GetMDataT',
            'original_basis_rotation_implementation_unmodified')))
    check('original_matrix_host_and_target_assertions_match',
          matrix['host']['ctest_cases_passed']==18 and matrix['ohos']['matrices']==cli['original_basis_matrices'] and
          matrix['ohos']['matrices']['passed'] is True and
          matrix['host']['assertions_passed']==matrix['ohos']['matrices']['assertions_passed']==353 and
          matrix['host']['independent_camera_poses_passed']==matrix['ohos']['matrices']['independent_camera_poses_passed']==315 and
          '353 assertions; 315 independent camera poses;' in local(matrix['host']['test_log']).read_text() and
          '100% tests passed, 0 tests failed out of 18' in local(matrix['host']['build_log']).read_text())
    check('original_matrix_summary_matches_active_evidence',
          matrix['app']['evidence']==app['report_directory']+'/result.json' and
          matrix['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_basis_matrix_tested'] is True)
    check('original_matrix_scope_not_overstated',
          all(matrix[key] is False for key in ('full_gl_matrix_state_implemented','general_stack_sequences_gpu_tested','scale_shear_gpu_tested','legacy_basis_globally_thread_safe')) and
          matrix['ohos']['matrices']['full_gl_matrix_state_implemented'] is False and matrix['ohos']['matrices']['original_renderer_tested'] is False and
          matrix['app']['model_view_evidence']=='logical CPU state only' and
          matrix['app']['mvp_and_normal_evidence']=='actual glGetUniformfv readback independently checked against analytic camera coefficients')
    check('original_matrix_sample_inventory_matches_scope',set(matrix['app']['samples'])=={'original_basis_drives_actual_matrix_uniforms','injected_keyboard_rotate','injected_keyboard_zoom','injected_keyboard_reset','injected_mouse_wheel_zoom','native_window_resize','native_window_restore'})
    for name,sample in matrix['app']['samples'].items():
        raw=checks[name]['details'];actual=raw.get('graphics',raw)
        check('original_matrix_sample:'+name,checks[name]['passed'] and actual==sample and original_matrices_match(actual))
    check('original_matrix_surface_and_process_recovery',
          matrix['app']['surface_recreation_sample']==checks['mesh_selection_survives_surface_recreation']['details'].get('graphics',checks['mesh_selection_survives_surface_recreation']['details']) and
          original_matrices_match(matrix['app']['surface_recreation_sample']) and
          matrix['app']['restored_sample']==app['glossy_state_after_restart']['graphics'] and original_matrices_match(matrix['app']['restored_sample']))
    def graphic_snapshots(value,path=()):
        if isinstance(value,dict):
            if {'meshKind','yaw','height','submittedVertices'}.issubset(value):yield path,value
            else:
                for key,nested in value.items():yield from graphic_snapshots(nested,path+(key,))
        elif isinstance(value,list):
            for index,nested in enumerate(value):yield from graphic_snapshots(nested,path+(index,))
    retained_graphics=list(graphic_snapshots(app))
    graphics=[g for path,g in retained_graphics if g.get('surfaceReady') is True and g.get('initialized') is True]
    inactive_pairs=[(path,g) for path,g in retained_graphics if g.get('surfaceReady') is False and g.get('initialized') is False]
    inactive_graphics=[g for path,g in inactive_pairs]
    check('retained_graphic_lifecycle_partition_is_explicit',len(graphics)+len(inactive_graphics)==len(retained_graphics) and len(inactive_graphics)>=8)
    check('inactive_graphics_only_document_page',all((isinstance(path[0],str) and path[0].startswith('options_document_')) or
          (path[0]=='checks' and type(path[1]) is int and app['checks'][path[1]]['name'].startswith('options_document_')) for path,g in inactive_pairs))
    check('inactive_surface_graphics_are_explicitly_unverified',all(all(g.get(k) is False for k in ('basisMatrixVerified','matrixUniformVerified','polygonProbeVerified','coverageVerified','meshValidated','submissionValidated','materialVerified','colorVerified','specularReferenceVerified')) for g in inactive_graphics))
    check('all_retained_active_graphic_states_have_independent_matrix_validation',len(graphics)>=120 and all(original_matrices_match(g) for g in graphics))
    check('original_polygon_source_and_snapshots_match',
          polygon['original_flow_preserved_exactly'] is True and source_checks.get('portable_original_polygon_branch_preserved_exactly') is True and
          polygon['upstream_commit']==load('source.lock.json')['commit'] and polygon['patch']=='patches/0009-extract-original-polygon-submission.patch' and
          all(digest(item['path'])==item['sha256'] for item in polygon['sources']))
    check('original_polygon_host_and_target_assertions_match',
          polygon['host']['ctest_cases_passed']==18 and polygon['ohos']['commands']==cli['original_polygon_commands'] and polygon['ohos']['commands']['passed'] is True and
          polygon['host']['assertions_passed']==polygon['ohos']['commands']['assertions_passed']==2052 and
          polygon['host']['branch_state_cases_passed']==polygon['ohos']['commands']['branch_state_cases_passed']==1980 and
          polygon['host']['independent_topology_poses_passed']==polygon['ohos']['commands']['independent_topology_poses_passed']==48 and
          '2052 assertions; 1980 branch-state cases; 48 independent topology poses;' in local(polygon['host']['test_log']).read_text())
    check('original_polygon_desktop_compile_only',polygon['host']['desktop_full_glprimitive_translation_unit_compiled'] is True and polygon['host']['desktop_runtime_tested'] is False and
          'olex2_desktop_glprimitive_compile.dir/upstream/glib/glprimitive.cpp.o' in local(polygon['host']['build_log']).read_text())
    check('original_polygon_summary_matches_active_evidence',polygon['app']['evidence']==app['report_directory']+'/result.json' and polygon['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_polygon_branch_and_gpu_tessellation_tested'] is True)
    check('original_polygon_scope_not_overstated',all(polygon[key] is False for key in ('full_glu_implemented','holes_supported','nonplanar_polygons_supported','gpu_vertex_color_tracking_implemented','selection_engine_implemented','legacy_gl_polygon_concavity_equivalence_claimed')) and
          all(polygon['ohos']['commands'][key] is False for key in ('full_glu_implemented','holes_supported','original_renderer_tested')) and polygon['app']['gpu_fixture_count']==1 and
          polygon['app']['gpu_state_restore_evidence']=='main-context GL state restored and queried using integer boolean-state queries; original application EGL context/display/draw/read surfaces unchanged and queried; no context switch')
    polygon_names={'original_polygon_branch_and_concave_gpu_tessellation'}|{f'primitive_2_detail_{d}_{mode}' for d in (0,3,5) for mode in ('solid','wire')}|{f'color_primitive_2_preset_{c}' for c in range(3)}|{f'glossy_primitive_2_color_{c}' for c in range(3)}
    check('original_polygon_sample_inventory_matches_scope',set(polygon['app']['samples'])==polygon_names)
    for name,sample in polygon['app']['samples'].items():
        raw=checks[name]['details'];actual=raw.get('graphics',raw)
        check('original_polygon_sample:'+name,checks[name]['passed'] and actual==sample and actual['meshKind']=='disks' and original_polygon_upload_matches(actual) and polygon_probe_matches(actual))
    poly_before,poly_after=app['polygon_state_before_restart'],app['polygon_state_after_restart'];poly_pid=checks['polygon_cold_restart_new_process']['details']
    check('original_polygon_surface_and_process_recovery',
          all(checks[k]['passed'] for k in ('polygon_surface_recreated','polygon_survives_surface_recreation','polygon_state_saved','polygon_cold_restart_new_process','polygon_restored_after_restart')) and
          polygon['app']['surface_recreation_sample']==checks['polygon_survives_surface_recreation']['details']['graphics'] and
          original_polygon_upload_matches(polygon['app']['surface_recreation_sample']) and polygon_probe_matches(polygon['app']['surface_recreation_sample']) and
          polygon['app']['before_restart']==poly_before and polygon['app']['after_restart']==poly_after and polygon['app']['restart_processes']==poly_pid and
          poly_pid['prior_pid']!=poly_pid['new_pid'] and str(poly_pid['prior_pid']).isdigit() and str(poly_pid['new_pid']).isdigit() and
          poly_before['workspaceState']['saveStatus']=='saved' and poly_after['workspaceState']['restored'] is True and poly_after['graphics']['meshKind']=='disks' and
          all(poly_before['graphics'][k]==poly_after['graphics'][k] for k in ('meshKind','meshGeneration','wireframe','colorPreset','materialPreset','materialCanonical','yaw','pitch','distance')) and
          original_polygon_upload_matches(poly_after['graphics']) and polygon_probe_matches(poly_after['graphics']))
    check('all_retained_active_graphic_states_have_original_polygon_and_gpu_probe_validation',len(graphics)>=125 and all(original_polygon_upload_matches(g) and polygon_probe_matches(g) for g in graphics))
    check('original_desktop_command_sources_match',all(digest(item['path'])==item['sha256'] for item in commands['sources']) and
          commands['upstream_commit']==load('source.lock.json')['commit'] and commands['upstream_interpreter_modified'] is False)
    check('original_desktop_command_host_and_target_match',commands['host']['ctest_cases_passed']==18 and
          commands['ohos']['commands']==cli['original_desktop_commands'] and commands['ohos']['commands']['passed'] is True and
          all(commands['host'][key]==commands['ohos']['commands'][key]==count for key,count in
              (('assertions_passed',1196),('independent_view_combinations_passed',576),('serialized_concurrent_calls_passed',64))) and
          '1196 assertions; 576 independent view combinations; 64 serialized concurrent calls;' in local(commands['host']['test_log']).read_text() and
          '100% tests passed, 0 tests failed out of 18' in local(commands['host']['build_log']).read_text())
    check('original_desktop_command_summary_matches_active_evidence',commands['app']['evidence']==app['report_directory']+'/result.json' and
          commands['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_desktop_command_interpreter_tested'] is True and baseline['original_desktop_commands_tested'] is True)
    check('original_desktop_command_scope_not_overstated',all(commands[key] is False for key in
          ('full_original_application_entry_integrated','full_original_ui_integrated','complete_original_command_library_integrated','python_integrated','callbacks_supported',
           'global_processor_thread_safety_claimed','gpu_device_loss_rollback_claimed','command_history_persisted','command_history_replayed')) and
          all(commands['ohos']['commands'][key] is False for key in ('complete_application_commands_registered','target_python_integrated','global_processor_thread_safety_claimed')))
    check('original_desktop_command_sample_inventory_matches_scope',set(commands['app']['samples'])==set(SAMPLES))
    for name,sample in commands['app']['samples'].items():
        check('original_desktop_command_sample:'+name,checks[name]['passed'] is True and checks[name]['details']==sample and command_sample_matches(name,sample))
    observations=app['desktop_command_input_observations']
    check('original_desktop_command_literal_input_observations_match',commands['app']['input_observations']==observations and set(observations)==set(SAMPLES) and
          all(o['expected']==o['actual_committed_text']==SAMPLES[name]['input'] and type(o['temporary_typing_mode_toggle']) is bool and
              o['typing_mode_restore_event_sent'] is o['temporary_typing_mode_toggle'] for name,o in observations.items()))
    check('original_desktop_command_surface_and_process_recovery',command_recovery_matches(app) and
          commands['app']['surface_recreation_sample']==checks['desktop_command_survives_surface_recreation']['details'] and
          commands['app']['before_restart']==app['desktop_command_state_before_restart'] and commands['app']['after_restart']==app['desktop_command_state_after_restart'] and
          commands['app']['restart_processes']==checks['desktop_command_cold_restart_new_process']['details'])
    check('original_application_runtime_sources_match',all(digest(item['path'])==item['sha256'] for item in application['sources']) and
          application['upstream_commit']==load('source.lock.json')['commit'] and application['original_basic_app_implementation_unchanged'] is True and
          application['original_default_save_options_body_unchanged'] is True and source_checks['original_basic_app_save_hook_only'] and source_checks['original_basic_app_implementation_unchanged'] and source_checks['original_hashed_container_counter_fix_only'])
    check('original_application_runtime_host_and_target_match',application['host']['ctest_cases_passed']==18 and application['ohos']['runtime']==cli['original_application_runtime'] and application['ohos']['runtime']['passed'] is True and
          all(application['host'][key]==application['ohos']['runtime'][key]==count for key,count in (('assertions_passed',86),('original_option_round_trips_passed',32),('mixed_serialized_interpreter_calls_passed',64))) and
          '86 assertions; 32 original option round trips; 64 mixed serialized interpreter calls;' in local(application['host']['test_log']).read_text() and
          '100% tests passed, 0 tests failed out of 18' in local(application['host']['build_log']).read_text())
    check('original_application_runtime_summary_matches_active_evidence',application['app']['evidence']==app['report_directory']+'/result.json' and application['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_application_runtime_and_options_tested'] is True and baseline['original_application_runtime_tested'] is True)
    check('original_application_runtime_scope_not_overstated',all(application[key] is False for key in ('full_original_application_entry_integrated','full_original_ui_integrated','all_original_application_functions_runtime_tested','complete_original_resource_tree_integrated','bundled_resources_immutable_claimed','process_environment_modified','working_directory_modified','power_loss_durability_verified','global_processor_thread_safety_claimed','application_function_history_persisted')) and
          all(application['ohos']['runtime'][key] is False for key in ('full_application_entry_integrated','all_original_application_functions_runtime_tested','process_environment_modified','global_processor_thread_safety_claimed','power_loss_durability_verified')) and
          application['resource_scope']=='Exact original OLEX2 licence only, copied atomically into a writable private base directory; not the complete original resource tree')
    check('original_application_runtime_licence_fixture_matches',application['ohos']['licence_fixture']==cli['application_runtime_fixture'] and digest('upstream/LICENCE.txt')==cli['application_runtime_fixture']['sha256'])
    check('original_application_function_inventory_matches_scope',set(application['app']['samples'])==set(APPLICATION_SAMPLES) and set(application['app']['before_samples'])==set(APPLICATION_SAMPLES))
    for name,sample in application['app']['samples'].items():
        check('original_application_function_sample:'+name,checks[name]['passed'] is True and checks[name]['details']==sample and app['application_function_before_samples'][name]==application['app']['before_samples'][name] and function_sample_matches(name,sample,application['app']['before_samples'][name]))
    check('original_application_runtime_baseline_and_visible_setting_match',application['app']['baseline']==checks['application_runtime_baseline']['details'] and runtime_matches(application['app']['baseline']['applicationRuntime']) and
          all(application['app']['visible_setting_checks'][name]==checks[name]['details'] and checks[name]['passed'] is True and checks[name]['details']['visible_hint_nodes']==count for name,count in (('application_runtime_hints_hidden',0),('application_runtime_hints_visible',1))))
    check('original_application_runtime_literal_input_observations_match',application['app']['input_observations']==app['application_function_input_observations'] and set(app['application_function_input_observations'])==set(APPLICATION_SAMPLES) and
          all(o['expected']==o['actual_committed_text']==APPLICATION_SAMPLES[name]['input'] and type(o['temporary_typing_mode_toggle']) is bool and o['typing_mode_restore_event_sent'] is o['temporary_typing_mode_toggle'] for name,o in app['application_function_input_observations'].items()))
    check('original_application_runtime_cold_process_recovery_matches',app_runtime_evidence_matches(app) and application['app']['before_restart']==app['application_runtime_before_restart'] and application['app']['after_restart']==app['application_runtime_after_restart'] and application['app']['restart_processes']==checks['application_runtime_cold_restart_new_process']['details'])
    from options_document_validation import NAMES as DOCUMENT_NAMES, SAMPLES as DOCUMENT_SAMPLES, app_options_document_evidence_matches, transfer_sample_matches, recovery_matches as document_recovery_matches
    check('options_document_sources_and_utf8_patch_match',all(digest(item['path'])==item['sha256'] for item in options_document['sources']) and
          options_document['utf8_reader_patch']=='patches/0012-add-explicit-utf8-settings-reader.patch' and source_checks.get('original_settings_explicit_utf8_reader_only') is True and source_checks.get('original_settings_save_body_unchanged') is True)
    check('options_document_host_and_target_match',options_document['host']['ctest_cases_passed']==18 and options_document['host']['assertions_passed']==23 and
          options_document['ohos']['runtime']==cli['original_options_document_transfer'] and cli['original_options_document_transfer']['passed'] is True and cli['original_options_document_transfer']['assertions_passed']==23 and
          '23 assertions;' in local(options_document['host']['test_log']).read_text() and '100% tests passed, 0 tests failed out of 18' in local(options_document['host']['build_log']).read_text())
    check('options_document_isolated_model_tests_match',options_document['model']['cases_passed']==26 and '# tests 26\n' in local(options_document['model']['evidence']).read_text() and '# fail 0\n' in local(options_document['model']['evidence']).read_text())
    check('options_document_summary_matches_active_evidence',options_document['app']['evidence']==app['report_directory']+'/result.json' and options_document['ohos']['evidence']==cli['report_directory']+'/result.json' and
          all(options_document['app'][key]==app['options_document_'+key] for key in ('before_samples','exported','imported','before_import','before_restart','after_restart','filename_observation')) and options_document['app']['function_input_observations']==app['options_document_function_input_observations'])
    check('options_document_inventory_matches_scope',set(options_document['app']['samples'])==set(DOCUMENT_NAMES))
    for name,(operation,sequence) in DOCUMENT_SAMPLES.items():
        actual=checks[name]['details']
        check('options_document_sample:'+name,checks[name]['passed'] is True and options_document['app']['samples'][name]==actual and transfer_sample_matches(name,actual,app['options_document_before_samples'][operation+'-'+str(sequence)],app['options_document_exported']))
    check('options_document_system_picker_and_visible_settings_match',package['options_document_system_picker_and_original_merge_tested'] is True and app_options_document_evidence_matches(app) and all(options_document['app']['samples'][name]==checks[name]['details'] for name in DOCUMENT_NAMES))
    check('options_document_distinct_process_restore_without_replay',document_recovery_matches(app))
    check('options_document_scope_not_overstated',all(options_document[key] is False for key in ('full_port_complete','physical_pc_verified','complete_application_document_open_save_supported','restorable_uri_history_persisted','uri_history_replayed','external_destination_atomicity_claimed','power_loss_durability_verified','legacy_settings_reader_behavior_changed','original_settings_save_body_changed')) and options_document['maximum_document_bytes']==1048576 and options_document['diagnostic_record_contains_last_selected_uri'] is True and options_document['ohos']['runtime']['system_picker_tested'] is False)
    try:
        provenance_path = local('out/app/entry-default-unsigned.provenance.json')
        proof = verify_build_provenance(local('out/app/entry-default-unsigned.hap'), provenance_path, ROOT)
        check('build_provenance_matches_current_hap_and_sources', proof['passed'] and proof['workspace_matches'])
        retained = app['build_provenance']
        archived_proof = verify_build_provenance(local(retained['retained_hap']), local(retained['retained_provenance']))
        current_proof_without_workspace = dict(proof, workspace_matches=False)
        check('runtime_retains_exact_tested_build_artifacts',
              retained['verification'] == proof and archived_proof == current_proof_without_workspace and
              digest(retained['retained_inputs']) == proof['input_manifest_sha256'] and
              archived_proof['hap_sha256'] == app['hap']['sha256'])
        check('build_provenance_summary_matches_direct_check',
              package.get('build_provenance_verified') is True and package.get('build_provenance') == proof)
        native_sources = {item['path'].removeprefix('upstream/')
                          for item in load('out/app/entry-default-unsigned.provenance.json')['native']['compiled_sources']
                          if item['path'].startswith('upstream/')}
        check('packaged_compiled_upstream_matches_build_metric', native_sources == set(coverage['selected_sources']))
        check('original_primitive_class_is_source_compiled', 'glib/glprimitive.cpp' in native_sources and all(load('reports/'+name+'-status.json')['full_primitive_class_target_compiled'] is True for name in ('original-triangle','original-polygon','original-line')))
    except Exception as error:
        report['build_provenance_error'] = f'{type(error).__name__}: {error}'
        check('build_provenance_matches_current_hap_and_sources', False)
    check('source_integrity_passed', load('reports/source-integrity.json')['passed'])
    check('helper_loop_equivalence_passed', load('reports/upstream-helper-extraction.json')['passed'])
    lines=load('reports/original-line-status.json')
    check('original_line_sources_and_exact_upstream_flow_match',lines['original_flow_preserved_exactly'] is True and lines['patch']=='patches/0013-extract-original-line-submission.patch' and
          source_checks.get('portable_original_line_branch_preserved_exactly') is True and all(source_checks.get('portable_original_line_path_branch_preserved_exactly:'+branch) is True and source_checks.get('desktop_original_line_path_wrapper_preserved:'+branch) is True for branch in ('sgloLineStrip','sgloLineLoop')) and all(digest(item['path'])==item['sha256'] for item in lines['sources']))
    check('original_line_host_and_target_assertions_match',lines['host']['ctest_cases_passed']==18 and
          lines['host']['assertions_passed']==cli['original_line_commands']['assertions_passed']==104199 and
          lines['host']['branch_state_cases_passed']==cli['original_line_commands']['branch_state_cases_passed']==640 and
          lines['host']['source_edge_meshes_passed']==cli['original_line_commands']['source_edge_meshes_passed']==24 and
          lines['ohos']['commands']==cli['original_line_commands'] and cli['original_line_commands']['passed'] is True and
          '100% tests passed, 0 tests failed out of 18' in local(lines['host']['build_log']).read_text())
    check('original_line_24_cpu_index_contracts_independently_validated',line_command_log_matches(local(lines['host']['test_log']).read_text()) and
          line_command_log_matches(local(lines['ohos']['test_log']).read_text()) and lines['ohos']['test_log']==cli['report_directory']+'/original-lines-run.log')
    check('original_line_desktop_compile_only',lines['host']['desktop_full_glprimitive_translation_unit_compiled'] is True and
          lines['host']['desktop_runtime_tested'] is False and 'olex2_desktop_glprimitive_compile.dir/upstream/glib/glprimitive.cpp.o' in local(lines['host']['build_log']).read_text())
    check('all_retained_active_graphic_states_have_original_line_ebo_validation',len(graphics)>=125 and all(original_line_upload_matches(g) for g in graphics))
    check('inactive_surfaces_have_no_current_line_buffer_proof',all(inactive_line_proof_cleared(g) for g in inactive_graphics))
    check('original_line_app_evidence_matches_current_package',lines['version']==version and lines['app']['checks_passed']==len(checks) and
          lines['app']['evidence']==app['report_directory']+'/result.json' and lines['ohos']['evidence']==cli['report_directory']+'/result.json' and
          lines['app']['hap_sha256']==app['hap']['sha256'] and package['original_line_branch_and_actual_index_buffer_tested'] is True and app_line_evidence_matches(app))
    check('original_line_saved_samples_match_raw_evidence',len(lines['app']['samples'])==11 and all(
          sample==checks[name]['details'].get('graphics',checks[name]['details']) and original_line_upload_matches(sample) for name,sample in lines['app']['samples'].items()) and
          lines['app']['surface_recreation_sample']==checks['mesh_selection_survives_surface_recreation']['details'] and lines['app']['restored_sample']==app['state_after_restart'])
    check('original_line_scope_not_overstated',all(lines.get(k) is False for k in ('full_port_complete','physical_pc_verified','gpu_vertex_color_tracking_implemented',
          'gpu_wide_line_state_implemented','fixed_function_normal_state_equivalent')) and cli['original_line_commands']['gpu_wide_lines_tested'] is False)
    check('original_core_sources_match',core['upstream_commit']==load('source.lock.json')['commit'] and
          all(digest(item['path'])==item['sha256'] for item in core['sources']))
    actual_core=[{key:item[key] for key in ('path','bytes','sha256')} for item in load('out/app/entry-default-unsigned.provenance.json')['native']['compiled_sources']
                 if item['path'].startswith('upstream/xlib/') or item['path'] in ('upstream/repository/fsext.cpp','upstream/repository/hkl_util.cpp','upstream/repository/refinement_listener.cpp')]
    check('original_core_production_compilation_matches',core['compiled_core_source_count']==len(core['compiled_core_sources'])==len(actual_core)==275 and core['compiled_core_sources']==actual_core and
          all(item['path'].removeprefix('upstream/') in coverage['selected_sources'] and digest(item['path'])==item['sha256'] for item in actual_core))
    check('original_core_real_constructor_and_registry_wiring_matches','class PlatformApp final:public TXApp' in local('native/application_runtime.cpp').read_text() and
          ':TXApp(Text(base+"/olex2-harmony")),config_(config)' in local('native/application_runtime.cpp').read_text() and
          'processor.GetLibrary().AttachLibrary(impl_->app->ExportLibrary("app"))' in local('native/application_runtime.cpp').read_text())
    cr=cli['original_application_runtime']
    check('original_core_host_and_target_lifecycle_match',core['host']['ctest_cases_passed']==18 and core['host']['runtime_assertions_passed']==cr['assertions_passed']==86 and
          core['host']['repeated_lifecycle_cases_passed']==cr['repeated_core_lifecycle_cases_passed']==8 and
          core['host']['failed_private_settings_startup_recovery_tested'] is cr['failed_private_settings_startup_recovery_tested'] is True and
          cr['original_core_empty_startup_tested'] is True and (cr['original_core_macro_registry_count'],cr['original_core_function_registry_count'],cr['original_core_file_count'])==(135,38,1) and
          'real TXApp empty startup/135 core macros/38 core functions (registered only)' in local(core['host']['test_log']).read_text() and
          '8 repeated lifecycles and failed-settings recovery' in local(core['host']['test_log']).read_text())
    check('original_core_actual_app_snapshot_inventory_matches',app_core_evidence_matches(app) and core['app']['baseline']==checks['original_core_runtime_empty_startup']['details'] and
          core['app']['runtime_snapshot_count']==len(list(runtime_snapshots(app))) and core['app']['runtime_snapshot_count']>0)
    check('original_core_current_package_and_evidence_match',core['version']==identity['versionName'] and core['app']['evidence']==app['report_directory']+'/result.json' and
          core['app']['hap_sha256']==app['hap']['sha256']==package['hap_sha256'] and core['app']['checks_passed']==194 and
          core['ohos']['evidence']==cli['report_directory']+'/result.json' and core['ohos']['runtime']==cr and
          package['original_core_empty_startup_in_actual_app_tested'] is True and baseline['original_core_empty_startup_tested'] is True)
    check('original_core_scope_not_overstated',core['genuine_core_empty_startup_integrated'] is True and core['functional_completion_percent'] is None and
          core['core_registry_commands_exposed_to_ui'] is True and core['core_registry_commands_runtime_tested'] is True and core['exposed_core_metadata_macros']==['core.LstMac','core.LstFun'] and all(core[k] is False for k in ('all_core_registry_commands_exposed_to_ui','all_core_registry_commands_runtime_tested','original_scientific_command_behavior_tested','original_scientific_documents_loaded','full_original_application_entry_integrated','full_original_ui_integrated','complete_original_resource_tree_integrated','full_port_complete','physical_pc_verified','physical_keyboard_mouse_verified','release_signing_verified')) and cr['original_core_commands_executed'] is False)
    from line_path_validation import path_probe_matches,inactive_path_proof_cleared,app_path_evidence_matches
    paths=load('reports/original-line-path-status.json')
    check('original_line_path_sources_and_flow_match',paths['original_flow_preserved_exactly'] is True and paths['patch']=='patches/0014-extract-original-line-path-submission.patch' and all(digest(item['path'])==item['sha256'] for item in paths['sources']) and all(source_checks.get('portable_original_line_path_branch_preserved_exactly:'+branch) is True and source_checks.get('desktop_original_line_path_wrapper_preserved:'+branch) is True for branch in ('sgloLineStrip','sgloLineLoop')))
    check('original_line_path_host_and_target_match',paths['host']['ctest_cases_passed']==18 and paths['ohos']['commands']==cli['original_line_path_commands'] and paths['ohos']['commands']['passed'] is True and all(paths['host'][k]==paths['ohos']['commands'][k]==v for k,v in (('assertions_passed',181512),('raw_branch_state_cases_passed',2688),('bounded_branch_state_cases_passed',1408),('independent_edge_topologies_passed',2))) and '181512 assertions; 2688 raw branch-state cases; 1408 bounded branch-state cases;' in local(paths['host']['test_log']).read_text() and '100% tests passed, 0 tests failed out of 18' in local(paths['host']['build_log']).read_text())
    check('original_line_path_desktop_compile_only',paths['host']['desktop_full_glprimitive_compiled'] is True and paths['host']['desktop_runtime_tested'] is False and 'olex2_desktop_glprimitive_compile.dir/upstream/glib/glprimitive.cpp.o' in local(paths['host']['build_log']).read_text())
    check('all_active_line_path_gpu_probes_match_independent_ebo_pixels',len(graphics)>=125 and all(path_probe_matches(g) for g in graphics))
    check('inactive_line_path_gpu_proof_is_cleared',all(inactive_path_proof_cleared(g) for g in inactive_graphics))
    check('original_line_path_saved_sample_inventory_matches',paths['app']['sample']==checks['original_line_path_gpu_probe']['details'] and path_probe_matches(paths['app']['sample']) and paths['app']['active_graphic_snapshots']==len(graphics) and paths['app']['inactive_document_snapshots']==len(inactive_graphics) and (paths['app']['actual_gpu_cases'],paths['app']['readback_regions_per_case'],paths['app']['region_pixel_count'])==(2,8,9))
    check('original_line_path_current_package_evidence_match',app_path_evidence_matches(app) and paths['version']==version and paths['app']['checks_passed']==194 and paths['app']['hap_sha256']==app['hap']['sha256']==package['hap_sha256'] and paths['app']['evidence']==app['report_directory']+'/result.json' and paths['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_line_paths_gpu_probe_tested'] is True and baseline['original_line_paths_tested'] is True)
    check('original_line_path_scope_not_overstated',paths['functional_completion_percent'] is None and all(paths[k] is False for k in ('primary_mesh_line_source_changed','gpu_vertex_color_tracking_implemented','gpu_wide_line_state_implemented','gpu_selecting_implemented','fixed_function_normal_state_equivalent','full_renderer_implemented','full_port_complete','physical_pc_verified')) and lines['line_strip_or_loop_shared'] is True and all(paths['ohos']['commands'][k] is False for k in ('gpu_execution_tested','gpu_vertex_color_tracking_tested','gpu_wide_lines_tested','full_renderer_tested')))
    from original_log_validation import parse_catalog_log,app_original_log_evidence_matches,SAMPLE_NAMES as LOG_NAMES,NATIVE_ASSERTIONS
    logs=load('reports/original-log-status.json')
    host_catalog=parse_catalog_log(local(logs['host']['test_log']).read_text())
    target_catalog=parse_catalog_log(local(logs['ohos']['test_log']).read_text())
    check('original_log_source_snapshot_matches',all(digest(item['path'])==item['sha256'] for item in logs['sources']))
    check('original_log_host_and_target_notifications_match',logs['host']['ctest_cases_passed']==18 and logs['host']['assertions_passed']==NATIVE_ASSERTIONS and logs['ohos']['bridge']==cli['original_log_bridge'] and logs['ohos']['bridge']['passed'] is True and logs['ohos']['bridge']['assertions_passed']==NATIVE_ASSERTIONS and host_catalog==target_catalog==logs['host']['catalog_reference'] and logs['ohos']['test_log']==cli['report_directory']+'/original-log-run.log' and '100% tests passed, 0 tests failed out of 18' in local(logs['host']['build_log']).read_text())
    check('original_log_actual_ui_text_and_clear_restart_match',app_original_log_evidence_matches(app,host_catalog))
    check('original_log_saved_ui_samples_match',set(logs['app']['samples'])==set(LOG_NAMES) and all(logs['app']['samples'][name]==checks[name]['details'] for name in LOG_NAMES))
    compiled_paths={item['path'] for item in load('out/app/entry-default-unsigned.provenance.json')['native']['compiled_sources']}
    check('original_log_production_callback_and_catalog_wiring_matches','native/original_log.cpp' in compiled_paths and 'log_.OnPost.Add(this)' in local('native/original_log.cpp').read_text() and 'log_.OnPost.Remove(this)' in local('native/original_log.cpp').read_text() and 'original->Replicate()' in local('native/application_runtime.cpp').read_text() and 'processor.processMacroEx(Text(input)' in local('native/application_runtime.cpp').read_text() and all('this.showCoreCatalog('+repr(value)+')' in local('app/entry/src/main/ets/pages/Index.ets').read_text() for value in ('core.LstMac','core.LstFun')))
    check('original_log_budgets_and_call_boundary_match',logs['limits_config']==dict(max_entries=256,max_retained_utf8_bytes=65536,max_entry_utf8_bytes=4096) and logs['host']['original_severity_cases_passed']==logs['ohos']['bridge']['original_severity_cases_passed']==5 and logs['host']['real_core_catalog_macros_tested']==logs['ohos']['bridge']['real_core_catalog_macros_tested']==2 and logs['host']['locally_serialized_caller_posts_passed']==logs['ohos']['bridge']['locally_serialized_caller_posts_passed']==400)
    check('original_log_current_package_evidence_match',logs['version']==version and logs['app']['checks_passed']==194 and logs['app']['hap_sha256']==app['hap']['sha256']==package['hap_sha256'] and logs['app']['evidence']==app['report_directory']+'/result.json' and logs['ohos']['evidence']==cli['report_directory']+'/result.json' and package['original_log_and_core_catalog_ui_tested'] is True and baseline['original_log_bridge_tested'] is True)
    check('original_log_scope_not_overstated',logs['exposed_original_core_metadata_macros']==['core.LstMac','core.LstFun'] and logs['original_metadata_macros_runtime_tested'] is True and logs['functional_completion_percent'] is None and all(logs[key] is False for key in ('full_core_registry_exposed','scientific_command_behavior_tested','global_legacy_thread_safety_claimed','log_severity_tracking_implemented','direct_log_stream_writes_captured','durable_log_storage_implemented','automatic_log_restore_or_replay','oom_failure_injection_tested','full_port_complete','physical_pc_verified','release_signing_verified')) and all(logs['ohos']['bridge'][key] is False for key in ('full_core_registry_exposed','scientific_command_behavior_tested','global_legacy_thread_safety_claimed','durable_log_storage_tested','oom_injection_tested')))
    from desktop_shell_validation import app_shell_evidence_matches,identity_matches,identity_snapshots,SAMPLES as SHELL_NAMES
    shell=load('reports/desktop-shell-status.json')
    expected_identity=load('app/AppScope/app.json5')['app']
    shell_index=local('app/entry/src/main/ets/pages/Index.ets').read_text()
    shell_ability=local('app/entry/src/main/ets/entryability/EntryAbility.ets').read_text()
    check('desktop_shell_source_snapshots_match',bool(shell['sources']) and all(digest(s['path'])==s['sha256'] for s in shell['sources']))
    model_log=local(shell['model']['evidence']).read_text()
    check('desktop_shell_submit_model_contract_matches',shell['model']['cases_passed']==19 and '# tests 19\n' in model_log and '# fail 0\n' in model_log)
    identities=list(identity_snapshots(app))
    check('desktop_shell_all_installed_identities_match',bool(identities) and len(identities)==shell['app']['identity_snapshots_checked'] and all(identity_matches(i,expected_identity) for i in identities))
    check('desktop_shell_actual_enter_identity_layout_pause_match',app_shell_evidence_matches(app,expected_identity))
    check('desktop_shell_saved_samples_match',all(shell['app']['samples'][name]==checks[name]['details'] for name in SHELL_NAMES))
    check('desktop_shell_production_wiring_matches',
          'bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT)' in shell_ability and
          'info.versionName' in shell_ability and 'info.versionCode' in shell_ability and
          'usesApplicationInterpreter(this.commandText)' in shell_index and '.onSubmit(() => { this.submitCommand(); })' in shell_index and
          "this.versionName.length > 0 ? `开发验证版  /  ${this.versionName}`" in shell_index and "开发验证版  /  0.14.0" not in shell_index and
          "Button('执行视图').id('olex2-command-run').layoutWeight(1).padding({ left: 4, right: 4 })" in shell_index and
          "Button('命令帮助').id('olex2-command-help').width('100%')" in shell_index)
    check('desktop_shell_current_package_evidence_matches',shell['version']==version and shell['app']['checks_passed']==194 and shell['app']['hap_sha256']==app['hap']['sha256']==package['hap_sha256'] and shell['app']['evidence']==app['report_directory']+'/result.json' and package['desktop_shell_identity_enter_and_layout_tested'] is True)
    check('desktop_shell_scope_not_overstated',all(shell[k] is False for k in ('input_rewritten','full_core_registry_exposed','physical_keyboard_verified','global_keyboard_shortcuts_implemented','full_port_complete','physical_pc_verified','release_signing_verified')) and shell['functional_completion_percent'] is None)
    check('cli_graphics_failures_not_overwritten',
          cli['core_gl_egl_symbol_load']['passed'] is False and
          cli['core_gl_egl_symbol_load']['exit_code'] == 127 and
          cli['gles3_offscreen_diagnostic']['passed'] is False and
          cli['gles3_offscreen_diagnostic']['exit_code'] == 3 and cli['result'].startswith('partial:'))
    check('sphere_header_still_unmodified', digest('upstream/sdl/esphere.h') ==
          '24aef454dec3cbf1e77d635ddbe3b9f7bd89ffa80cf04f31b74a17ab267af54e')
except Exception as error:
    report['error'] = f'{type(error).__name__}: {error}'
    check('all_required_evidence_readable', False)
report['passed'] = bool(report['checks']) and all(c['passed'] for c in report['checks'])
local('reports/adaptation-consistency.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
for item in report['checks']:
    print(('PASS ' if item['passed'] else 'FAIL ') + item['name'])
if 'error' in report:
    print(report['error'])
print('Evidence consistency only; complete port and physical-PC qualification remain unverified.')
raise SystemExit(0 if report['passed'] else 1)
