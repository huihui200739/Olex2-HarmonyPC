#!/usr/bin/env python3
"""Extend exact25 finite font proof with its own raw212 UI/7PID/12-start evidence.

This never changes the frozen font-only recorder or claims full adaptation.
"""
import argparse
import copy
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import unittest
import uuid
from zoneinfo import ZoneInfo

from build_provenance_png import verify
from font_raster_flush_application_validation import validate_application
from font_raster_flush_current_qualification import validate_current_qualifications
from font_raster_flush_progress_validation import revalidate_window
from font_raster_flush_window_validation import IDENTITY
from native_font_progress_validation import ROOT, bound, canonical, check_bound, require
from renderer_probe_identity import package_identity

CURRENT_NAMES = ('gx-resource-font-status', 'gx-vector-font-status', 'gx-font-status',
                 'gx-application-platform-status', 'gx-persistent-runtime-status',
                 'gx-texture-load-status', 'gx-texture-load-route-status')


def validate_previous(previous, package, source, verification):
    require(previous['status'] == 'in_progress' and previous['passed'] is False
            and previous['milestone_complete'] is False
            and previous['full_port_complete'] is False
            and previous['functional_completion_percent'] is None,
            'Finite proof cannot claim overall completion')
    candidate = previous['current_build_candidate']
    require(candidate['identity'] == IDENTITY and candidate['hap'] == package
            and candidate['source'] == str(source.relative_to(ROOT))
            and candidate['verification'] == canonical(verification),
            'Previous exact25 source/HAP/build proof differs')
    require(previous['font_window']['actual_rounds'] == 3
            and previous['font_window']['independent_checks'] == 445
            and previous['font_window']['complete_font_support'] is False
            and previous['font_window']['full_wx_runtime_verified'] is False,
            'Previous finite font scope differs')


