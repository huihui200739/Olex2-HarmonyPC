#!/usr/bin/env python3
"""Synchronize the current 0.22 diagnostic summaries from retained passed evidence.

Does not run builds, modify runtime evidence, or turn partial diagnostics green.
The independent consistency checker must still run after this synchronization.
"""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import argparse,hashlib,json,re
from report_artifact_paths import baseline_artifact_paths
R=Path(__file__).resolve().parents[1]
def load(p):return json.loads((R/p).read_text())
def save(p,d):(R/p).write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256((R/p).read_bytes()).hexdigest()
def artifact(p):return dict(path=p,bytes=(R/p).stat().st_size,sha256=sha(p))
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--log-prefix',default='desktop-shell-verified');p.add_argument('--clean-build-directory');args=p.parse_args()
assert re.fullmatch(r'[a-z0-9-]+',args.log_prefix)
log=lambda name:'reports/app/'+args.log_prefix+'-'+('host-clean-build' if name=='host-build' else name)+'.log'
a=load('reports/app/last-run.json');n=load('reports/emulator/last-run.json');pkg=load('reports/app/package-verification.json');src=load('reports/source-integrity.json');version=load('app/AppScope/app.json5')['app']['versionName']
assert version=='0.22.0', 'Update this scoped synchronizer explicitly for another release.'
assert a['result'].startswith('passed:') and len(a['checks'])==194 and all(c['passed'] for c in a['checks'])
assert a['hap']['sha256']==sha('out/app/entry-default-unsigned.hap') and pkg['passed'] and pkg['build_provenance_verified'] and pkg['glossy_rendering_tested'] and src['passed']
assert n['original_triangle_commands']['passed'] and n['original_triangle_commands']['assertions_passed']==31805 and n['original_triangle_commands']['branch_state_cases_passed']==31752
assert pkg['original_synthetic_window_continuity_tested'] and pkg['original_window_integration']['original_ui_checks_preserved']==187
assert pkg['original_triangle_branch_tested']
assert pkg['original_polygon_branch_and_gpu_tessellation_tested']
assert n['original_polygon_commands']['passed'] and (n['original_polygon_commands']['assertions_passed'],n['original_polygon_commands']['branch_state_cases_passed'],n['original_polygon_commands']['independent_topology_poses_passed'])==(2052,1980,48)
assert '2052 assertions; 1980 branch-state cases; 48 independent topology poses;' in (R/log('host-original_polygons-tests')).read_text()
assert pkg['original_basis_matrix_tested'] and n['original_basis_matrices']['passed']
assert n['original_basis_matrices']['assertions_passed']==353 and n['original_basis_matrices']['independent_camera_poses_passed']==315
assert '353 assertions; 315 independent camera poses;' in (R/log('host-original_matrices-tests')).read_text()
assert '31805 assertions; 31752 branch-state cases;' in (R/log('host-original_triangles-tests')).read_text()
assert n['upstream_lit_material']['passed'] and n['upstream_lit_material']['assertions_passed']==137
assert n['upstream_material_commands']['passed'] and n['upstream_material_commands']['assertions_passed']==524360
assert '100% tests passed, 0 tests failed out of 18' in (R/log('host-build')).read_text()
assert '# tests 48\n' in (R/log('preference-tests')).read_text() and '# fail 0\n' in (R/log('preference-tests')).read_text()
assert '137 assertions;' in (R/log('host-lit_material-tests')).read_text()
assert pkg['original_desktop_command_interpreter_tested']
assert n['original_desktop_commands']['passed'] and (n['original_desktop_commands']['assertions_passed'],n['original_desktop_commands']['independent_view_combinations_passed'],n['original_desktop_commands']['serialized_concurrent_calls_passed'])==(1196,576,64)
assert '1196 assertions; 576 independent view combinations; 64 serialized concurrent calls;' in (R/log('host-desktop_commands-tests')).read_text()
from application_runtime_validation import app_runtime_evidence_matches,app_core_evidence_matches,runtime_snapshots
from options_document_validation import app_options_document_evidence_matches
assert pkg['options_document_system_picker_and_original_merge_tested'] and app_options_document_evidence_matches(a)
assert n['original_options_document_transfer']['passed'] and n['original_options_document_transfer']['assertions_passed']==23
assert '23 assertions;' in (R/log('host-options_transfer-tests')).read_text()
assert '# tests 26\n' in (R/log('options-document-model-tests')).read_text() and '# fail 0\n' in (R/log('options-document-model-tests')).read_text()
assert pkg['original_application_runtime_and_options_tested'] and app_runtime_evidence_matches(a)
assert pkg['original_core_empty_startup_in_actual_app_tested'] and app_core_evidence_matches(a)
assert n['original_application_runtime']['passed'] and (n['original_application_runtime']['assertions_passed'],n['original_application_runtime']['original_option_round_trips_passed'],n['original_application_runtime']['mixed_serialized_interpreter_calls_passed'])==(86,32,64)
assert '86 assertions; 32 original option round trips; 64 mixed serialized interpreter calls;' in (R/log('host-runtime-tests')).read_text()
from line_validation import line_command_log_matches,app_line_evidence_matches
assert pkg['original_line_branch_and_actual_index_buffer_tested'] and app_line_evidence_matches(a)
assert n['original_line_commands']['passed'] and n['original_line_commands']['independent_index_contracts_verified']
assert line_command_log_matches((R/log('host-original_lines-tests')).read_text())
assert line_command_log_matches((R/n['report_directory']/'original-lines-run.log').read_text())
from line_path_validation import app_path_evidence_matches
assert pkg['original_line_paths_gpu_probe_tested'] and app_path_evidence_matches(a)
assert n['original_line_path_commands']['passed'] and n['original_line_path_commands']['assertions_passed']==181512
assert '181512 assertions; 2688 raw branch-state cases; 1408 bounded branch-state cases;' in (R/log('host-original_line_paths-tests')).read_text()
from original_log_validation import parse_catalog_log,app_original_log_evidence_matches,SAMPLE_NAMES as LOG_NAMES,NATIVE_ASSERTIONS
host_catalog=parse_catalog_log((R/log('host-original_log-tests')).read_text());target_catalog=parse_catalog_log((R/n['report_directory']/'original-log-run.log').read_text())
assert host_catalog==target_catalog and n['original_log_bridge']['passed'] and n['original_log_bridge']['assertions_passed']==NATIVE_ASSERTIONS
assert pkg['original_log_and_core_catalog_ui_tested'] and app_original_log_evidence_matches(a,host_catalog)
from desktop_shell_validation import app_shell_evidence_matches,identity_snapshots,SAMPLES as SHELL_NAMES
assert pkg['desktop_shell_identity_enter_and_layout_tested'] and app_shell_evidence_matches(a,load('app/AppScope/app.json5')['app'])
assert '# tests 19\n' in (R/log('command-submit-model-tests')).read_text() and '# fail 0\n' in (R/log('command-submit-model-tests')).read_text()
checks={c['name']:c for c in a['checks']};now=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds');ap=a['report_directory']+'/result.json';cp=n['report_directory']+'/result.json';hap=a['hap']['sha256'];count=len(a['checks'])
clean_directory=args.clean_build_directory or 'out/ohos-arm64-clean-'+version
assert (R/clean_directory).resolve().is_relative_to((R/'out').resolve())
clean=load('reports/clean-build-status.json');clean.update(checked_at=now,version=version,source_lock_sha256=sha('source.lock.json'),build_directory=clean_directory,build_log=log('ohos-clean-build'),comparison_evidence=cp,artifacts=[])
for item in n['artifacts']:
 if item['name']=='libc++_shared.so':continue
 actual=artifact(clean['build_directory']+'/'+item['name']);actual['matches_target_tested_executable']=actual['sha256']==item['sha256'];assert actual['matches_target_tested_executable'];clean['artifacts'].append(actual)
