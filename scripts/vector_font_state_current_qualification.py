"""Offline raw-evidence audit of candidate21's own full diagnostic qualifications."""
import json
from pathlib import Path

from app_report_log import parse_app_reports
from gx_application_validation import (
    validate_lifecycle, validate_image_observations, inspect_owned_resource_observations,
    validate_no_decoder_control)
from png_texture_hook_validation import validate_png_texture_hook
from vector_font_state_startup_validation import validate_runtime_snapshot
from vector_font_state_tool_profiles import collection_source, startup_source
from native_font_progress_validation import ROOT, bound, canonical, require
from vector_font_state_window_validation import IDENTITY


def load(path):
    return json.loads(Path(path).read_text())


def digest(path):
    return bound(path)['sha256']


def process_ids(value):
    found = set()
    if isinstance(value, dict):
        for name, item in value.items():
            if (name == 'pid' or name.endswith('_pid')) and str(item).isdigit() and int(item) > 0:
                found.add(int(item))
            found.update(process_ids(item))
    elif isinstance(value, list):
        for item in value:
            found.update(process_ids(item))
    return found


def platform_checks(raw, fixtures):
    return dict(lifecycle=validate_lifecycle(raw['lifecycle']),
                image_capabilities=validate_image_observations(raw['images'], fixtures,
                    require_resources=True, resource_proof='owned-memory'),
                owned_resources=inspect_owned_resource_observations(raw, proof='owned-memory',
                    texture_backend='libpng-unassociated-rgb'),
                no_decoder_control=validate_no_decoder_control(raw['no_decoder_control'], 'before-first-image-api'),
                post_decode_control=validate_no_decoder_control(raw['post_decode_control'], 'after-capability-probes'),
                texture_hook=validate_png_texture_hook(raw['texture_hook'], fixtures,
                    require_resources=True, resource_proof='owned-memory'))


def check_profile(path, expected, baseline):
    report = load(path)
    profile = report['execution_profile']
    require(profile['source_file'] == 'executed-profile.py' and profile['baseline'] == baseline,
            'Executed profile filename/baseline differs')
    saved = path.parent / profile['source_file']
    require(saved.resolve().is_relative_to(path.parent.resolve()), 'Profile path escapes report directory')
    require(profile['candidate'] == '0.22.21' and saved.read_text() == expected
            and digest(saved) == profile['source_sha256'], 'Executed exact21 profile changed')
    names = {'vector_font_state_tool_profiles.py', 'vector_font_state_application_validation.py',
             'vector_font_state_startup_validation.py', 'texture_context_application_validation.py',
             'texture_context_startup_validation.py'}
    require(set(report['profile_dependencies']) == names, 'Exact21 profile dependency set differs')
    evidence = [bound(saved)]
    for name, expected_hash in report['profile_dependencies'].items():
        require(digest(path.parent / name) == expected_hash == digest(ROOT / 'scripts' / name),
                'Executed profile dependency changed: ' + name)
        evidence.append(bound(path.parent / name))
    return evidence


