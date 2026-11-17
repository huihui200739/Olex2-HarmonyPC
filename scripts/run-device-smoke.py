#!/usr/bin/env python3
"""Run Olex2 utility and geometry adapters on an explicitly selected OHOS target."""
from line_validation import line_command_log_matches
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
from original_log_validation import parse_catalog_log, NATIVE_ASSERTIONS
import re
import shlex
import subprocess
import uuid
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', required=True, help='Explicit hdc target from hdc list targets')
parser.add_argument('--repeats', type=int, default=3)
parser.add_argument('--build-directory', type=Path, default=Path('out/ohos-arm64'), help='Explicit existing production target build inside this project out directory')
parser.add_argument('--sdk', type=Path, default=Path(os.environ.get(
    'OHOS_NATIVE_HOME', '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native')))
parser.add_argument('--hdc', type=Path, default=Path(os.environ.get(
    'HDC_BIN', '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')))
args = parser.parse_args()
if not 1 <= args.repeats <= 10:
    parser.error('--repeats must be between 1 and 10')
def select_build_directory(directory):
    selected=(ROOT/directory).resolve()
    if not selected.is_relative_to((ROOT/'out').resolve()) or not selected.is_dir():
        raise ValueError('--build-directory must be an existing build directory inside this project out directory')
    return selected
try:
    build_directory=select_build_directory(args.build_directory)
except ValueError as error:
    parser.error(str(error))
now = datetime.now(ZoneInfo('Asia/Shanghai'))
run_id = now.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:6]
report_dir = ROOT / 'reports/emulator' / run_id
report_dir.mkdir(parents=True, exist_ok=False)
summary = {
    'started_at': now.isoformat(timespec='seconds'),
    'scope': 'OlexSys utility, geometry submission, original color/material values and bounded adapters; not complete Olex2 or original renderer',
    'target': args.target,
    'build_directory': str(build_directory.relative_to(ROOT)),
    'report_directory': str(report_dir.relative_to(ROOT)),
    'utility_runs': [],
    'olex2_rendering_tested': False,
    'gui_tested': False,
    'physical_pc_verified': False,
    'full_port_complete': False,
}
remote_dir = None
uploaded = []

def remote(command, log_name, timeout=45):
    """hdc shell exit status is not sufficient; require a remote exit marker."""
    marker = 'OLEX2_EXIT_' + uuid.uuid4().hex
    wrapped = '( ' + command + ' ); result=$?; printf "\\n' + marker + '=%s\\n" "$result"'
    result = subprocess.run([str(args.hdc), '-t', args.target, 'shell', wrapped],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=timeout)
    output = result.stdout.replace('\r\n', '\n')
    (report_dir / log_name).write_text(output)
    matches = re.findall(r'^' + marker + r'=(\d+)\s*$', output, re.MULTILINE)
    if result.returncode != 0 or len(matches) != 1:
        raise RuntimeError(f'{log_name}: missing remote exit marker / hdc failure')
    cleaned = re.sub(r'\n' + marker + r'=\d+\s*$', '\n', output)
    return int(matches[0]), cleaned

