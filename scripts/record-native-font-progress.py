#!/usr/bin/env python3
"""Record exact19 font qualification without transferring candidate16 UI/startup passes."""
import argparse
import json
import re
import unittest
import subprocess
import uuid
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from build_provenance_png import verify
from renderer_probe_identity import package_identity
from native_font_application_validation import validate_application
from native_font_progress_validation import (
    ROOT, IDENTITY, bound, check_bound, revalidate_window,
    validate_disassembly, canonical, require)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'hap', 'font-report', 'raster-proof'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--app-report', type=Path)
    parser.add_argument('--ui-tools', type=Path)
    parser.add_argument('--python-log', type=Path)
    parser.add_argument('--font-host-log', type=Path)
    parser.add_argument('--standard-host-log', type=Path)
    parser.add_argument('--platform-report', type=Path)
    parser.add_argument('--startup-report', type=Path)
    parser.add_argument('--mixed-report', type=Path)
    args = parser.parse_args()
    source, hap = args.source.resolve(), args.hap.resolve()
    require(package_identity(hap) == IDENTITY, 'Only exact19 package accepted')
    proof = verify(hap, hap.with_suffix('.provenance.json'), source)
    hap_bound = bound(hap)
    font = revalidate_window(args.font_report, hap_bound['sha256'])
    binary = json.loads(args.raster_proof.read_text())
    require(binary['identity'] == '0.22.19' and binary['hap'] == hap_bound,
            'Binary proof HAP/version differs')
    for name in ('hap', 'provenance', 'unstripped_source_build_output', 'disassembly'):
        check_bound(binary[name])
    require(binary['provenance'] == bound(hap.with_suffix('.provenance.json')),
            'Binary proof provenance differs')
    library = check_bound(binary['unstripped_source_build_output'])
    require(library.is_relative_to(hap.parent / 'stage') and library.name == 'libolex2core_graphics.so',
            'Binary proof must use this build output, not an unrelated library')
    require(binary['function'] == 'gl4es_glRasterPos3f' and
            binary['actual_float32_viewport_fmla'] is True, 'Unexpected raster model')
    command = ['/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-objdump',
               '--disassemble', '--start-address=' + str(binary['function_start']),
               '--stop-address=' + str(binary['function_end']), str(library)]
    fresh = subprocess.run(command, capture_output=True, text=True, check=True).stdout
    disassembly = validate_disassembly(check_bound(binary['disassembly']).read_text(), fresh,
                                       binary['function_start'], binary['function_end'])
    ui = dict(status='not-yet-qualified-for-candidate19', inherited_candidate16_pass=False)
    require(bool(args.app_report) == bool(args.ui_tools), 'Supply both UI report and tools')
    if args.app_report:
        actual = canonical(validate_application(json.loads(args.app_report.read_text()), args.ui_tools, IDENTITY))
        ui = dict(status='passed-limited-212-check-diagnostic-ui',
                  report=bound(args.app_report), qualification=actual,
                  inherited_candidate16_pass=False)
    tests = dict(host_models_packaged=False, model_tests_are_not_GPU_qualification=True)
    if args.python_log:
        log = args.python_log.read_text()
        matched = re.search(r'Ran (\d+) tests in ', log)
        suite = unittest.defaultTestLoader.discover(str(ROOT / 'scripts/tests'), pattern='test_*.py')
        require(matched and log.rstrip().endswith('OK') and int(matched[1]) == suite.countTestCases(),
                'Full Python log does not cover the current complete test suite')
        tests['python_cases'] = int(matched[1])
        tests['python_log'] = bound(args.python_log)
    for path, key, count in ((args.font_host_log, 'font_cell_route_host', 2),
                             (args.standard_host_log, 'unchanged_standard_host', 18)):
        if path:
            require('100% tests passed, 0 tests failed out of ' + str(count) in path.read_text(),
                    'Expected complete host CTest log: ' + key)
            tests[key] = dict(ctest_cases=count, log=bound(path), GPU_executed=False)
    require(bool(args.platform_report) == bool(args.startup_report),
            'Supply both platform and startup reports')
    qualification = None
    if args.platform_report:
        require(args.app_report is not None, 'Full resource/startup audit requires current full UI')
        from native_font_current_qualification import validate_current_qualifications
        qualification = validate_current_qualifications(args.app_report, ui['qualification'],
                            args.platform_report, args.startup_report, hap_bound['sha256'])
    mixed = None
    if args.mixed_report:
        from native_font_mixed_progress_validation import validate_mixed_report
        mixed = validate_mixed_report(args.mixed_report, hap_bound['sha256'])
    now = datetime.now(ZoneInfo('Asia/Shanghai'))
    destination = ROOT / 'reports/emulator' / ('native-font-revalidation-' +
                  now.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    destination.mkdir()
    (destination / 'actual-raster-disassembly.log').write_text(fresh)
    current = ROOT / 'reports/gx-application-platform-status.json'
    prior = json.loads(current.read_text())
    history = dict(prior.get('history', {}))
    archives = {}
    for name in ('gx-application-platform-status', 'gx-persistent-runtime-status',
                 'gx-texture-load-status', 'gx-texture-load-route-status'):
        path = ROOT / 'reports' / (name + '.json')
        archive = destination / ('before-' + name + '.json')
        archive.write_bytes(path.read_bytes())
        archives[name] = bound(archive)
    if prior['current_build_candidate']['identity']['versionName'] == '0.22.16':
        history['qualified_candidate16_before_font'] = archives
    else:
        require('qualified_candidate16_before_font' in history,
                'Retain the real candidate16 qualification')
    history['before_this_font_record'] = archives
    font_result = destination / 'font-revalidation.json'
    font_result.write_text(json.dumps(font, ensure_ascii=False, indent=2) + '\n')
    record = dict(kind='original-font-current-candidate-limited-qualification', status='in_progress',
                  passed=False, milestone_complete=False, full_port_complete=False,
                  functional_completion_percent=None, physical_pc_verified=False,
                  installed_target_code_directly_hashed=False,
                  production_baseline=prior['production_baseline'],
                  current_build_candidate=dict(identity=IDENTITY, source=source.relative_to(ROOT).as_posix(),
                      hap=hap_bound, verification=proof, actual_compiled_native_sources=proof['compiled_native_sources'],
                      upstream_cpp_compiled=379, patches=20, explicit_core_exports=367,
                      promotion='exact19 font window qualified; each other current-version gate qualified separately'),
                  target=dict(target=font['target'], same_existing_emulator='MateBook Pro',
                      device_identity=font['device_identity'], last_verified_install_version='0.22.19',
                      font_evidence_pid=font['pid'], data_reset=False, agreement_auto_accepted=False),
                  current_ui=ui,
                  platform_image_audit=dict(status='not-yet-qualified-for-candidate19', inherited_candidate16_pass=False),
                  startup_audit=dict(status='not-yet-qualified-for-candidate19', inherited_candidate16_pass=False),
                  mixed_routes=dict(status='font/drawing/load-interleaving-not-yet-tested-for-candidate19',
                                    inherited_candidate16_pass=False),
                  font_window=dict(status='passed-limited-native-ASCII-window', report=bound(font_result),
                      actual_rounds=3, independent_checks=font['checks'], actual_context_continuations=2,
                      actual_scale_points=[18, 27, 18], original_256_glyph_lists=True,
                      text='Ab09', complete_font_support=False,
                      independent_raster_binary=disassembly),
                  history=history, tests=tests,
                  remaining=['完整字体描述/资源/字体选择/DPI/字符与样式',
                             '19自身全部UI PID平台资源和固定12启动，字体/原绘图/加载交替',
                             '完整原UI/菜单/命令/文档/资源/Python/插件及P1～P8',
                             '真机/物理输入/正式签名与发行'],
                  evidence=[bound(args.font_report), bound(args.raster_proof), bound(font_result),
                            bound(destination / 'actual-raster-disassembly.log')],
                  updated_at=now.isoformat(timespec='seconds'))
    if qualification:
        record['platform_image_audit'] = qualification['platform_image_audit']
        record['startup_audit'] = qualification['startup_audit']
        record['target']['last_verified_startup_pid'] = qualification['last_startup_pid']
        record['evidence'] += qualification['evidence']
        record['remaining'][1] = '字体/原绘图/加载交替与字体广泛字符/样式/异常恢复'
    if mixed:
        mixed_result = destination / 'mixed-revalidation.json'
        mixed_result.write_text(json.dumps(mixed, ensure_ascii=False, indent=2) + '\n')
        record['mixed_routes'] = dict(status='passed-limited-three-route-window-interleaving',
            report=bound(mixed_result), original_report=mixed['raw_report'], actual_actions=18,
            pids=mixed['pids'], checks=mixed['counters'],
            direct_load_font_counter_available=False,
            exact_cumulative_font_requests_per_pid=9,
            inherited_candidate16_pass=False)
        record['evidence'] += [bound(mixed_result)] + mixed['evidence']
        record['remaining'][1] = '字体广泛字符/样式/异常恢复与完整纹理格式/mipmap/异常恢复'
    if args.app_report:
        record['evidence'] += [bound(args.app_report), bound(args.ui_tools / 'tool-profile.json')]
    for log in (args.python_log, args.font_host_log, args.standard_host_log):
        if log:
            record['evidence'].append(bound(log))
    for name in ('record-native-font-progress.py', 'native_font_progress_validation.py',
                 'native_font_application_validation.py', 'native_font_raster_window_validation.py',
                 'native_font_window_validation.py', 'native_font_current_qualification.py',
                 'native_font_tool_profiles.py', 'native_font_startup_validation.py',
                 'native_font_mixed_progress_validation.py', 'native_font_mixed_validation.py'):
        path = ROOT / 'scripts' / name
        (destination / name).write_bytes(path.read_bytes())
        record['evidence'].append(bound(destination / name))
    for path in [destination / 'result.json', ROOT / 'reports/gx-font-status.json'] + [
            ROOT / 'reports' / (name + '.json') for name in archives]:
        path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(dict(status=record['status'], version='0.22.19', font_rounds=3,
                         checks=font['checks'], current_ui=ui['status'],
                         report_directory=str(destination), full_port_complete=False), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