def validate_replay(report, tools):
    value = json.loads(report.read_text())
    require(value['passed'] is True and value['all_original_bytes_equal'] is True
            and (value['recipe_files'], value['metadata_files'], value['total_files']) == (65, 3, 68)
            and value['full_port_complete'] is False,
            'Exact local65+3 tool replay is required, not a runtime qualification')
    manifest = json.loads((tools / 'tool-profile.json').read_text())
    expected = set(manifest['files']) | {'tool-profile.json', 'host-catalog-reference.json',
                                       'reports/app/persistent-gx-host-catalog.log'}
    require(len(value['files']) == 68 and {r['path'] for r in value['files']} == expected,
            'Exact tool replay file inventory differs')
    replay_root = ROOT / 'out/gx-application-evaluation/font-raster-flush-ui-tools25-replay-attempt1'
    evidence = [bound(report)]
    for row in value['files']:
        first, second = (tools / row['path']).resolve(), (replay_root / row['path']).resolve()
        require(first.is_relative_to(tools) and second.is_relative_to(replay_root),
                'Tool replay artifact escaped its directory')
        data = first.read_bytes()
        require(data == second.read_bytes() and len(data) == row['bytes']
                and hashlib.sha256(data).hexdigest() == row['sha256'],
                'Exact replay bytes changed: ' + row['path'])
        evidence += [bound(first), bound(second)]
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'hap', 'font-report', 'app-report', 'ui-tools',
                 'platform-report', 'startup-report', 'python-log', 'ui-replay-report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source, hap, tools = args.source.resolve(), args.hap.resolve(), args.ui_tools.resolve()
    require(source.is_relative_to(ROOT) and tools.is_relative_to(ROOT / 'out'),
            'Use only this project frozen source and isolated tools')
    require(package_identity(hap) == IDENTITY, 'Only exact25 is accepted')
    verification = verify(hap, hap.with_suffix('.provenance.json'), source)
    package = bound(hap)
    paths = [ROOT / 'reports' / (name + '.json') for name in CURRENT_NAMES]
    original = paths[0].read_bytes()
    require(all(path.read_bytes() == original for path in paths), 'All seven current records must agree')
    previous = json.loads(original)
    validate_previous(previous, package, source, verification)
    for evidence in previous['evidence']:
        check_bound(evidence)
    font = canonical(revalidate_window(args.font_report, package['sha256'], source))
    require(font == json.loads(check_bound(previous['font_window']['report']).read_text()),
            'Re-read actual font evidence differs from original finite qualification')
    ui = canonical(validate_application(json.loads(args.app_report.read_text()), tools, IDENTITY))
    qualification = validate_current_qualifications(args.app_report, ui, args.platform_report,
                                                   args.startup_report, package['sha256'])
    replay_evidence = validate_replay(args.ui_replay_report, tools)
    log = args.python_log.read_text()
    match = re.search(r'Ran (\d+) tests in ', log)
    suite = unittest.defaultTestLoader.discover(str(ROOT / 'scripts/tests'), pattern='test_*.py')
    require(match and log.rstrip().endswith('OK') and int(match[1]) == suite.countTestCases(),
            'Full current Python suite must pass with the exact current test count')
    now = datetime.now(ZoneInfo('Asia/Shanghai'))
    directory = ROOT / 'reports/emulator' / ('font-raster-flush-ui-revalidation-' +
                now.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    directory.mkdir()
    record = copy.deepcopy(previous)
    archives = {}
    for path in paths:
        saved = directory / ('before-' + path.name)
        saved.write_bytes(original)
        archives[path.stem] = bound(saved)
    record['history']['qualified_candidate25_before_own_ui_resources_startups'] = archives
    independent = directory / 'independent-current-qualification.json'
    independent.write_text(json.dumps(qualification, ensure_ascii=False, indent=2) + '\n')
    record['current_ui'] = dict(status='passed-limited-212-check-diagnostic-ui',
        report=bound(args.app_report), qualification=ui, inherited_prior_candidate_pass=False)
    for name in ('platform_image_audit', 'startup_audit'):
        record[name] = qualification[name]
    record['target']['last_observed_startup_pid'] = qualification['last_startup_pid']
    record['current_build_candidate']['promotion'] = (
        'Exact25 limited font windows, own212 UI, all7 UI PID resources and12 initialized starts; '
        'mixed routes and full adaptation not yet qualified')
    record['complete_process_fd_metadata_verified'] = (
        qualification['platform_image_audit']['complete_descriptor_metadata_scan_passed'] and
        qualification['startup_audit']['complete_descriptor_metadata_scan_passed'])
    record['tests'].update(complete_current_python_suite_verified=True,
        python_cases=int(match[1]), python_log=bound(args.python_log),
        own_ui_tools_replay=bound(args.ui_replay_report))
    require(record['remaining'][0].startswith('25自己的212'), 'Unexpected finite qualification remaining scope')
    record['remaining'][0] = '25自身三路混合尚未资格验证；每字体动作15请求、每PID最终45，不能继承21'
    record['evidence'] += [bound(args.font_report), bound(args.app_report), bound(independent),
                           bound(args.python_log)] + list(archives.values()) + replay_evidence + qualification['evidence']
    for name in ('record-font-raster-flush-ui-progress.py', 'font_raster_flush_current_qualification.py',
                 'font_raster_flush_application_validation.py', 'font_raster_flush_tool_profiles.py',
                 'font_raster_flush_startup_validation.py', 'collect-gx-font-raster-flush-evaluation.py',
                 'run-font-raster-flush-startup-audit.py'):
        saved = directory / name
        saved.write_bytes((ROOT / 'scripts' / name).read_bytes())
        record['evidence'].append(bound(saved))
    record['updated_at'] = now.isoformat(timespec='seconds')
    data = json.dumps(record, ensure_ascii=False, indent=2) + '\n'
    for path in [directory / 'result.json'] + paths:
        path.write_text(data)
    print(json.dumps(dict(report_directory=str(directory), status='in_progress', own_ui_checks=212,
        actual_ui_pids=7, actual_starts=12, full_port_complete=False,
        functional_completion_percent=None), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
