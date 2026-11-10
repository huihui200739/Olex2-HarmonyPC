"""Re-read actual19 three-route artifacts; never infer success from action flags."""
import json
from pathlib import Path
from native_font_mixed_validation import (
    IDENTITY, action_plan, expected_counters, validate_texture_load,
    validate_original_window, validate_lifetime_mixed, validate_font)
from native_font_progress_validation import ROOT, bound, canonical, require, visible
from texture_1d_window_validation import validate as validate_1d
from texture_context_window_validation import validate as validate_retained_1d


def load(path):
    return json.loads(Path(path).read_text())


def validate_mixed_report(report, hap_sha):
    report = Path(report).resolve()
    raw = load(report)
    require(raw['passed'] is True and raw['identity'] == IDENTITY and
            raw['target'] == '127.0.0.1:5555' and raw['hap_sha256'] == hap_sha,
            'Mixed report identity/HAP/target/result differs')
    require(len(raw['orders']) == 2 and [r['first_route'] for r in raw['orders']] == ['original', 'load'],
            'Both opposite three-route orders required')
    pids = set()
    totals = dict(actions=0, original=0, lifetime=0, texture_1d=0,
                  retained_1d=0, load=0, font=0, mixed_checks=0)
    evidence = [bound(report)]
    for order in raw['orders']:
        plan = expected_counters(action_plan(order['first_route']))
        require(order['plan'] == plan and len(order['actions']) == 9, 'Exact nine-action plan differs')
        previous_load = previous_original = previous_lifetime = previous_font = None
        owners = manager = pid = None
        for step, recorded in zip(plan, order['actions']):
            require(all(recorded[k] == v for k, v in step.items()), 'Recorded action differs from fixed plan')
            label = order['first_route'] + '-first-' + str(step['global_lease']) + '-' + step['route']
            folder = report.parent / 'runtime' / (label + '-native')
            before = folder.with_name(label + '-before')
            after = folder.with_name(label + '-after')
            require(folder == ROOT / recorded['folder'], 'Mixed native artifact directory differs')
            manifest = load(folder / 'received-artifacts.json')
            require(manifest['identity'] == IDENTITY and manifest['hap_sha256'] == hap_sha and
                    manifest['run'] == step['run'], 'Mixed native receipt identity differs')
            name = {'font': 'shared-original-font-run-', 'original': 'shared-original-integrated-run-',
                    'load': 'shared-original-texture-load-run-'}[step['route']] + str(step['run']) + '.json'
            observation = load(folder / name)
            legacy = observation['original']['legacy']
            proof = legacy['persistent_gx'] if step['route'] == 'original' else legacy
            current_pid = int(observation['original']['pid'])
            if pid is None:
                pid = current_pid
                pids.add(pid)
            require(current_pid == pid == int(manifest['pid']), 'Mixed route PID changed')
            layouts = {}
            for capture in (before, after):
                require((capture / 'pid.txt').read_text().strip() == str(pid), 'Captured live PID differs')
                ok, details = visible(load(capture / 'layout.json'))
                layouts[capture.name] = canonical(details)
                require(ok, 'Mixed original/new controls clipped')
                evidence += [bound(capture / item) for item in
                             ('layout.json', 'native-verification.json', 'pid.txt', 'process-scoped-report.log')]
            before_state, after_state = [load(c / 'native-verification.json') for c in (before, after)]
            require(before_state['workspaceState'] == after_state['workspaceState'] and
                    before_state['applicationRuntime'] == after_state['applicationRuntime'],
                    'Mixed host workspace/runtime state changed')
            addresses = {k: proof[k] for k in ('owner_address', 'renderer_address', 'scene_address')}
            if owners is None:
                owners = addresses
            require(addresses == owners and all(type(v) is int and v > 0 for v in addresses.values()),
                    'Mixed genuine original owner addresses changed')
            if step['route'] != 'font':
                current_manager = (legacy['texture_load'] if step['route'] == 'load' else
                                   legacy['persistent_textures'])['manager_address']
                if manager is None:
                    manager = current_manager
                require(type(manager) is int and manager > 0 and manager == current_manager,
                        'Persistent texture manager changed across routes')
            lease = step['global_lease']
            require(proof['binding_before'] == proof['detachment_before'] == lease - 1 and
                    proof['binding_after'] == proof['detachment_after'] == lease,
                    'Actual global lease differs from all-route ordinal')
            require(observation['run'] == observation['original']['run'] == legacy['run'] == step['run'],
                    'Actual local route generation differs')
            if step['route'] != 'load':
                require(type(proof['font_requests']) is int and proof['font_requests'] == step['expected_font_requests'],
                        'Exact cumulative font requests differ')
            if step['route'] == 'load':
                checked = validate_texture_load(folder, previous_load)
                require(checked['passed'] and canonical(checked) == recorded['texture_load'],
                        'Raw original loading pixels/logs/restoration differ')
                for when, state in (('before', before_state), ('after', after_state)):
                    log = legacy['texture_load']['log_' + when]
                    require(all(state['originalLog'][k] == log[k] for k in
                                ('posted', 'dropped', 'truncated', 'captureFailures', 'clearCount',
                                 'clearedEntries', 'textSha256')) and
                            state['originalLog']['entryCount'] == len(log['entries']),
                            'Genuine loader logs changed at UI boundary')
                totals['load'] += len(checked['checks'])
                previous_load = folder
            elif step['route'] == 'font':
                checked = validate_font(folder, lease, previous_font)
                require(checked['passed'] and canonical(checked) == recorded['font'],
                        'Raw original glyph pixels/scale/restoration differ')
                totals['font'] += len(checked['checks'])
                previous_font = folder
            else:
                original = validate_original_window(folder, before, after, IDENTITY, create_previews=False)
                lifetime, actual_addresses = validate_lifetime_mixed(proof, str(pid), step['run'],
                    [before_state, after_state], lease, step['expected_font_requests'], previous_lifetime)
                one = validate_1d(folder)
                retained = validate_retained_1d(folder, previous_original)
                require(original['passed'] and len(original['checks']) == 126 and
                        canonical(original) == recorded['original'], 'Original126 raw window gates differ')
                require(len(lifetime) == 10 and all(x['passed'] for x in lifetime) and
                        canonical(lifetime) == recorded['lifetime'], 'All ten exact lease/font lifetime gates differ')
                require(one['passed'] and len(one['checks']) == 64 and canonical(one) == recorded['texture_1d'],
                        'Original64 temporary1D gates differ')
                require(retained['passed'] and canonical(retained) == recorded['retained_1d'],
                        'Retained1D raw restoration gates differ')
                for key, value in (('original', original['checks']), ('lifetime', lifetime),
                                   ('texture_1d', one['checks']), ('retained_1d', retained['checks'])):
                    totals[key] += len(value)
                previous_lifetime = dict(passed=True, pid=str(pid), run=step['run'], addresses=actual_addresses)
                previous_original = folder
            names = {
                'same actual process throughout this planned order',
                'same unique owner renderer and scene across all three entries',
                'global lease counts exactly follow all three planned routes',
                'actual route-local generation in wrapper and original core',
                'saved workspace and initialized runtime unchanged',
                'all three actual controls remain fully visible after window return'}
            if step['route'] != 'font':
                names.add('same persistent manager across original drawing and loading')
            if step['route'] != 'load':
                names.add('exact cumulative planned font requests at font/drawing boundary')
            names.update({
                'load': {'all original load replace log GPU and per-route restoration gates',
                         'UI preserves genuine loader log before', 'UI preserves genuine loader log after'},
                'font': {'all original font glyph scale raster and restoration gates preserved'},
                'original': {'all original 126 pixel matrix driver and host gates preserved',
                             'all ten lifetime gates with explicit global lease ordinal',
                             'all original 64 temporary 1D gates preserved',
                             'all original persistent 1D color and restoration gates preserved'}
                }[step['route']])
            checks_by_name = {item['name']: item for item in recorded['checks']}
            require(len(recorded['checks']) == len(names) and set(checks_by_name) == names,
                    'Mixed check name/count changed or was dropped')
            require(recorded['layout'] == layouts[before.name] and
                    checks_by_name['all three actual controls remain fully visible after window return']['detail'] == layouts[after.name],
                    'Mixed recorded layout evidence differs')
            require(recorded['passed'] is True and all(x['passed'] for x in recorded['checks']),
                    'Mixed recorded check failure must be retained')
            totals['actions'] += 1
            totals['mixed_checks'] += len(recorded['checks'])
            evidence.append(bound(folder / 'received-artifacts.json'))
        require(order['actions'][-1]['route'] == 'font' and
                order['actions'][-1]['expected_font_requests'] == 9 and order['pid'] == pid,
                'Final actual font count must close every prior loading interval')
    require(len(pids) == 2 and totals['actions'] == 18, 'Two real PIDs and all18 actual actions required')
    for name in ('native_font_mixed_validation.py', 'run-native-font-mixed-smoke.py',
                 'native_font_raster_window_validation.py', 'native_font_window_validation.py',
                 'persistent_gx_window_validation.py', 'original_renderer_validation.py',
                 'texture_load_window_validation.py', 'texture_1d_window_validation.py',
                 'texture_context_window_validation.py', 'native_font_progress_validation.py'):
        require((report.parent / name).read_bytes() == (ROOT / 'scripts' / name).read_bytes(),
                'Captured mixed analytical recipe changed: ' + name)
        evidence.append(bound(report.parent / name))
    return dict(passed=True, pids=sorted(pids), counters=totals, raw_report=bound(report),
                all_six_directed_route_pairs_covered=True,
                local_runs_each_route=[1, 2, 3], global_lease_ordinals_per_pid=list(range(1, 10)),
                exact_font_requests_per_pid=9, direct_load_font_counter_available=False,
                font_counter_closure='Exact at all font/drawing boundaries; final font closes all preceding load intervals',
                evidence=evidence, full_port_complete=False, functional_completion_percent=None)