assert len(clean['artifacts'])==19;save('reports/clean-build-status.json',clean)
b=load('reports/baseline-status.json');b.update(checked_at=now,native_app_version=version,native_app_report=ap,emulator_report=cp,native_app_renderer_scope=a['scope'],lit_material_tested=True,original_triangle_commands_tested=True,original_basis_matrices_tested=True,original_polygon_commands_tested=True,original_desktop_commands_tested=True,original_application_runtime_tested=True,original_options_document_transfer_tested=True,original_line_commands_tested=True,original_core_empty_startup_tested=True,original_line_paths_tested=True,original_log_bridge_tested=True)
b['checks'].update(host_build='passed: eighteen CTest cases in a fresh build; desktop APIs compile-only',host_smoke='passed: eighteen groups including 31805 original triangle commands, 31752 branch-state cases and 353 original matrix assertions / 315 independent camera poses and 2052 original polygon/tessellation assertions / 1980 branch cases / 48 topology poses',ohos_arm64_clean_build='passed: fresh build plus refreshed test target; all 19 executable hashes match target-tested bytes',gui_build='native diagnostic ArkUI/XComponent app 0.22.0 built; original full GUI unported',hap_package='0.22.0 unsigned debug diagnostic HAP installed/tested; not release-signed',pc_emulator_native_app='passed: 194 checks; original triangle/polygon uploads, independent matrices, isolated actual concave FBO pixels and polygon surface/cold-process recovery',desktop_preference_model='passed: 48 cases, schema1/2/3 to4 migration and 576 supported state combinations',pc_emulator_lit_material='passed: 137 target assertions; original specular/shininess commands and independent triangle-row reference',pc_emulator_glossy_rendering='passed: twelve geometry/color glossy samples compare actual RGB with independent perspective-correct CPU predictions and a specular-disabled baseline')
b['checks']['pc_emulator_original_desktop_commands']='passed: original TEMacroLib/TLibrary dispatch consumed by actual pc.* view adapters; 13 literal UI commands, rejection atomicity, surface/cold-process recovery; not full original command library'
b['checks']['pc_emulator_original_lines']='passed: original uncolored sgloLines drives actual wire EBO; mapped bytes independently checked against exact source-edge topology; CPU color/width traces are not GPU feature claims'
b['checks']['pc_emulator_original_triangles']='passed: original shared triangle branch drives actual uncolored mesh uploads; source, vertex order, normals, real pixels and lifecycle checked'
b['checks']['pc_emulator_original_matrices']='passed: original owned TEBasis camera, actual MVP/normal uniform readback, independent analytic validation, interaction and lifecycle; general scales/shears/stacks remain CPU evidence only'
paths=baseline_artifact_paths([item['path'] for item in b['artifacts']],n['build_directory'],[item['name'] for item in n['artifacts'] if item['name']!='libc++_shared.so'])
b['artifacts']=[artifact(path) for path in paths];save('reports/baseline-status.json',b)
m=load('reports/mesh-adapter-status.json');m.update(checked_at=now,version=version,app_checks_passed=count,app_evidence=ap,cli_evidence=cp,host_ctest_cases_passed=18,target_mesh_checks=n['upstream_mesh_adapter']);m['actual_app_mesh_samples']={k:checks[k]['details'].get('graphics',checks[k]['details']) for k in m['actual_app_mesh_samples']};save('reports/mesh-adapter-status.json',m)
for file in ['submission','primitive-adapter','desktop-state','color-adapter','material-adapter']:
 path='reports/'+file+'-status.json';d=load(path);d.update(checked_at=now,version=version)
 d['app'].update(checks_passed=count,evidence=ap,hap_sha256=hap)
 if 'host' in d:d['host'].update(ctest_cases_passed=18,build_log=log('host-build'))
 if 'ohos' in d:d['ohos'].update(evidence=cp)
 if file=='submission':
  d['host'].update(mesh_log=log('host-mesh-tests'),submission_log=log('host-submission-tests'),helper_primitive_log=log('host-primitive-tests'))
  d['ohos'].update(build_log=log('ohos-build'),mesh_stream_adapter=n['upstream_mesh_adapter'],shared_submission=n['shared_sphere_submission'],helper_primitives=n['upstream_helper_primitives']);d['app'].update(package_verified=True,default_batch=checks['actual_shared_submission_stream']['details'])
 elif file=='primitive-adapter':
  d['host']['helper_log']=log('host-primitive-tests');d['ohos'].update(build_log=log('ohos-build'),helper=n['upstream_helper_primitives'])
  d['app'].update(samples={k:checks[k]['details'].get('graphics',checks[k]['details']) for k in d['app']['samples']},surface_recreation_sample=checks['mesh_selection_survives_surface_recreation']['details'])
 elif file=='desktop-state':
  d['storage']['schema_version']=4;d['model'].update(source_sha256=sha(d['model']['source']),test_source_sha256=sha(d['model']['test_source']),cases_passed=48,evidence=log('preference-tests'),scope='synthetic schema1/2/3 to4 migration, strict historical ranges, 576 state combinations and save coalescing')
  d['app'].update(before_restart=a['state_before_restart'],after_restart=a['state_after_restart'],processes=checks['workspace_cold_restart_new_process']['details'])
 elif file=='color-adapter':
  d['host']['color_log']=log('host-color-tests');d['ohos']['color_values']=n['upstream_color_values'];d['app'].update(samples={k:v['details'] for k,v in checks.items() if k.startswith('color_primitive_')},surface_recreation_sample=checks['color_survives_surface_recreation']['details'],restored_color=a['state_after_restart']['graphics'])
 elif file=='material-adapter':
  d['presets_scope']='Legacy zero-specular presets0..2 and flags783 remain in the original fields; added preset3/flags1023 is described in lit.preset.'
  d['scope']='Original material commands drive symmetric opaque ambient/diffuse/specular/emission and bounded shininess in a defined custom GLES shader; not full fixed-function lighting'
  d['host']['material_log']=log('host-material-tests');d['ohos']['material_values']=n['upstream_material_values'];d['app'].update(samples={k:v['details'] for k,v in checks.items() if k.startswith('material_primitive_') and '_preset_' in k},lighting_responses={k:v['details'] for k,v in checks.items() if k.startswith('material_primitive_') and k.endswith('_lighting_response')},surface_recreation_sample=checks['material_survives_surface_recreation']['details'],restored_material=a['state_after_restart']['graphics'],small_window_controls_fully_visible=checks['small_window_view_controls_fully_visible']['details'])
  d['commands'].update(host_log=log('host-material_commands-tests'),ohos=n['upstream_material_commands'],app_sample=checks['original_material_commands_drive_uniforms']['details'],limits='All uint16 command traces have CPU evidence; GPU consumer adds bounded symmetric opaque specular/shininess, not transparency/color tracking/full fixed-function rendering.')
  d['lit']=dict(preset={'id':3,'name':'高光','ambient_factor':.3,'diffuse_factor':.4,'specular_factor':.3,'emission_factor':0,'shininess':32,'flags':1023},assertions_passed=137,host_log=log('host-lit_material-tests'),ohos=n['upstream_lit_material'],samples={k:v['details'] for k,v in checks.items() if k.startswith('glossy_primitive_')},before_restart=a['glossy_state_before_restart'],after_restart=a['glossy_state_after_restart'],formula='ambient + diffuse * max(dot(n,L),0) + specular * pow(max(dot(n,H),0),shininess) when dot(n,L)>0 + emission; per-vertex perspective interpolation; L=normalize(.35,.5,1), H=normalize(L+(0,0,1))',input_scope='SetLitPreset(0..3), symmetric opaque faces, finite normalized coefficients summing<=1; shininess0..128; current glossy preset32. Legacy Parse and SetPreset remain their original strict zero-specular subset.',validation='Twelve actual solid geometry/color samples against all CPU-reference interior row pixels and four retained raw samples each; same-mesh specular-zero GPU baseline; glossy surface/cold-process recovery and reset to zero terms.',full_fixed_function_lighting=False)
  extra=['native/lit_row_reference.h','native/lit_row_reference.cpp','native/tests/lit_material_smoke.cpp','scripts/app_report_log.py','scripts/tests/test_app_reports.py']
  d['sources']+=[{'path':p} for p in extra if p not in [i['path'] for i in d['sources']]]
  d['limits']=['Custom symmetric opaque lighting, not complete original fixed-function equivalence','No transparent blending/alpha test, asymmetric faces, color tracking, point smoothing, complete light state, full renderer or original UI','Legacy strict zero-specular parser remains bounded; added preset uses original typed material fields and command flow','CPU row reference handles visible solid triangles with positive clip w and no near/far clipping; wireframe has geometry coverage evidence, not glossy pixel-reference coverage','Host-only legacy schema migration evidence; actual schema4 glossy cold restart is separately retained','Unsigned debug API24 emulator only; physical PC/input and release unverified']
 if 'sources' in d:d['sources']=[dict(path=i['path'],sha256=sha(i['path'])) for i in d['sources']]
 save(path,d)
