#!/usr/bin/env python3
"""Revalidate exact25 mixed routes and its own font/UI/7PID/12-start evidence.

No device operations, inherited candidate passes, or full-port completion claims.
"""
import argparse
import copy
from datetime import datetime
import json
from pathlib import Path
import re
import unittest
import uuid
from zoneinfo import ZoneInfo

from build_provenance_png import verify
from font_raster_flush_application_validation import validate_application
from font_raster_flush_current_qualification import validate_current_qualifications
from font_raster_flush_mixed_progress_validation import validate_mixed_report
from font_raster_flush_progress_validation import revalidate_window
from font_raster_flush_window_validation import IDENTITY
from native_font_progress_validation import ROOT, bound, canonical, check_bound, require
from renderer_probe_identity import package_identity

CURRENT_NAMES = ('gx-resource-font-status', 'gx-vector-font-status', 'gx-font-status',
                 'gx-application-platform-status', 'gx-persistent-runtime-status',
                 'gx-texture-load-status', 'gx-texture-load-route-status')


def validate_current_base(previous, package, source, verification, font, ui, current):
    require(previous['status'] == 'in_progress' and previous['passed'] is False
            and previous['milestone_complete'] is False and previous['full_port_complete'] is False
            and previous['functional_completion_percent'] is None,
            'Partial proof cannot claim overall completion')
    candidate = previous['current_build_candidate']
    require(candidate['identity'] == IDENTITY and candidate['hap'] == package
            and candidate['source'] == str(source.relative_to(ROOT))
            and candidate['verification'] == canonical(verification),
            'Exact25 source/HAP/build proof differs')
    require(font['passed'] is True and font['checks'] == 445
            and previous['font_window']['independent_checks'] == 445
            and previous['font_window']['complete_font_support'] is False,
            'Exact finite font proof differs')
    require(ui['passed'] is True and ui['total_ui_checks'] == 212
            and previous['current_ui']['status'] == 'passed-limited-212-check-diagnostic-ui'
            and previous['current_ui']['qualification'] == ui,
            'Own raw212 UI qualification must match exactly')
    require(current['passed'] is True and
            len(current['platform_image_audit']['pids']) == 7
            and current['startup_audit']['actual_starts'] == 12
            and previous['platform_image_audit'] == current['platform_image_audit']
            and previous['startup_audit'] == current['startup_audit'],
            'Own raw all7PID/12-start qualification differs')
    require(previous['mixed_routes']['status'] == 'not-yet-qualified-for-candidate25',
            'Only the unqualified current mixed milestone may be extended')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'hap', 'font-report', 'app-report', 'ui-tools',
                 'platform-report', 'startup-report', 'mixed-report', 'python-log'):
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
    require(all(p.read_bytes() == original for p in paths), 'All seven current records must agree')
    previous = json.loads(original)
    for evidence in previous['evidence']:
        check_bound(evidence)
    font = canonical(revalidate_window(args.font_report, package['sha256'], source))
    require(font == json.loads(check_bound(previous['font_window']['report']).read_text()),
            'Raw font evidence differs from its original independently qualified record')
    ui = canonical(validate_application(json.loads(args.app_report.read_text()), tools, IDENTITY))
    current = validate_current_qualifications(args.app_report, ui, args.platform_report,
                                               args.startup_report, package['sha256'])
    validate_current_base(previous, package, source, verification, font, ui, current)
    mixed = validate_mixed_report(args.mixed_report, package['sha256'], source)
    require(mixed['passed'] is True and mixed['exact_font_requests_per_pid'] == 45
            and mixed['counters']['actions'] == 18
            and mixed['counters']['actual_integer_mask_observations'] == 102,
            'All18 exact25 actions, forty-five requests per PID and102 mask captures required')
    log = args.python_log.read_text()
    match = re.search(r'Ran (\d+) tests in ', log)
    suite = unittest.defaultTestLoader.discover(str(ROOT / 'scripts/tests'), pattern='test_*.py')
    require(match and log.rstrip().endswith('OK') and int(match[1]) == suite.countTestCases(),
            'Full current Python suite must pass with its exact test count')
    now = datetime.now(ZoneInfo('Asia/Shanghai'))
    directory = ROOT / 'reports/emulator' / ('font-raster-flush-mixed-revalidation-' +
                now.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    directory.mkdir()
    record = copy.deepcopy(previous)
    archives = {}
    for path in paths:
        saved = directory / ('before-' + path.name)
        saved.write_bytes(original)
        archives[path.stem] = bound(saved)
    record['history']['qualified_candidate25_before_mixed'] = archives
    independent = directory / 'independent-mixed-revalidation.json'
    independent.write_text(json.dumps(mixed, ensure_ascii=False, indent=2) + '\n')
    current_path = directory / 'independent-current-qualification.json'
    current_path.write_text(json.dumps(current, ensure_ascii=False, indent=2) + '\n')
    record['mixed_routes'] = dict(status='passed-own-limited-exact25-three-route-interleaving',
        report=bound(args.mixed_report), independent_report=bound(independent),
        qualification=mixed, inherited_prior_candidate_pass=False)
    record['current_build_candidate']['promotion'] = (
        'Exact25 limited font windows, own212 UI, all7 UI PID resources,12 initialized starts '
        'and2PID/18-action mixed routes; NOT complete original UI/fonts/application adaptation')
    raw = json.loads(args.mixed_report.read_text())
    record['target']['last_observed_mixed_pid'] = raw['orders'][-1]['pid']
    record['tests'].update(complete_current_python_suite_verified=True,
        python_cases=int(match[1]), python_log=bound(args.python_log))
    require(record['remaining'][0].startswith('25自身三路混合'), 'Unexpected mixed remaining scope')
    record['remaining'] = record['remaining'][1:]
    record['evidence'] += list(archives.values()) + mixed['evidence'] + current['evidence'] + [
        bound(args.font_report), bound(independent), bound(current_path), bound(args.python_log)]
    saved = directory / Path(__file__).name
    saved.write_bytes(Path(__file__).read_bytes())
    record['evidence'].append(bound(saved))
    record['updated_at'] = now.isoformat(timespec='seconds')
    data = json.dumps(record, ensure_ascii=False, indent=2) + '\n'
    for path in [directory / 'result.json'] + paths:
        path.write_text(data)
    print(json.dumps(dict(report_directory=str(directory), status='in_progress',
        actual_mixed_actions=18, actual_mixed_pids=len(mixed['pids']),
        actual_integer_masks=102, exact_font_requests_per_pid=45,
        full_port_complete=False, functional_completion_percent=None), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