def validate_current_qualifications(app_path, ui, platform_path, startup_path, hap_sha):
    app_path, platform_path, startup_path = map(lambda p: Path(p).resolve(),
                                               (app_path, platform_path, startup_path))
    app, platform, startup = load(app_path), load(platform_path), load(startup_path)
    require(ui['passed'] is True and ui['total_ui_checks'] == 212, 'Full current212 UI must be revalidated')
    require(platform['passed'] is True and startup['passed'] is True
            and platform['target'] == startup['target'] == '127.0.0.1:5555',
            'Platform/startup did not pass on the selected existing emulator')
    require(platform['application']['identity'] == startup['identity'] == IDENTITY
            and platform['application']['hap_sha256'] == startup['hap']['sha256'] == hap_sha,
            'Platform/startup actual candidate identity differs')
    require(platform['application']['ui'] == ui and platform['application']['sha256'] == digest(app_path),
            'Collected current UI qualification differs from revalidated raw UI')
    for report in (platform, startup):
        require(report['resource_proof'] == 'owned-memory' and
                report['texture_backend'] == 'libpng-unassociated-rgb' and
                report['post_decode_control_required'] is True,
                'All owned-memory/PNG/post-decode controls are required')
    require(startup['owned_resource_audit_required'] is True, 'Startup resource proof is mandatory')
    fixtures = load(ROOT / 'scripts/tests/fixtures/gx_synthetic_images.json')['fixtures']
    fixture_hashes = {item['file']: item['sha256'] for item in fixtures}
    required_files = {'result.json', '合成颜色-2.png'} | set(fixture_hashes)
    pids = process_ids(app)
    require(len(pids) == 7 and {row['pid'] for row in platform['processes']} == pids
            and len(platform['processes']) == 7, 'Every one of the seven actual UI PIDs must be collected exactly once')
    require(platform['current_pid'] in pids, 'Current platform process is absent from actual UI evidence')
    evidence = [bound(platform_path), bound(startup_path)]
    for row in platform['processes']:
        folder = platform_path.parent / ('pid-' + str(row['pid']))
        raw = load(folder / 'result.json')
        require(raw['pid'] == row['pid'] and raw.get('execution_completed') is True,
                'Native platform process identity/completion differs')
        require(set(row['files']) == required_files, 'Every exact synthetic resource must be retained')
        for name, metadata in row['files'].items():
            artifact = folder / name
            require(artifact.resolve().is_relative_to(folder.resolve()), 'Platform artifact escapes PID directory')
            actual = bound(artifact)
            require(actual['bytes'] == metadata['bytes'] and
                    actual['sha256'] == metadata['sha256'] == metadata['target_sha256'],
                    'Platform target/received bytes changed: ' + name)
            if name in fixture_hashes:
                require(actual['sha256'] == fixture_hashes[name], 'Fixed independent PNG bytes differ: ' + name)
            evidence.append(actual)
        require(row['files']['合成颜色-2.png']['sha256'] == fixture_hashes['synthetic-rgb-2.png'],
                'Unicode PNG differs from fixed RGB reference')
        for key, checked in canonical(platform_checks(raw, fixtures)).items():
            require(checked.get('passed', True) is True and row[key] == checked,
                    'Raw platform image/resource gate changed or failed: ' + key)
        evidence.append(bound(folder / 'result.json'))
    require(len(startup['runs']) == 12 and startup.get('long_lived_runtime_initialization_required') is True,
            'Exactly12 real initialized starts required')
    require(len({r['pid'] for r in startup['runs']}) == 12 and
            [r['run'] for r in startup['runs']] == list(range(1, 13)),
            'Startup PID/ordinal protocol differs')
    for row in startup['runs']:
        require(row['passed'] is True, 'Failed startup cannot be omitted')
        folder = startup_path.parent / ('run-' + str(row['run']))
        raw = load(folder / 'native-result.json')
        require(raw['pid'] == row['pid'] and raw.get('execution_completed') is True and
                digest(folder / 'native-result.json') == row['native_result_sha256'] == row['target_sha256'],
                'Raw target startup evidence differs')
        for key, checked in canonical(platform_checks(raw, fixtures)).items():
            require(checked.get('passed', True) is True and row['images' if key == 'image_capabilities' else key] == checked,
                    'Startup image/resource gate changed or failed: ' + key)
        snapshot = load(folder / 'runtime-status.json')
        runtime = validate_runtime_snapshot(snapshot, IDENTITY)
        require(runtime['passed'] and len(runtime['checks']) == 10 and row['runtime_owner'] == runtime
                and digest(folder / 'runtime-status.json') == row['runtime_status_sha256']
                and digest(folder / 'runtime-process-report.log') == row['runtime_log_sha256'],
                'Initialized actual owner startup evidence changed or failed')
        require(snapshot in parse_app_reports((folder / 'runtime-process-report.log').read_text()),
                'Runtime snapshot absent from explicit live PID capture')
        evidence += [bound(folder / name) for name in
                     ('native-result.json', 'runtime-status.json', 'runtime-process-report.log')]
    # Owned-memory/public-release qualification does not establish complete FD identity.
    # Preserve the original false value unless every revalidated raw scope proves it.
    for report, rows in ((platform, platform['processes']), (startup, startup['runs'])):
        actual = all(row['owned_resources']['complete_descriptor_metadata_scan_passed'] is True for row in rows)
        require(type(report['complete_descriptor_metadata_scan_passed']) is bool and
                report['complete_descriptor_metadata_scan_passed'] == actual,
                'Full descriptor metadata summary differs from original scoped observations')
    evidence += check_profile(platform_path, collection_source(), 'collect-gx-texture-context-evaluation.py')
    evidence += check_profile(startup_path, startup_source(), 'run-texture-context-startup-audit.py')
    return dict(passed=True,
                platform_image_audit=dict(status='passed-for-all-seven-current-UI-PIDs',
                    report=bound(platform_path), pids=sorted(pids), inherited_prior_candidate_pass=False,
                    complete_descriptor_metadata_scan_passed=platform['complete_descriptor_metadata_scan_passed']),
                startup_audit=dict(status='passed-fixed-12-current-candidate-initialized-startups',
                    report=bound(startup_path), actual_starts=12, passed=12, failed=0,
                    actual_long_lived_owner_startup_checks=120, inherited_prior_candidate_pass=False,
                    all_twelve_passed=True, process_fd_equality_check_preserved=True,
                    complete_descriptor_metadata_scan_passed=startup['complete_descriptor_metadata_scan_passed'],
                    all_sdk_internal_allocations_proven_released=False),
                last_startup_pid=startup['runs'][-1]['pid'], evidence=evidence,
                full_port_complete=False, functional_completion_percent=None)