triangle_sources=['upstream/glib/trianglesubmit.h','upstream/glib/glprimitive.cpp','upstream/glib/glprimitive.h','native/primitive_triangles.h','native/primitive_triangles.cpp','native/tests/original_triangles_smoke.cpp','app/entry/src/main/cpp/renderer.cpp','app/entry/src/main/cpp/renderer.h','app/entry/src/main/cpp/napi_init.cpp','app/entry/src/main/cpp/CMakeLists.txt','app/entry/src/main/cpp/types/libolex2native/index.d.ts','app/entry/src/main/ets/pages/Index.ets','CMakeLists.txt','scripts/run-app-smoke.py','scripts/run-device-smoke.py']
triangle_names=['original_triangle_branch_drives_actual_upload','actual_upstream_mesh_source','mesh_detail_minimum','mesh_detail_maximum','mesh_wireframe_mode']+[f'primitive_{k}_detail_{d}_{mode}' for k in (1,2,3) for d in (0,3,5) for mode in ('solid','wire')]
triangle_data=dict(checked_at=now,version=version,scope='Shared original TGlPrimitive sgloTriangles vertex/normal/color command branch; actual app upload consumes its uncolored per-vertex-normal output; not the complete primitive class or renderer',upstream_commit=load('source.lock.json')['commit'],patch='patches/0007-extract-original-primitive-triangle-submission.patch',original_flow_preserved_exactly=True,host=dict(ctest_cases_passed=18,assertions_passed=31805,branch_state_cases_passed=31752,build_log=log('host-build'),test_log=log('host-original_triangles-tests'),desktop_full_glprimitive_translation_unit_compiled=True,desktop_runtime_tested=False),ohos=dict(evidence=cp,build_log=log('ohos-build'),commands=n['original_triangle_commands']),app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'].get('graphics',checks[name]['details']) for name in triangle_names},surface_recreation_sample=checks['mesh_selection_survives_surface_recreation']['details'],restored_sample=a['glossy_state_after_restart']['graphics']),sources=[dict(path=path,sha256=sha(path)) for path in triangle_sources],full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,full_primitive_class_target_compiled=True,gpu_vertex_color_tracking_implemented=False,selection_engine_implemented=False,limits=['CPU traces preserve per-vertex colors, ForcePlain and selection branch behavior; these do not implement GPU color tracking or picking','Current actual GLES route consumes complete uncolored per-vertex-normal triangles from existing shared geometry helpers','Strict target boundary rejects malformed attribute counts, incomplete triangles, nonfinite positions and undefined normals; not universal desktop GL acceptance','Begin/end state, full transforms/display lists, clipping, textures, font/GLU, original UI and application integration remain incomplete'])
save('reports/original-triangle-status.json',triangle_data)
matrix_sources=['upstream/sdl/ebasis.h','upstream/sdl/ebasis.cpp','patches/0008-add-owned-original-basis-matrix-exports.patch','native/original_matrices.h','native/original_matrices.cpp','native/tests/original_matrices_smoke.cpp','scripts/matrix_validation.py','scripts/tests/test_matrix_validation.py']+triangle_sources[6:]
matrix_names=['original_basis_drives_actual_matrix_uniforms','injected_keyboard_rotate','injected_keyboard_zoom','injected_keyboard_reset','injected_mouse_wheel_zoom','native_window_resize','native_window_restore']
# Use actual retained checks, not generated render states.
matrix_names=[name for name in matrix_names if name in checks]
assert len(matrix_names)==7, 'Matrix interaction/window evidence names changed; inspect actual checks.'
matrix_data=dict(checked_at=now,version=version,scope='Original owned TEBasis exports and unchanged RotateX/RotateY drive actual camera MVP and inverse-transpose normal uniforms; bounded explicit CPU matrix state, not full legacy GL or full renderer',
    upstream_commit=load('source.lock.json')['commit'],patch='patches/0008-add-owned-original-basis-matrix-exports.patch',
    original_export_assignments_preserved_exactly=True,original_basis_implementation_unchanged=True,
    host=dict(ctest_cases_passed=18,assertions_passed=353,independent_camera_poses_passed=315,build_log=log('host-build'),test_log=log('host-original_matrices-tests')),
    ohos=dict(evidence=cp,build_log=log('ohos-build'),matrices=n['original_basis_matrices']),
    app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'].get('graphics',checks[name]['details']) for name in matrix_names},
        surface_recreation_sample=checks['mesh_selection_survives_surface_recreation']['details'].get('graphics',checks['mesh_selection_survives_surface_recreation']['details']),
        restored_sample=a['glossy_state_after_restart']['graphics'],
        model_view_evidence='logical CPU state only',mvp_and_normal_evidence='actual glGetUniformfv readback independently checked against analytic camera coefficients'),
    sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(matrix_sources)],
    full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,
    full_gl_matrix_state_implemented=False,general_stack_sequences_gpu_tested=False,scale_shear_gpu_tested=False,legacy_basis_globally_thread_safe=False,
    limits=['Only current rigid camera reaches the actual shader; general scale/shear/stack operation correctness has CPU evidence only',
      'CopyMData and CopyMDataT use caller-owned buffers; legacy static getters and other static-buffer methods retain their original behavior, so the full class is not thread-safe',
      'Current projection remains the diagnostic perspective; no full original renderer projection, clipping, selection, display-list or application integration claim',
      'Finite float target boundary and depth32 stack are explicit adapter limits, not universal legacy GL acceptance',
      'Unsigned debug API24 emulator only; physical PC/input and release qualification unverified'])