try:
    files = [build_directory / 'olex2_sdl_smoke',
             build_directory / 'olex2_mesh_smoke',
             build_directory / 'olex2_submission_smoke',
             build_directory / 'olex2_primitive_smoke',
             build_directory / 'olex2_color_smoke',
             build_directory / 'olex2_material_smoke',
             build_directory / 'olex2_material_commands_smoke',
             build_directory / 'olex2_lit_material_smoke',
             build_directory / 'olex2_original_triangles_smoke',
             build_directory / 'olex2_original_lines_smoke',
             build_directory / 'olex2_original_line_paths_smoke',
             build_directory / 'olex2_original_matrices_smoke',
             build_directory / 'olex2_original_polygons_smoke',
             build_directory / 'olex2_desktop_commands_smoke',
             build_directory / 'olex2_application_runtime_smoke',
             build_directory / 'olex2_options_transfer_smoke',
             build_directory / 'olex2_original_log_smoke',
             build_directory / 'olex2_gl_core_probe',
             build_directory / 'olex2_gles_egl_smoke',
             args.sdk / 'llvm/lib/aarch64-linux-ohos/libc++_shared.so']
    for file in files:
        if not file.is_file():
            raise FileNotFoundError(file)
    command = ('echo ARCH; uname -m; echo DEVICE_TYPE; param get const.product.devicetype; '
               'echo MODEL; param get const.product.model; echo API; param get const.ohos.apiversion; '
               'echo SYSTEM; param get const.ohos.fullname')
    code, info = remote(command, 'device-info.log')
    if code != 0:
        raise RuntimeError('Could not query target metadata')
    summary['device_info'] = info.strip()
    def value_after(label):
        lines = info.splitlines()
        return lines[lines.index(label) + 1].strip()
    if value_after('ARCH') != 'aarch64':
        raise RuntimeError('This baseline is ARM64, but the selected target is not')
    summary['device_type'] = value_after('DEVICE_TYPE')
    summary['system'] = value_after('SYSTEM')
    summary['api'] = value_after('API')
    summary['device_model'] = value_after('MODEL')
    summary['target_is_emulator'] = summary['device_model'] == 'emulator'
    if summary['device_type'] != '2in1':
        raise RuntimeError('Expected the selected PC/2in1 test target')

    code, output = remote('mktemp -d /data/local/tmp/olex2-harmonypc.XXXXXX', 'create-directory.log')
    candidates = [s.strip() for s in output.splitlines()
                  if re.fullmatch(r'/data/local/tmp/olex2-harmonypc\.[A-Za-z0-9]{6,}', s.strip())]
    if code != 0 or len(candidates) != 1:
        raise RuntimeError('Could not create a dedicated test directory')
    remote_dir = candidates[0]
    summary['remote_directory'] = remote_dir
    summary['artifacts'] = []
    for file in files:
        destination = remote_dir + '/' + file.name
        uploaded.append(destination)
        result = subprocess.run([str(args.hdc), '-t', args.target, 'file', 'send',
                                 str(file), destination], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=60)
        (report_dir / ('transfer-' + file.name + '.log')).write_text(result.stdout)
        if result.returncode != 0:
            raise RuntimeError('File transfer failed: ' + file.name)
        digest = hashlib.sha256(file.read_bytes()).hexdigest()
        code, output = remote('sha256sum ' + shlex.quote(destination),
                              'sha256-' + file.name + '.log')
        if code != 0 or not any(line.startswith(digest + ' ') for line in output.splitlines()):
            raise RuntimeError('Transferred file digest mismatch: ' + file.name)
        summary['artifacts'].append({'name': file.name, 'sha256': digest})
    license_fixture=ROOT/'upstream/LICENCE.txt';license_destination=remote_dir+'/ORIGINAL-LICENCE.txt'
    uploaded.append(license_destination)
    sent=subprocess.run([str(args.hdc),'-t',args.target,'file','send',str(license_fixture),license_destination],capture_output=True,text=True,timeout=60)
    (report_dir/'transfer-runtime-licence.log').write_text(sent.stdout+sent.stderr)
    if sent.returncode:raise RuntimeError('Original runtime licence fixture transfer failed')
    license_digest=hashlib.sha256(license_fixture.read_bytes()).hexdigest()
    code,output=remote('sha256sum '+shlex.quote(license_destination),'sha256-runtime-licence.log')
    if code!=0 or not any(line.startswith(license_digest+' ') for line in output.splitlines()):raise RuntimeError('Original licence fixture digest mismatch')
    summary['application_runtime_fixture']={'source':'upstream/LICENCE.txt','sha256':license_digest,'scope':'exact original licence only; synthetic private settings are created by the test'}
    executables = [remote_dir + '/' + file.name for file in files if file.name.startswith('olex2_')]
    code, _ = remote('chmod 700 ' + ' '.join(map(shlex.quote, executables)), 'executable-mode.log')
    if code != 0:
        raise RuntimeError('Cannot set executable mode on the uploaded test programs')
    prefix = 'cd ' + shlex.quote(remote_dir) + ' && LD_LIBRARY_PATH=' + shlex.quote(remote_dir) + ' '
    for index in range(1, args.repeats + 1):
        code, output = remote(prefix + './olex2_sdl_smoke ' + shlex.quote(remote_dir),
                              f'utility-run-{index}.log')
        assertions = [line for line in output.splitlines()
                      if line.startswith('PASS ') and 'baseline only' not in line]
        passed = code == 0 and len(assertions) == 10 and 'FAIL ' not in output
        summary['utility_runs'].append({'iteration': index, 'exit_code': code,
                                        'assertions_passed': len(assertions), 'passed': passed})
        print(f'Utility run {index}: exit={code}, assertions={len(assertions)}, passed={passed}', flush=True)
        if not passed:
            raise RuntimeError(f'Utility run {index} failed; see its saved output')
    mesh_code, mesh_output = remote(prefix + './olex2_mesh_smoke', 'upstream-mesh-run.log')
    mesh_assertions = [line for line in mesh_output.splitlines()
                       if line.startswith('PASS ') and 'sphere adapter and shared triangle submission only' not in line]
    mesh_passed = mesh_code == 0 and len(mesh_assertions) == 38 and 'FAIL ' not in mesh_output
    summary['upstream_mesh_adapter'] = {'passed': mesh_passed, 'exit_code': mesh_code,
                                      'assertions_passed': len(mesh_assertions), 'original_renderer_tested': False}
    print(f'Upstream mesh adapter: exit={mesh_code}, assertions={len(mesh_assertions)}, passed={mesh_passed}', flush=True)
    if not mesh_passed:
        raise RuntimeError('Upstream mesh adapter checks failed; see upstream-mesh-run.log')
    submission_code, submission_output = remote(prefix + './olex2_submission_smoke', 'shared-submission-run.log')
    submission_assertions = [line for line in submission_output.splitlines()
                             if line.startswith('PASS ') and 'shared GlSphereEx Render/RenderEx;' not in line]
    submission_passed = submission_code == 0 and len(submission_assertions) == 13 and 'FAIL ' not in submission_output
    summary['shared_sphere_submission'] = {'passed': submission_passed, 'exit_code': submission_code,
        'assertions_passed': len(submission_assertions), 'scope': 'synthetic Render and inclusive-mask RenderEx stream; no GL context',
        'original_renderer_tested': False}
    print(f'Shared submission: exit={submission_code}, assertions={len(submission_assertions)}, passed={submission_passed}', flush=True)
    if not submission_passed:
        raise RuntimeError('Shared sphere submission checks failed; see shared-submission-run.log')
    primitive_code, primitive_output = remote(prefix + './olex2_primitive_smoke', 'upstream-primitives-run.log')
    primitive_assertions = [line for line in primitive_output.splitlines()
                            if line.startswith('PASS ') and 'upstream helper primitive adapter only;' not in line]
    primitive_passed = primitive_code == 0 and len(primitive_assertions) == 122 and 'FAIL ' not in primitive_output
    summary['upstream_helper_primitives'] = {'passed': primitive_passed, 'exit_code': primitive_code,
        'assertions_passed': len(primitive_assertions), 'scope': 'synthetic torus/disks/rims, known topology conversion and explicit legacy tail behavior',
        'original_renderer_tested': False}
    print(f'Upstream helper primitives: exit={primitive_code}, assertions={len(primitive_assertions)}, passed={primitive_passed}', flush=True)
    if not primitive_passed:
        raise RuntimeError('Helper primitive adapter checks failed; see upstream-primitives-run.log')
    color_code, color_output = remote(prefix + './olex2_color_smoke', 'upstream-color-run.log')
    color_assertions = [line for line in color_output.splitlines()
                        if line.startswith('PASS ') and 'upstream color value adapter only;' not in line]
    color_passed = color_code == 0 and len(color_assertions) == 139 and 'FAIL ' not in color_output
    summary['upstream_color_values'] = {'passed': color_passed, 'exit_code': color_code,
        'assertions_passed': len(color_assertions), 'scope': 'original color parser/packing, opaque adapter and synthetic color matching only',
        'original_renderer_tested': False}
    print(f'Upstream color values: exit={color_code}, assertions={len(color_assertions)}, passed={color_passed}', flush=True)
    if not color_passed:
        raise RuntimeError('Upstream color value checks failed; see upstream-color-run.log')
    material_code, material_output = remote(prefix + './olex2_material_smoke', 'upstream-material-run.log')
    material_assertions = [line for line in material_output.splitlines()
                          if line.startswith('PASS ') and 'upstream material value adapter only;' not in line]
    material_passed = material_code == 0 and len(material_assertions) == 196 and 'FAIL ' not in material_output
    summary['upstream_material_values'] = {'passed': material_passed, 'exit_code': material_code,
        'assertions_passed': len(material_assertions), 'scope': 'original material value methods and symmetric opaque adapter; legacy Init excluded',
        'original_renderer_tested': False}
    print(f'Upstream material values: exit={material_code}, assertions={len(material_assertions)}, passed={material_passed}', flush=True)
    if not material_passed:
        raise RuntimeError('Upstream material value checks failed; see upstream-material-run.log')
    commands_code, commands_output = remote(prefix + './olex2_material_commands_smoke', 'material-commands-run.log')
    match = re.search(r'PASS upstream material command flow; (\d+) assertions; (\d+) flag-state cases;', commands_output)
    commands_passed = commands_code == 0 and match is not None and match.groups() == ('524360', '524288') and 'FAIL ' not in commands_output
    summary['upstream_material_commands'] = {'passed': commands_passed, 'exit_code': commands_code,
        'assertions_passed': int(match[1]) if match else 0, 'flag_state_cases_passed': int(match[2]) if match else 0,
        'scope': 'exact original Init command control flow, independent exhaustive flag-state oracle and bounded opaque command consumer; not complete fixed-function rendering',
        'original_renderer_tested': False}
    print(f'Upstream material commands: exit={commands_code}, passed={commands_passed}', flush=True)
    if not commands_passed:
        raise RuntimeError('Material command-flow checks failed; see material-commands-run.log')
    lit_code, lit_output = remote(prefix + './olex2_lit_material_smoke', 'lit-material-run.log')
    lit_match = re.search(r'PASS lit material and independent row reference; (\d+) assertions;', lit_output)
    lit_passed = lit_code == 0 and lit_match is not None and int(lit_match[1]) == 137 and 'FAIL ' not in lit_output
    summary['upstream_lit_material'] = {'passed': lit_passed, 'exit_code': lit_code,
        'assertions_passed': int(lit_match[1]) if lit_match else 0,
        'scope': 'original specular/shininess commands, bounded symmetric opaque consumer and independent submitted-triangle lighting reference; not full fixed-function rendering',
        'original_renderer_tested': False}
    print(f'Upstream lit material: exit={lit_code}, passed={lit_passed}', flush=True)
    if not lit_passed: raise RuntimeError('Lit material/reference checks failed; see lit-material-run.log')
    triangle_code, triangle_output = remote(prefix + './olex2_original_triangles_smoke', 'original-triangles-run.log')
    triangle_match = re.search(r'PASS original primitive triangle commands; (\d+) assertions; (\d+) branch-state cases;', triangle_output)
    triangle_passed = triangle_code == 0 and triangle_match is not None and triangle_match.groups() == ('31805','31752') and 'FAIL ' not in triangle_output
    summary['original_triangle_commands'] = {'passed': triangle_passed, 'exit_code': triangle_code,
        'assertions_passed': int(triangle_match[1]) if triangle_match else 0,
        'branch_state_cases_passed': int(triangle_match[2]) if triangle_match else 0,
        'scope': 'exact original TGlPrimitive triangle branch, inherited attributes and grouped normals; colors/selection are CPU command traces, target GLES consumer explicitly uncolored',
        'original_renderer_tested': False}
    print(f'Original primitive triangle commands: exit={triangle_code}, passed={triangle_passed}', flush=True)
    if not triangle_passed: raise RuntimeError('Original triangle command checks failed; see original-triangles-run.log')
    line_code,line_output=remote(prefix+'./olex2_original_lines_smoke','original-lines-run.log')
    line_passed=line_code==0 and line_command_log_matches(line_output)
    summary['original_line_commands']=dict(passed=line_passed,exit_code=line_code,assertions_passed=104199 if line_passed else 0,
        branch_state_cases_passed=640 if line_passed else 0,source_edge_meshes_passed=24 if line_passed else 0,
        independent_index_contracts_verified=line_command_log_matches(line_output),scope='original sgloLines color/ForcePlain/selection and width traces; 24 exact source-edge streams; CPU only, not GPU width/color/normal equivalence',gpu_wide_lines_tested=False,original_renderer_tested=False)
    print(f'Original line commands: exit={line_code}, passed={line_passed}',flush=True)
    if not line_passed:raise RuntimeError('Original line command checks failed; see original-lines-run.log')
    path_code,path_output=remote(prefix+'./olex2_original_line_paths_smoke','original-line-paths-run.log')
    path_match=re.search(r'PASS original line strip/loop paths; (\d+) assertions; (\d+) raw branch-state cases; (\d+) bounded branch-state cases; 2 independent edge topologies;',path_output)
    path_passed=path_code==0 and path_match is not None and path_match.groups()==('181512','2688','1408') and 'FAIL ' not in path_output
    summary['original_line_path_commands']=dict(passed=path_passed,exit_code=path_code,assertions_passed=181512 if path_passed else 0,raw_branch_state_cases_passed=2688 if path_passed else 0,bounded_branch_state_cases_passed=1408 if path_passed else 0,independent_edge_topologies_passed=2 if path_passed else 0,scope='original sgloLineStrip/Loop CPU bodies, explicit target boundary and independent consecutive/closed source edge topology; raw selection/color differences retained',gpu_execution_tested=False,gpu_vertex_color_tracking_tested=False,gpu_wide_lines_tested=False,full_renderer_tested=False)
    print(f'Original strip/loop paths: exit={path_code}, passed={path_passed}',flush=True)
    if not path_passed:raise RuntimeError('Original line strip/loop checks failed; see original-line-paths-run.log')

    matrix_code, matrix_output = remote(prefix + './olex2_original_matrices_smoke', 'original-matrices-run.log')
    matrix_match = re.search(r'PASS original basis matrix state; (\d+) assertions; (\d+) independent camera poses;', matrix_output)
    matrix_passed = matrix_code == 0 and matrix_match is not None and matrix_match.groups() == ('353','315') and 'FAIL ' not in matrix_output
    summary['original_basis_matrices'] = {'passed': matrix_passed, 'exit_code': matrix_code,
        'assertions_passed': int(matrix_match[1]) if matrix_match else 0,
        'independent_camera_poses_passed': int(matrix_match[2]) if matrix_match else 0,
        'scope': 'owned original TEBasis exports, bounded explicit matrix state and analytic camera/normal transforms; general stacks/scales/shears are CPU evidence only',
        'original_renderer_tested': False, 'full_gl_matrix_state_implemented': False}
    print(f'Original basis matrices: exit={matrix_code}, passed={matrix_passed}', flush=True)
    if not matrix_passed: raise RuntimeError('Original matrix checks failed; see original-matrices-run.log')
    polygon_code,polygon_output=remote(prefix+'./olex2_original_polygons_smoke','original-polygons-run.log')
    polygon_match=re.search(r'PASS original polygon and simple-planar tessellation; (\d+) assertions; (\d+) branch-state cases; (\d+) independent topology poses;',polygon_output)
    polygon_passed=polygon_code==0 and polygon_match is not None and polygon_match.groups()==('2052','1980','48') and 'FAIL ' not in polygon_output
    summary['original_polygon_commands']={'passed':polygon_passed,'exit_code':polygon_code,'assertions_passed':int(polygon_match[1]) if polygon_match else 0,
        'branch_state_cases_passed':int(polygon_match[2]) if polygon_match else 0,'independent_topology_poses_passed':int(polygon_match[3]) if polygon_match else 0,
        'scope':'original polygon submission and new bounded single-contour simple-planar tessellation; color traces are CPU only',
        'full_glu_implemented':False,'holes_supported':False,'original_renderer_tested':False}
    print(f'Original polygons: exit={polygon_code}, passed={polygon_passed}',flush=True)
    if not polygon_passed:raise RuntimeError('Original polygon checks failed; see original-polygons-run.log')
    interpreter_code,interpreter_output=remote(prefix+'./olex2_desktop_commands_smoke','desktop-commands-run.log')
    interpreter_match=re.search(r'PASS original desktop command interpreter; (\d+) assertions; (\d+) independent view combinations; (\d+) serialized concurrent calls;',interpreter_output)
    interpreter_passed=interpreter_code==0 and interpreter_match is not None and interpreter_match.groups()==('1196','576','64') and 'FAIL ' not in interpreter_output
    summary['original_desktop_commands']={'passed':interpreter_passed,'exit_code':interpreter_code,'assertions_passed':int(interpreter_match[1]) if interpreter_match else 0,
        'independent_view_combinations_passed':int(interpreter_match[2]) if interpreter_match else 0,'serialized_concurrent_calls_passed':int(interpreter_match[3]) if interpreter_match else 0,
        'scope':'original TEMacroLib expression parser, original quote-aware chain tokenizer and TLibrary/AMacro dispatch with explicit pc.* view adapters',
        'complete_application_commands_registered':False,'target_python_integrated':False,'global_processor_thread_safety_claimed':False}
    print(f'Original command interpreter: exit={interpreter_code}, passed={interpreter_passed}',flush=True)
    if not interpreter_passed:raise RuntimeError('Original desktop command interpreter checks failed; see desktop-commands-run.log')
    runtime_code,runtime_output=remote(prefix+'./olex2_application_runtime_smoke '+shlex.quote(remote_dir)+' '+shlex.quote(license_destination),'application-runtime-run.log')
    runtime_match=re.search(r'PASS original application runtime; (\d+) assertions; (\d+) original option round trips; (\d+) mixed serialized interpreter calls;',runtime_output)
    runtime_passed=runtime_code==0 and runtime_match is not None and runtime_match.groups()==('86','32','64') and 'real TXApp empty startup/135 core macros/38 core functions (registered only)' in runtime_output and '8 repeated lifecycles and failed-settings recovery' in runtime_output and 'FAIL ' not in runtime_output
    summary['original_application_runtime']={'passed':runtime_passed,'exit_code':runtime_code,'assertions_passed':int(runtime_match[1]) if runtime_match else 0,'original_option_round_trips_passed':int(runtime_match[2]) if runtime_match else 0,'mixed_serialized_interpreter_calls_passed':int(runtime_match[3]) if runtime_match else 0,'original_core_empty_startup_tested':runtime_passed,'original_core_macro_registry_count':135 if runtime_passed else 0,'original_core_function_registry_count':38 if runtime_passed else 0,'original_core_file_count':1 if runtime_passed else 0,'original_core_commands_executed':False,'repeated_core_lifecycle_cases_passed':8 if runtime_passed else 0,'failed_private_settings_startup_recovery_tested':runtime_passed,'scope':'actual TXApp empty core startup plus TBasicApp private directories, original 22-function registry, original settings parser/serializer and atomic platform SaveOptions hook; not all 22 functions individually tested','full_application_entry_integrated':False,'all_original_application_functions_runtime_tested':False,'process_environment_modified':False,'global_processor_thread_safety_claimed':False,'power_loss_durability_verified':False}
    print(f'Original application runtime: exit={runtime_code}, passed={runtime_passed}',flush=True)
    if not runtime_passed:raise RuntimeError('Original application runtime checks failed; see application-runtime-run.log')
    log_code,log_output=remote(prefix+'./olex2_original_log_smoke '+shlex.quote(remote_dir)+' '+shlex.quote(license_destination),'original-log-run.log')
    log_reference=parse_catalog_log(log_output)
    log_passed=log_code==0 and 'FAIL ' not in log_output
    summary['original_log_bridge']={'passed':log_passed,'exit_code':log_code,'assertions_passed':NATIVE_ASSERTIONS,'original_severity_cases_passed':5,'real_core_catalog_macros_tested':2,'locally_serialized_caller_posts_passed':400,
        'catalogs':{kind:{key:value for key,value in item.items() if key!='text'} for kind,item in log_reference.items()},
        'scope':'actual original TLog OnPost observer and LstMac/LstFun metadata output; original-library caller work locally serialized',
        'full_core_registry_exposed':False,'scientific_command_behavior_tested':False,'global_legacy_thread_safety_claimed':False,'durable_log_storage_tested':False,'oom_injection_tested':False}
    print(f'Original log bridge: exit={log_code}, passed={log_passed}',flush=True)
    if not log_passed:raise RuntimeError('Original log bridge checks failed; see original-log-run.log')
    transfer_code,transfer_output=remote(prefix+'./olex2_options_transfer_smoke '+shlex.quote(remote_dir)+' '+shlex.quote(license_destination),'options-transfer-run.log')
    transfer_match=re.search(r'PASS original options document transfer; (\d+) assertions;',transfer_output)
    transfer_passed=transfer_code==0 and transfer_match is not None and transfer_match[1]=='23' and 'FAIL ' not in transfer_output
    summary['original_options_document_transfer']={'passed':transfer_passed,'exit_code':transfer_code,'assertions_passed':int(transfer_match[1]) if transfer_match else 0,'scope':'original-format read-only snapshot and validated private option merge; Unicode, save failure preservation and restart; no system-picker qualification','system_picker_tested':False,'complete_application_documents_supported':False}
    print(f'Original options document transfer: exit={transfer_code}, passed={transfer_passed}',flush=True)
    if not transfer_passed:raise RuntimeError('Original options document transfer checks failed; see options-transfer-run.log')
    desktop_code, _ = remote(prefix + './olex2_gl_core_probe', 'core-gl-load.log')
    summary['core_gl_egl_symbol_load'] = {
        'exit_code': desktop_code, 'passed': desktop_code == 0,
        'graphics_context_created': False, 'rendering_tested': False}
    es_symbols, _ = remote(prefix + './olex2_gles_egl_smoke --symbols-only', 'gles-symbol-load.log')
    summary['gles3_egl_symbol_load'] = {'exit_code': es_symbols, 'passed': es_symbols == 0}
    if es_symbols == 0:
        render_code, output = remote(prefix + './olex2_gles_egl_smoke', 'gles-offscreen.log')
        summary['gles3_offscreen_diagnostic'] = {
            'exit_code': render_code,
            'passed': render_code == 0 and 'PASS green shader draw/readback' in output,
            'scope': 'standalone 8x8 pbuffer / synthetic color-and-triangle diagnostic; not Olex2 rendering',
            'output': output.strip()}
    summary['utility_baseline_passed'] = all(x['passed'] for x in summary['utility_runs'])
    offscreen_passed = summary.get('gles3_offscreen_diagnostic', {}).get('passed', False)
    if desktop_code == 0 and es_symbols == 0 and offscreen_passed:
        summary['result'] = 'passed: utility baseline and standalone graphics diagnostics only'
    else:
        summary['result'] = 'partial: utility, mesh, shared submission, helper primitives, color/material values and portable/lit material plus original triangle commands and basis matrices plus polygons and original desktop command interpreter plus original application runtime, options document transfer and original line commands passed; standalone graphics diagnostics still fail'

except Exception as error:
    summary['result'] = 'failed'
    summary['error'] = f'{type(error).__name__}: {error}'
    print(summary['error'], flush=True)
finally:
    if remote_dir and uploaded:
        try:
            # Remove only exact files uploaded by this run; never recursively delete.
            cmd = 'rm -f ' + ' '.join(map(shlex.quote, uploaded)) + ' && rmdir ' + shlex.quote(remote_dir)
            code, _ = remote(cmd, 'cleanup.log')
            summary['cleanup_completed'] = code == 0
        except Exception as error:
            summary['cleanup_completed'] = False
            summary['cleanup_error'] = str(error)
    summary['finished_at'] = datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds')
    encoded = json.dumps(summary, ensure_ascii=False, indent=2) + '\n'
    (report_dir / 'result.json').write_text(encoded)
    (ROOT / 'reports/emulator/last-run.json').write_text(encoded)
    print(encoded)
raise SystemExit(0 if summary['result'].startswith('passed:') else 1)
