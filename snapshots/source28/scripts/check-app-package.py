#!/usr/bin/env python3
"""Inspect the locally built debug HAP and compare it to actual app-test evidence."""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile
from zoneinfo import ZoneInfo
from matrix_validation import original_matrices_match
from desktop_command_validation import SAMPLES as COMMAND_SAMPLES,command_sample_matches,command_recovery_matches
from line_validation import app_line_evidence_matches
from line_path_validation import app_path_evidence_matches
from original_log_validation import parse_catalog_log,app_original_log_evidence_matches
from desktop_shell_validation import app_shell_evidence_matches
from polygon_validation import polygon_probe_matches,original_polygon_upload_matches
from application_runtime_validation import app_core_evidence_matches,app_runtime_evidence_matches
from options_document_validation import app_options_document_evidence_matches
from original_renderer_integration_validation import validate_application
from build_provenance import verify as verify_build_provenance

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--hap', type=Path, default=ROOT / 'out/app/entry-default-unsigned.hap')
p.add_argument('--sdk', type=Path, default=Path(os.environ.get('OHOS_NATIVE_HOME', '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')))
a = p.parse_args()
report = {'checked_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'), 'full_port_complete': False}
try:
    lock = json.loads((ROOT / 'source.lock.json').read_text())
    expected = json.loads((ROOT / 'app/AppScope/app.json5').read_text())['app']
    last = json.loads((ROOT / 'reports/app/last-run.json').read_text())
    tested_checks = {c['name']: c['passed'] for c in last['checks']}
    primitive_checks = [f'primitive_{kind}_detail_{detail}_{mode}' for kind in [1, 2, 3] for detail in [0, 3, 5] for mode in ['solid', 'wire']]
    input_checks = ['injected_keyboard_rotate', 'injected_keyboard_zoom', 'injected_keyboard_wireframe', 'injected_keyboard_primitive', 'injected_keyboard_reset', 'injected_mouse_wheel_zoom']
    color_checks = [f'color_primitive_{kind}_preset_{preset}' for kind in range(4) for preset in range(3)] + ['color_survives_surface_recreation', 'workspace_color_restored_after_restart']
    material_checks = [f'material_primitive_{k}_color_{c}_preset_{m}' for k in range(4) for c in (range(3) if k==0 else [0]) for m in range(3)] + [f'material_primitive_{k}_color_{c}_lighting_response' for k in range(4) for c in (range(3) if k==0 else [0])] + ['material_survives_surface_recreation','workspace_material_restored_after_restart']
    glossy_checks=[f'glossy_primitive_{kind}_color_{color}' for kind in range(4) for color in range(3)]+['glossy_surface_recreated','glossy_survives_surface_recreation','glossy_state_saved','glossy_cold_restart_new_process','glossy_restored_after_restart','glossy_return_to_classic_clears_state']
    persistence_checks = ['workspace_state_saved', 'workspace_cold_restart_new_process', 'workspace_selection_restored_after_restart', 'workspace_view_restored_after_restart']
    digest = lambda data: hashlib.sha256(data).hexdigest()
    with zipfile.ZipFile(a.hap) as z:
        app = json.loads(z.read('module.json'))['app']
        libs = sorted(n for n in z.namelist() if n.endswith('.so'))
        native = z.read('libs/arm64-v8a/libolex2native.so')
        licence = z.read('resources/rawfile/OLEX2-LICENCE.txt')
        gl4es_licence=z.read('resources/rawfile/GL4ES-LICENSE')
        glu_source=z.read('resources/rawfile/GLU-original-source.tar.gz')
        notices=z.read('resources/rawfile/THIRD-PARTY-NOTICES.txt')
        with tempfile.NamedTemporaryFile(dir=ROOT / 'out/app', suffix='.so') as f:
            f.write(native)
            f.flush()
            elf = subprocess.check_output([str(a.sdk / 'llvm/bin/llvm-readelf'), '-h', '-d', f.name], text=True)
    (ROOT / 'reports/app/native-elf.txt').write_text(elf)
    provenance = verify_build_provenance(a.hap, a.hap.with_suffix('.provenance.json'), ROOT)
    report['build_provenance'] = provenance
    report['original_window_integration'] = validate_application(last,ROOT,{k:expected[k] for k in ('bundleName','versionName','versionCode')})
    report['original_synthetic_window_continuity_tested'] = report['original_window_integration']['passed']
    report.update({
        'hap_sha256': digest(a.hap.read_bytes()),
        'matches_installed_and_tested_hap': digest(a.hap.read_bytes()) == last['hap']['sha256'],
        'helper_primitives_tested': all(tested_checks.get(name, False) for name in primitive_checks),
        'desktop_input_injection_tested': all(tested_checks.get(name, False) for name in input_checks),
        'glossy_rendering_tested': all(tested_checks.get(name,False) for name in glossy_checks),
        'options_document_system_picker_and_original_merge_tested': app_options_document_evidence_matches(last),
        'original_application_runtime_and_options_tested': app_runtime_evidence_matches(last),
        'original_core_empty_startup_in_actual_app_tested': app_core_evidence_matches(last),
        'original_line_paths_gpu_probe_tested': app_path_evidence_matches(last),
        'desktop_shell_identity_enter_and_layout_tested': app_shell_evidence_matches(last,expected),
        'original_log_and_core_catalog_ui_tested': app_original_log_evidence_matches(last,parse_catalog_log((ROOT/'reports/app/graphics-0-22-verified-host-original_log-tests.log').read_text())),
        'original_desktop_command_interpreter_tested': all(name in {c['name'] for c in last['checks']} and command_sample_matches(name,next(c['details'] for c in last['checks'] if c['name']==name)) for name in COMMAND_SAMPLES) and command_recovery_matches(last),
        'original_polygon_branch_and_gpu_tessellation_tested': tested_checks.get('original_polygon_branch_and_concave_gpu_tessellation',False) and polygon_probe_matches(last.get('final_report',{}).get('graphics',{})) and original_polygon_upload_matches(last.get('final_report',{}).get('graphics',{})),
        'original_basis_matrix_tested': tested_checks.get('original_basis_drives_actual_matrix_uniforms',False) and original_matrices_match(last.get('final_report',{}).get('graphics',{})),
        'original_line_branch_and_actual_index_buffer_tested': app_line_evidence_matches(last),
        'original_triangle_branch_tested': tested_checks.get('original_triangle_branch_drives_actual_upload',False),
        'upstream_material_commands_tested': tested_checks.get('original_material_commands_drive_uniforms',False),
        'upstream_material_pixels_tested': all(tested_checks.get(name,False) for name in material_checks),
        'upstream_color_pixels_tested': all(tested_checks.get(name, False) for name in color_checks),
        'desktop_state_persistence_tested': all(tested_checks.get(name, False) for name in persistence_checks),
        'physical_pc_verified': False,
        'shared_submission_tested': last.get('final_report', {}).get('upstreamSubmissionTested', False),
        'last_app_test_passed': last['result'].startswith('passed:') and all(c['passed'] for c in last['checks']),
        'bundle_name': app['bundleName'], 'version': app['versionName'],
        'package_identity_matches_source': all(app[k] == expected[k] for k in ['bundleName', 'versionName', 'versionCode']),
        'debug_build': app.get('debug') is True and app.get('buildMode') == 'debug',
        'signing_mode': 'unsigned debug as configured; no formal signing verified',
        'libraries': libs,
        'expected_native_libraries_only': libs == ['libs/arm64-v8a/libc++_shared.so', 'libs/arm64-v8a/libolex2core_graphics.so', 'libs/arm64-v8a/libolex2native.so'],
        'vendor_license_and_source_notice_retained': gl4es_licence==(ROOT/'vendor/gl4es/LICENSE').read_bytes() and glu_source==(ROOT/'vendor/GLU-original-source.tar.gz').read_bytes() and notices==(ROOT/'app/entry/src/main/resources/rawfile/THIRD-PARTY-NOTICES.txt').read_bytes(),
        'glu_original_source_archive_sha256':digest(glu_source),
        'licence_sha256': digest(licence), 'licence_matches_upstream': digest(licence) == lock['license_sha256'],
        'native_library_sha256': digest(native), 'elf_aarch64': 'AArch64' in elf,
        'native_dependencies': re.findall(r'Shared library: \[([^\]]+)\]', elf),
        'no_desktop_gl_dependency': 'libGLv4.so' not in elf,
        'no_glu_dependency': 'libGLU' not in elf,
    })
    report['build_provenance_verified'] = provenance['passed'] and provenance['workspace_matches']
    gates = ['vendor_license_and_source_notice_retained', 'original_synthetic_window_continuity_tested', 'desktop_shell_identity_enter_and_layout_tested', 'original_log_and_core_catalog_ui_tested', 'original_line_branch_and_actual_index_buffer_tested', 'options_document_system_picker_and_original_merge_tested', 'original_application_runtime_and_options_tested', 'original_core_empty_startup_in_actual_app_tested', 'original_line_paths_gpu_probe_tested', 'original_desktop_command_interpreter_tested', 'original_polygon_branch_and_gpu_tessellation_tested', 'original_basis_matrix_tested', 'original_triangle_branch_tested', 'build_provenance_verified', 'matches_installed_and_tested_hap', 'last_app_test_passed', 'package_identity_matches_source', 'debug_build',
             'shared_submission_tested', 'helper_primitives_tested', 'desktop_input_injection_tested', 'desktop_state_persistence_tested', 'upstream_color_pixels_tested', 'upstream_material_pixels_tested', 'upstream_material_commands_tested', 'glossy_rendering_tested', 'no_glu_dependency', 'expected_native_libraries_only', 'licence_matches_upstream', 'elf_aarch64', 'no_desktop_gl_dependency']
    report['passed'] = all(report[k] for k in gates)
except Exception as error:
    report['passed'] = False
    report['error'] = f'{type(error).__name__}: {error}'
(ROOT / 'reports/app/package-verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(0 if report['passed'] else 1)