save('reports/original-matrix-status.json',matrix_data)

polygon_sources=['upstream/glib/polygonsubmit.h','upstream/glib/glprimitive.cpp','patches/0009-extract-original-polygon-submission.patch','native/primitive_polygons.h','native/primitive_polygons.cpp','native/tests/original_polygons_smoke.cpp','native/primitive_batch.cpp','native/upstream_mesh.h','app/entry/src/main/cpp/polygon_probe.h','app/entry/src/main/cpp/polygon_probe.cpp','scripts/polygon_validation.py','scripts/tests/test_polygon_validation.py']+triangle_sources[6:]
polygon_names=['original_polygon_branch_and_concave_gpu_tessellation']+[f'primitive_2_detail_{d}_{mode}' for d in (0,3,5) for mode in ('solid','wire')]+[f'color_primitive_2_preset_{c}' for c in range(3)]+[f'glossy_primitive_2_color_{c}' for c in range(3)]
polygon_data=dict(checked_at=now,version=version,scope='Original sgloPolygon vertex/normal/color branch; actual six disk-helper boundaries flow through it before tessellation; new bounded simple-planar contour tessellator plus one isolated actual concave GPU fixture, not GLU/full renderer',
    upstream_commit=load('source.lock.json')['commit'],patch='patches/0009-extract-original-polygon-submission.patch',original_flow_preserved_exactly=True,
    host=dict(ctest_cases_passed=18,assertions_passed=2052,branch_state_cases_passed=1980,independent_topology_poses_passed=48,build_log=log('host-build'),test_log=log('host-original_polygons-tests'),desktop_full_glprimitive_translation_unit_compiled=True,desktop_runtime_tested=False),
    ohos=dict(evidence=cp,build_log=log('ohos-build'),commands=n['original_polygon_commands']),
    app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'].get('graphics',checks[name]['details']) for name in polygon_names},
        surface_recreation_sample=checks['polygon_survives_surface_recreation']['details']['graphics'],before_restart=a['polygon_state_before_restart'],after_restart=a['polygon_state_after_restart'],
        restart_processes=checks['polygon_cold_restart_new_process']['details'],gpu_fixture_count=1,gpu_probe_scope='one isolated synthetic 64x64 RGBA8 FBO U contour in the actual application context; three actual inside and three actual background pixels, including the concave notch',
        gpu_state_restore_evidence='main-context GL state restored and queried using integer boolean-state queries; original application EGL context/display/draw/read surfaces unchanged and queried; no context switch'),
    sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(polygon_sources)],
    full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,full_primitive_class_target_compiled=True,
    full_glu_implemented=False,holes_supported=False,nonplanar_polygons_supported=False,gpu_vertex_color_tracking_implemented=False,selection_engine_implemented=False,legacy_gl_polygon_concavity_equivalence_claimed=False,
    limits=['New tessellator accepts one bounded simple planar contour with optional closing duplicate; holes, intersections, touching boundaries, nonplanar/subprecision geometry and budget excess are explicitly rejected',
      'CPU topology checks use independent ray crossing, area and winding across 48 plane/scale/winding/closing poses; GPU concavity coverage is one isolated U contour, not universal GPU acceptance',
      'Actual disk helper keeps its convex-only contract and original fan/edge order; concave input is not silently accepted by that convex helper',
      'Original per-vertex color, ForcePlain and selecting branches have CPU command evidence only; GPU color tracking/picking remain unimplemented',
      'Desktop cull-face/begin/end wrapper remains real GL; target uses the current explicit rasterization policy, not the complete original class/state machine',
      'Full original application/UI, GLU/display lists/textures/fonts and release/physical PC qualification remain incomplete'])
save('reports/original-polygon-status.json',polygon_data)
print(version,count,ap,cp,hap)

from desktop_command_validation import SAMPLES,command_sample_matches,command_recovery_matches
assert all(command_sample_matches(name,checks[name]['details']) for name in SAMPLES)
assert command_recovery_matches(a)
command_sources=['native/desktop_commands.h','native/desktop_commands.cpp','native/tests/desktop_commands_smoke.cpp','upstream/sdl/macrolib.cpp','upstream/sdl/library.cpp','upstream/sdl/function.cpp','scripts/desktop_command_validation.py','scripts/tests/test_desktop_command_validation.py','scripts/check-app-package.py','scripts/check-adaptation-status.py','scripts/sync-verified-reports.py','scripts/tests/test_status_consistency.py','native/original_interpreter.h','native/original_interpreter.cpp']+[str(p.relative_to(R)) for p in sorted((R/'upstream/sdl/exparse').glob('*')) if p.is_file()]+triangle_sources[6:]
command_data=dict(checked_at=now,version=version,scope='Unmodified original TEMacroLib parser and TLibrary/AMacro dispatch consumed by explicit pc.* synthetic view adapters in the actual app; not the full original Olex2 application or command library',
    upstream_commit=load('source.lock.json')['commit'],upstream_interpreter_modified=False,
    host=dict(ctest_cases_passed=18,assertions_passed=1196,independent_view_combinations_passed=576,serialized_concurrent_calls_passed=64,build_log=log('host-build'),test_log=log('host-desktop_commands-tests')),
    ohos=dict(evidence=cp,build_log=log('ohos-build'),commands=n['original_desktop_commands']),
    app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'] for name in SAMPLES},
        input_observations=a['desktop_command_input_observations'],surface_recreation_sample=checks['desktop_command_survives_surface_recreation']['details'],
        before_restart=a['desktop_command_state_before_restart'],after_restart=a['desktop_command_state_after_restart'],restart_processes=checks['desktop_command_cold_restart_new_process']['details']),
    sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(command_sources)],
    full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,
    full_original_application_entry_integrated=False,full_original_ui_integrated=False,complete_original_command_library_integrated=False,
    python_integrated=False,callbacks_supported=False,global_processor_thread_safety_claimed=False,gpu_device_loss_rollback_claimed=False,
    command_history_persisted=False,command_history_replayed=False,
    limits=['Only pc.mesh/detail/wire/color/material/view/reset/help platform adapters are registered; full application macros, callbacks and Python remain unconnected',
      'Finite synthetic view state only; not real structures or domain workflows',
      'CPU validation is transactional for known adapter errors, but GPU/device-loss rollback is not claimed; applied records actual commit separately from passed',
      'Only this adapter serializes and restores the legacy global processor pointer; original interpreter global thread safety is not claimed',
      'View preferences survive surface and cold-process recreation; command text/history and execution sequence are neither persisted nor replayed',
      'Exact literal UI text is checked before execution; separately injected spaces avoid observed emulator batching loss without changing command grammar',
      'Unsigned debug emulator and injected keys only; physical keyboard/IME, PC hardware and release qualification unverified'])
save('reports/original-desktop-command-status.json',command_data)

from application_runtime_validation import SAMPLES as APPLICATION_SAMPLES
runtime_sources=['native/application_runtime.h','native/application_runtime.cpp','native/original_interpreter.h','native/original_interpreter.cpp','native/tests/application_runtime_smoke.cpp','upstream/sdl/bapp.h','upstream/sdl/bapp.cpp','upstream/sdl/settingsfile.h','upstream/sdl/settingsfile.cpp','upstream/sdl/ehashed.h','patches/0010-allow-platform-options-save-override.patch','patches/0011-initialize-and-reset-hashed-container-count.patch','patches/0012-add-explicit-utf8-settings-reader.patch','scripts/application_runtime_validation.py','scripts/tests/test_application_runtime_validation.py','app/entry/src/main/ets/entryability/EntryAbility.ets']+command_sources
runtime_data=dict(checked_at=now,version=version,scope='Actual TXApp empty core startup (core commands not exposed), plus TBasicApp directory/22-function registry with original settings format, platform atomic SaveOptions override, exact original licence mapped from HAP resource and a persisted UI setting; not complete Olex2 entry/resources or all original function qualification',
    upstream_commit=load('source.lock.json')['commit'],original_basic_app_implementation_unchanged=True,original_default_save_options_body_unchanged=True,
    platform_save_hook_patch='patches/0010-allow-platform-options-save-override.patch',hashed_container_count_patch='patches/0011-initialize-and-reset-hashed-container-count.patch',
    host=dict(ctest_cases_passed=18,assertions_passed=86,original_option_round_trips_passed=32,mixed_serialized_interpreter_calls_passed=64,build_log=log('host-build'),test_log=log('host-runtime-tests')),
    ohos=dict(evidence=cp,build_log=log('ohos-build'),runtime=n['original_application_runtime'],licence_fixture=n['application_runtime_fixture']),
    app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,baseline=checks['application_runtime_baseline']['details'],samples={name:checks[name]['details'] for name in APPLICATION_SAMPLES},before_samples=a['application_function_before_samples'],input_observations=a['application_function_input_observations'],
      before_restart=a['application_runtime_before_restart'],after_restart=a['application_runtime_after_restart'],restart_processes=checks['application_runtime_cold_restart_new_process']['details'],
      visible_setting_checks={name:checks[name]['details'] for name in ('application_runtime_hints_hidden','application_runtime_hints_visible')}),
    sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(runtime_sources)],
    resource_scope='Exact original OLEX2 licence only, copied atomically into a writable private base directory; not the complete original resource tree',
    full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,full_original_application_entry_integrated=False,full_original_ui_integrated=False,
    all_original_application_functions_runtime_tested=False,complete_original_resource_tree_integrated=False,bundled_resources_immutable_claimed=False,
    process_environment_modified=False,working_directory_modified=False,power_loss_durability_verified=False,global_processor_thread_safety_claimed=False,application_function_history_persisted=False,
    limits=['Original app factory registers 22 functions; the actual UI test exercises 10 named original functions, not all function/argument combinations',
      'Genuine TXApp empty core startup is connected; its 135 macros/38 functions are registered but not exposed or executed by the UI interpreter. TGXApp/Olex2App/wx, full command behavior, Python/plugins/resources and domain logic remain unconnected',
      'Original settings parser/serializer is used explicitly; desktop ReadOptions environment/PATH behavior is intentionally not executed',
      'Save hook validates bounded option text, writes an exclusive temporary file, checks staged original-format values, flushes and renames; parent-directory power-loss durability is not claimed',
      'Known pre-commit save errors restore the prior in-memory options; general nested original function side effects are not a global transaction',
      'Only local platform interpreter consumers share the current-processor lock; unrelated upstream callers and all original globals are not globally certified thread-safe',
      'Real ui.commandHints option changes visible ArkUI content, survives a distinct process with the same saved-file hash; expression sequence/history resets rather than replaying',
      'Original hashed-container count has explicit release initialization and Clear reset; no unrelated container redesign',
      'Unsigned API24 emulator and injected input only; physical PC, physical keyboard/IME and release qualification unverified'])
save('reports/original-application-runtime-status.json',runtime_data)

from options_document_validation import NAMES as DOCUMENT_NAMES
transfer_sources=['native/application_runtime.h','native/application_runtime.cpp','native/tests/options_transfer_smoke.cpp','upstream/sdl/settingsfile.h','upstream/sdl/settingsfile.cpp','patches/0012-add-explicit-utf8-settings-reader.patch','app/entry/src/main/ets/model/OptionsDocumentTransfer.ts','app/entry/src/main/ets/model/OptionsDocumentStore.ets','app/entry/src/main/ets/pages/Index.ets','app/entry/src/main/cpp/napi_init.cpp','app/entry/src/main/cpp/types/libolex2native/index.d.ts','scripts/options_document_validation.py','scripts/tests/test_options_document_validation.py','scripts/tests/options-document.test.mjs','scripts/run-app-smoke.py','scripts/run-device-smoke.py','scripts/check-app-package.py','scripts/check-adaptation-status.py','scripts/sync-verified-reports.py','scripts/tests/test_status_consistency.py']
transfer_data=dict(checked_at=now,version=version,scope='Actual system document save/select URI path with original-format read-only settings snapshot and validated key merge; not complete original application document open/save',
 host=dict(ctest_cases_passed=18,assertions_passed=23,build_log=log('host-build'),test_log=log('host-options_transfer-tests')),
 ohos=dict(evidence=cp,build_log=log('ohos-build'),runtime=n['original_options_document_transfer']),
 model=dict(cases_passed=26,evidence=log('options-document-model-tests'),scope='isolated synthetic byte-stream and operation-control fixtures; not platform I/O evidence'),
 app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'] for name in DOCUMENT_NAMES},before_samples=a['options_document_before_samples'],exported=a['options_document_exported'],imported=a['options_document_imported'],before_import=a['options_document_before_import'],before_restart=a['options_document_before_restart'],after_restart=a['options_document_after_restart'],filename_observation=a['options_document_filename_observation'],function_input_observations=a['options_document_function_input_observations']),
 sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(transfer_sources)],utf8_reader_patch='patches/0012-add-explicit-utf8-settings-reader.patch',maximum_document_bytes=1048576,
 full_port_complete=False,physical_pc_verified=False,original_olex2_renderer_tested=False,complete_application_document_open_save_supported=False,restorable_uri_history_persisted=False,uri_history_replayed=False,diagnostic_record_contains_last_selected_uri=True,external_destination_atomicity_claimed=False,power_loss_durability_verified=False,legacy_settings_reader_behavior_changed=False,original_settings_save_body_changed=False,
 limits=['Original settings keys are merged; absent existing keys and existing private-file comments remain, imported file formatting/comments are not wholesale substituted',
 'UTF-8 with optional BOM is explicit; original legacy LoadSettings and original SaveSettings body retain their existing behavior',
 'Export is a snapshot at operation start, not a claim that concurrent later changes are included; the effective UI default is included without committing private settings',
 'Only URIs returned by system document save/select are used; no broad storage permission or system setting modification',
 'External providers do not offer the private atomic rename: partial writes are completed, flushed and byte-read-back checked; failure may leave a partial selected destination and never deletes user files',
 'The document UI intentionally has no live XComponent; its inactive graphics flags are not GPU success evidence; all active graphics snapshots remain independently checked',
 'Distinct-process import recovery resets transfer/function/view-command histories rather than replaying; not complete Olex2 document or physical-PC/release qualification'])
save('reports/options-document-status.json',transfer_data)

line_sources=['native/primitive_lines.h','native/primitive_lines.cpp','native/tests/original_lines_smoke.cpp','upstream/glib/linesubmit.h','upstream/glib/glprimitive.cpp','upstream/glib/glprimitive.h','patches/0013-extract-original-line-submission.patch','CMakeLists.txt','app/entry/src/main/cpp/CMakeLists.txt','app/entry/src/main/cpp/renderer.h','app/entry/src/main/cpp/renderer.cpp','app/entry/src/main/cpp/napi_init.cpp','app/entry/src/main/cpp/types/libolex2native/index.d.ts','app/entry/src/main/ets/pages/Index.ets','scripts/line_validation.py','scripts/tests/test_line_validation.py','scripts/tests/test_ui_input_injection.py','scripts/tests/test_device_build_directory.py','scripts/run-app-smoke.py','scripts/run-device-smoke.py','scripts/check-app-package.py','scripts/check-adaptation-status.py','scripts/sync-verified-reports.py','scripts/tests/test_status_consistency.py']
line_names=[name for name in checks if name=='original_line_branch_drives_actual_wire_upload' or name=='mesh_wireframe_mode' or name.startswith('primitive_') and name.endswith('_wire')]
line_data=dict(checked_at=now,version=version,scope='Exact original sgloLines body drives existing uncolored wire EBO order; mapped actual GPU EBO bytes independently checked against 24 source-edge topology contracts; existing triangle normals/custom shader retained, not fixed-function state equivalence',
    patch='patches/0013-extract-original-line-submission.patch',original_flow_preserved_exactly=True,
    host=dict(ctest_cases_passed=18,assertions_passed=104199,branch_state_cases_passed=640,source_edge_meshes_passed=24,test_log=log('host-original_lines-tests'),build_log=log('host-build'),desktop_full_glprimitive_translation_unit_compiled=True,desktop_runtime_tested=False),
    ohos=dict(evidence=cp,commands=n['original_line_commands'],build_log=log('ohos-build'),test_log=n['report_directory']+'/original-lines-run.log'),
    app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'].get('graphics',checks[name]['details']) for name in line_names},surface_recreation_sample=checks['mesh_selection_survives_surface_recreation']['details'],restored_sample=a['state_after_restart']),
    sources=[dict(path=path,sha256=sha(path)) for path in line_sources],full_port_complete=False,physical_pc_verified=False,
    gpu_vertex_color_tracking_implemented=False,gpu_wide_line_state_implemented=False,fixed_function_normal_state_equivalent=False,full_primitive_class_target_compiled=True,line_strip_or_loop_shared=True,
    limits=['This primary mesh summary remains sgloLines-only; newly shared strip/loop branches have separate bounded synthetic FBO evidence, not primary-mesh path replacement','CPU color/ForcePlain/selection and scaled/restored width traces do not implement GPU color tracking, picking or arbitrary line widths','Actual line indices preserve original source edge boundaries, including no triangulation diagonals; retained triangle normals/custom material shader are not original incoming-normal state','Source/host/OHOS CLI/actual EBO bytes/window pixels/lifecycle evidence are separate; full original renderer, UI, physical PC and release remain unverified'])
save('reports/original-line-status.json',line_data)

# The actual HAP compile provenance, not an isolated preflight archive, is the
# authoritative source of core inclusion. Registry counts are not behavior tests.
native=load('out/app/entry-default-unsigned.provenance.json')['native']['compiled_sources']
core_compiled=[{key:item[key] for key in ('path','bytes','sha256')} for item in native if item['path'].startswith('upstream/xlib/') or item['path'] in ('upstream/repository/fsext.cpp','upstream/repository/hkl_util.cpp','upstream/repository/refinement_listener.cpp')]
assert len(core_compiled)==275 and all('-fPIC' in item['command'] for item in native if item['path'] in {v['path'] for v in core_compiled})
core_paths=list(dict.fromkeys(['scripts/tests/test_device_build_directory.py','cmake/Olex2Core.cmake','CMakeLists.txt','app/entry/src/main/cpp/CMakeLists.txt','upstream/xlib/xapp.h','upstream/xlib/xapp.cpp']+runtime_sources+[v['path'] for v in core_compiled]))
core_snapshots=list(runtime_snapshots(a))
core_data=dict(checked_at=now,version=version,scope='Genuine public non-dummy TXApp startup/destruction and empty container; only two actual original metadata catalog macros are separately exposed, not full core command behavior',
 upstream_commit=load('source.lock.json')['commit'],
 host=dict(ctest_cases_passed=18,runtime_assertions_passed=86,repeated_lifecycle_cases_passed=8,failed_private_settings_startup_recovery_tested=True,test_log=log('host-runtime-tests'),build_log=log('host-build')),
 ohos=dict(evidence=cp,runtime=n['original_application_runtime']),
 app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,baseline=checks['original_core_runtime_empty_startup']['details'],runtime_snapshot_count=len(core_snapshots)),
 compiled_core_sources=core_compiled,compiled_core_source_count=len(core_compiled),
 sources=[dict(path=path,sha256=sha(path)) for path in core_paths],
 genuine_core_empty_startup_integrated=True,core_registry_commands_exposed_to_ui=True,core_registry_commands_runtime_tested=True,
 exposed_core_metadata_macros=["core.LstMac","core.LstFun"],all_core_registry_commands_exposed_to_ui=False,all_core_registry_commands_runtime_tested=False,original_scientific_command_behavior_tested=False,
 original_scientific_documents_loaded=False,full_original_application_entry_integrated=False,full_original_ui_integrated=False,complete_original_resource_tree_integrated=False,
 full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,physical_keyboard_mouse_verified=False,release_signing_verified=False,
 limits=['135 core macros/38 functions are registration counts, not full behavior coverage; app.* evaluator still has 22 basic functions, separate core.* catalog has only original LstMac/LstFun',
 'No original scientific document or scientific core-command behavior, original GUI/renderer, Python, plugin or external-tool qualification; only two metadata macros actually execute',
 'Existing private directory mapping, exact licence, original UTF-8 settings, atomic save hook and system document setting transfer remain in use',
 'Eight isolated repeated lifecycle cases and invalid-settings restart do not certify every failure path or global thread safety',
 'Source inclusion ratio is not a functional-completion metric; physical hardware/input and release remain unverified'])
save('reports/original-core-runtime-status.json',core_data)

from line_validation import graphic_snapshots as path_graphics
path_snapshots=list(path_graphics(a));path_active=[g for _,g in path_snapshots if g.get('surfaceReady') is True and g.get('initialized') is True];path_inactive=[g for _,g in path_snapshots if g.get('surfaceReady') is False and g.get('initialized') is False]
path_sources=['scripts/report_artifact_paths.py','scripts/tests/test_report_artifact_paths.py','source.lock.json','upstream/glib/linepathsubmit.h','upstream/glib/glprimitive.cpp','patches/0014-extract-original-line-path-submission.patch','native/primitive_line_paths.h','native/primitive_line_paths.cpp','native/tests/original_line_paths_smoke.cpp','app/entry/src/main/cpp/line_path_probe.h','app/entry/src/main/cpp/line_path_probe.cpp','scripts/line_path_validation.py','scripts/tests/test_line_path_validation.py','scripts/check-source.py']+line_sources
path_data=dict(checked_at=now,version=version,scope='Exact original strip/loop bodies, bounded CPU source-edge topology and two uncolored width-one synthetic contours in the actual window GLES context; not primary mesh path replacement or full renderer',patch='patches/0014-extract-original-line-path-submission.patch',
 host=dict(ctest_cases_passed=18,assertions_passed=181512,raw_branch_state_cases_passed=2688,bounded_branch_state_cases_passed=1408,independent_edge_topologies_passed=2,test_log=log('host-original_line_paths-tests'),build_log=log('host-build'),desktop_full_glprimitive_compiled=True,desktop_runtime_tested=False),
 ohos=dict(evidence=cp,commands=n['original_line_path_commands'],test_log=n['report_directory']+'/original-line-paths-run.log'),
 app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,sample=checks['original_line_path_gpu_probe']['details'],active_graphic_snapshots=len(path_active),inactive_document_snapshots=len(path_inactive),actual_gpu_cases=2,readback_regions_per_case=8,region_pixel_count=9),
 sources=[dict(path=path,sha256=sha(path)) for path in dict.fromkeys(path_sources)],full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,
 original_flow_preserved_exactly=True,primary_mesh_line_source_changed=False,gpu_vertex_color_tracking_implemented=False,gpu_wide_line_state_implemented=False,gpu_selecting_implemented=False,fixed_function_normal_state_equivalent=False,full_renderer_implemented=False,
 limits=['Raw strip and loop selecting/color request behavior remains intentionally different; target boundary does not rewrite legacy 0/1-point or malformed-color bodies','Two synthetic uncolored multiplier-one contours only; no colored/wide/picking/fixed-function-normal GPU qualification','Mapped actual EBO indices/digests and independent consecutive/closed topology plus 16 pixel neighborhoods; not full framebuffer/driver conformance','All active retained contexts are checked and inactive document proof is cleared; no OOM/device-loss fault injection','Existing primary meshes still use original sgloLines; full original renderer/UI and hardware/release remain unverified'])
save('reports/original-line-path-status.json',path_data)

log_sources=['native/original_log.h','native/original_log.cpp','native/tests/original_log_smoke.cpp','native/application_runtime.h','native/application_runtime.cpp','native/original_interpreter.h','native/original_interpreter.cpp','upstream/sdl/log.h','upstream/sdl/log.cpp','upstream/sdl/actions.h','upstream/sdl/actions.cpp','upstream/sdl/function.h','upstream/sdl/library.cpp','upstream/xlib/xmacro.cpp','CMakeLists.txt','app/entry/src/main/cpp/CMakeLists.txt','app/entry/src/main/cpp/napi_init.cpp','app/entry/src/main/cpp/types/libolex2native/index.d.ts','app/entry/src/main/ets/pages/Index.ets','scripts/original_log_validation.py','scripts/tests/test_original_log_validation.py','scripts/tests/test_status_consistency.py','scripts/run-app-smoke.py','scripts/run-device-smoke.py','scripts/check-app-package.py','scripts/check-adaptation-status.py','scripts/sync-verified-reports.py']
log_data=dict(checked_at=now,version=version,scope='Actual original TLog OnPost observer and genuine LstMac/LstFun metadata macros in target application UI; not complete core command behavior or full entry point',
 host=dict(ctest_cases_passed=18,assertions_passed=NATIVE_ASSERTIONS,original_severity_cases_passed=5,real_core_catalog_macros_tested=2,locally_serialized_caller_posts_passed=400,test_log=log('host-original_log-tests'),build_log=log('host-build'),catalog_reference=host_catalog),
 ohos=dict(evidence=cp,bridge=n['original_log_bridge'],test_log=n['report_directory']+'/original-log-run.log'),
 app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,samples={name:checks[name]['details'] for name in LOG_NAMES}),
 limits_config=dict(max_entries=256,max_retained_utf8_bytes=65536,max_entry_utf8_bytes=4096),
 sources=[dict(path=path,sha256=sha(path)) for path in log_sources],
 exposed_original_core_metadata_macros=['core.LstMac','core.LstFun'],original_metadata_macros_runtime_tested=True,full_core_registry_exposed=False,scientific_command_behavior_tested=False,global_legacy_thread_safety_claimed=False,log_severity_tracking_implemented=False,direct_log_stream_writes_captured=False,durable_log_storage_implemented=False,automatic_log_restore_or_replay=False,oom_failure_injection_tested=False,
 full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,release_signing_verified=False,
 retained_development_failures=['reports/app/original-log-development-host-tests.log','reports/app/original-log-development-host-tests-corrected.log','reports/app/original-log-development-host-tests-initial-ctest-detail.log','reports/app/original-log-development-host-crash-extract.json','reports/app/original-log-development-host-serialized-tests.log','reports/app/original-log-development-host-catalog-check.log'],
 limits=['OnPost has no severity metadata; raw/direct stream writes and original verbose filtering are not redefined','Caller work using original strings/hash/interpreter remains locally serialized by OriginalInterpreterGuard; unsupported unguarded legacy parallel failure retained, not certified fixed globally','Only LstMac/LstFun clones in a separate core.* library execute; 135/38 metadata inventory is not full macro/function qualification','Bounded ring is transient, clear preserves sequences; compact diagnostics first/last samples may be retained privately and tests retain actual UI text, not automatic full-log persistence/replay','Original catalog expressions retain original grammar, including parentheses and prefix log events of a failing chain; no transactional rollback claim','No physical hardware, release, OOM/device-loss or complete original application/UI/renderer qualification'])
save('reports/original-log-status.json',log_data)


shell_sources=['scripts/tests/test_ui_input_injection.py','app/AppScope/app.json5','app/entry/src/main/ets/pages/Index.ets','app/entry/src/main/ets/entryability/EntryAbility.ets','app/entry/src/main/ets/model/CommandSubmit.ts','scripts/tests/command-submit.test.mjs','scripts/desktop_shell_validation.py','scripts/tests/test_desktop_shell_validation.py','scripts/run-app-smoke.py','scripts/check-app-package.py','scripts/sync-verified-reports.py','scripts/check-adaptation-status.py','scripts/tests/test_status_consistency.py']
shell_data=dict(checked_at=now,version=version,scope='Installed bundle identity, actual Enter dispatch between existing restricted pc/app interpreters, usable sidebar labels and honest paused-surface status',
 model=dict(cases_passed=19,evidence=log('command-submit-model-tests')),
 app=dict(evidence=ap,hap_sha256=hap,checks_passed=count,identity_snapshots_checked=len(list(identity_snapshots(a))),samples={name:checks[name]['details'] for name in SHELL_NAMES}),
 sources=[dict(path=path,sha256=sha(path)) for path in shell_sources],
 input_rewritten=False,full_core_registry_exposed=False,physical_keyboard_verified=False,global_keyboard_shortcuts_implemented=False,
 full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,release_signing_verified=False,
 retained_previous_verified_baseline='reports/app/0.20.0-pre-desktop-shell-summaries/manifest.json',
 limits=['Installed identity comes from own BundleManager API, never a hardcoded source/version fallback; API failure displays unknown/error',
 'Enter routes the literal app.* prefix to the existing 22-function interpreter, other inputs remain on the existing limited pc.* view interpreter; full core registry is not added',
 'Only actual app.ConfigDir()/pc.help Enter results and unchanged other interpreter/settings are qualified; other routing strings are isolated model tests',
 'Observed labels and bounds plus explicit source padding/layout and screenshots are checked; not all fonts/DPI/accessibility conformance',
 'Drawing paused means the current records page has no active drawing surface; it does not erase retained successful GPU evidence',
 'Injected emulator key events are not physical keyboard, global shortcut, full original UI, physical PC or release qualification'])
save('reports/desktop-shell-status.json',shell_data)
