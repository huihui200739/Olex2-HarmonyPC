"""Independent hash-bound validation of genuine UI PNG loading on a real window.

Fixture data describes synthetic inputs only. It never supplies runtime identities,
GPU states or a successful observation in place of collected device artifacts.
"""
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('LockedAtoms', 'ConstrainedAtoms')
IDENTITY = dict(bundleName='com.example.olex2harmonypc', versionName='0.22.15', versionCode=221500)

def positive(value):
    return type(value) is int and value > 0

def cpu_expected(fixture):
    side, rgb = fixture['width'], fixture['rgb']
    return bytes(v for y in range(side) for x in range(side)
                 for v in (*rgb[(x * side + y) * 3:(x * side + y) * 3 + 3], 255))

def sampled_rgb(fixture, u, v):
    side, rgb = fixture['width'], fixture['rgb']
    sx, sy = u * side - .5, v * side - .5
    x, y = math.floor(sx), math.floor(sy)
    fx, fy = sx - x, sy - y
    def texel(xx, yy, channel):
        # The actual original loader transposes source x/y before GPU upload.
        return rgb[((xx % side) * side + (yy % side)) * 3 + channel]
    return [sum(texel(x + dx, y + dy, c) * wx * wy
                for dx, wx in ((0, 1 - fx), (1, fx))
                for dy, wy in ((0, 1 - fy), (1, fy))) for c in range(3)] + [255]

def _single(folder):
    folder = Path(folder)
    checks, summary = [], {}
    def check(name, value, detail=None):
        checks.append(dict(name=name, passed=bool(value), detail=detail))
    try:
        manifest = json.loads((folder/'received-artifacts.json').read_text())
        run, pid = manifest['run'], manifest['pid']
        check('strict positive run and PID', positive(run) and positive(pid))
        check('explicit candidate15 package', manifest['identity'] == IDENTITY)
        if not all(c['passed'] for c in checks):
            raise ValueError('Invalid process or package identity')
        files = manifest['files']
        for name, record in files.items():
            p = folder/name
            check('safe artifact path ' + name, not Path(name).is_absolute() and '..' not in Path(name).parts and p.resolve().is_relative_to(folder.resolve()))
            if not checks[-1]['passed']:
                raise ValueError('Artifact path outside collected directory')
            data = p.read_bytes()
            check('target hash and size ' + name, len(data) == record['bytes'] and hashlib.sha256(data).hexdigest() == record['sha256'] == record['target_sha256'])
        integrated_name = f'shared-original-texture-load-run-{run}.json'
        integrated = json.loads((folder/integrated_name).read_text())
        original = integrated['original']; legacy = original['legacy']; probe = legacy['texture_load']
        artifact = f'original-texture-load-pid-{pid}-run-{run}'
        check('actual process-scoped route', original['pid'] == pid and
              run == integrated['run'] == original['run'] == legacy['run'] and
              original['artifact_subdirectory'] == manifest['artifact_subdirectory'] == artifact)
        check('main renderer and external bindings revalidated', all(integrated.get(k) is True for k in
              ('limited_path_verified', 'main_graphics_revalidated', 'main_selection_unchanged', 'external_egl_restored')) and
              integrated['main_frames_after'] == integrated['main_frames_before'] + 1)
        check('real isolated main-window context presented and destroyed', all(original.get(k) is True for k in
              ('shared_existing_runtime', 'same_main_window_surface', 'original_context_isolated', 'swapped',
               'temporary_context_detached', 'temporary_context_destroyed', 'parent_state_restored')) and original.get('error') == '')
        check('original loader route', type(probe.get('schema')) is int and probe['schema'] == 1 and
              probe['route'] == 'original-TGXApp-LoadTextures-real-window' and probe['offscreen'] is False)
        check('unique long-lived owner and detached textures', all(legacy.get(k) is True for k in
              ('shared_existing_runtime', 'unique_owner', 'textures_detached_before_gl4es_close', 'scene_unbound', 'scene_disabled', 'execution_completed')) and legacy.get('error') == '')
        addresses = {k: legacy[k] for k in ('owner_address', 'renderer_address', 'scene_address')}
        addresses['manager_address'] = probe['manager_address']
        check('distinct positive persistent owner addresses', all(positive(v) for v in addresses.values()) and len(set(addresses.values())) == 4)
        check('balanced single scene/context lease', type(legacy['binding_before']) is int and type(legacy['detachment_before']) is int and
              legacy['binding_before'] == legacy['detachment_before'] and
              legacy['binding_after'] == legacy['detachment_after'] == legacy['binding_before'] + 1)
        w, h = legacy['width'], legacy['height']
        check('bounded actual window', positive(w) and positive(h) and 64 <= w <= 4096 and 64 <= h <= 4096)
        if not checks[-1]['passed']:
            raise ValueError('Unbounded actual window')
        restored = probe['restored_from_prior_context']
        check('restoration boolean', type(restored) is bool)
        phases = probe['phases']
        expected_phases = (['restored-b'] if restored else []) + ['loaded-a', 'loaded-b']
        check('exact restoration/load/replace phase order', [p['phase'] for p in phases] == expected_phases)
        if not checks[-1]['passed']:
            raise ValueError('Missing or extra loading phases')
        fixtures = json.loads((ROOT/'experiments/gx-texture-load/synthetic-fixtures.json').read_text())['fixtures']
        reference = {(f['generation'], f['name']): f for f in fixtures}
        required = {integrated_name, f'shared-original-window-run-{run}.json', f'actual-driver-trace-run-{run}.log',
                    f'actual-gpu-run-{run}-parent-before.json', f'actual-gpu-run-{run}-parent-after.json'}
        for generation in ('a', 'b'):
            for name in NAMES:
                filename = f'texture-input-{generation}/{name}.png'; required.add(filename)
                raw = (folder/filename).read_bytes(); f = reference[generation, name]
                check('exact synthetic PNG ' + filename, len(raw) == f['png_bytes'] and hashlib.sha256(raw).hexdigest() == f['png_sha256'])
        texture_addresses, phase_hashes = {}, {}
        for phase in phases:
            label = phase['phase']; generation = 'a' if label == 'loaded-a' else 'b'
            check(label + ' exact two named original UI textures', [t['name'] for t in phase['textures']] == list(NAMES))
            if not checks[-1]['passed']:
                raise ValueError('Original named textures differ')
            for texture in phase['textures']:
                name = texture['name']; f = reference[generation, name]
                check(label + ' ' + name + ' size/type metadata', type(texture['width']) is int and type(texture['height']) is int and texture['width'] == texture['height'] == f['width'])
                check(label + ' ' + name + ' resident address/id', positive(texture['texture_address']) and positive(texture['resident_id']) and texture['resident_id'] != 0xffffffff)
                if name not in texture_addresses:
                    texture_addresses[name] = texture['texture_address']
                check(label + ' ' + name + ' retained object across replacement', texture_addresses[name] == texture['texture_address'])
                check(label + ' ' + name + ' original sampler applied', all(type(texture[k]) is int for k in ('min_filter', 'mag_filter', 'wrap_s', 'wrap_t')) and
                      (texture['min_filter'], texture['mag_filter'], texture['wrap_s'], texture['wrap_t']) == (9728, 9729, 10497, 10497))
                name_cpu = f'load-textures-run-{run}-{label}-{name}-cpu.rgba'; required.add(name_cpu)
                check(label + ' ' + name + ' exact transposed original readback', (folder/name_cpu).read_bytes() == cpu_expected(f))
            gpu_name = f'actual-gpu-run-{run}-load-textures-{label}.json'; required.add(gpu_name)
            gpu = json.loads((folder/gpu_name).read_text())
            check(label + ' real linked driver sampler on default window framebuffer', positive(gpu.get('program')) and gpu.get('framebuffer') == 0 and gpu.get('viewport') == [0, 0, w, h] and
                  gpu.get('link_status') == 1 and gpu.get('program_info') == '' and gpu.get('driver_error_before_snapshot') == gpu.get('driver_error') == 0 and
                  any(u.get('type') == 35678 and u.get('size') == 1 for u in gpu.get('active_uniform_values', [])))
            pixel_name = f'load-textures-run-{run}-{label}.rgba'; required.add(pixel_name)
            raw = (folder/pixel_name).read_bytes(); phase_hashes[label] = hashlib.sha256(raw).hexdigest()
            check(label + ' exact framebuffer byte length', len(raw) == w * h * 4)
            if len(raw) != w * h * 4:
                continue
            count, failures = 0, []
            for y in range(0, h, max(1, h // 70)):
                for x in range(0, w, max(1, w // 100)):
                    index = 0 if x + .5 < w / 2 else 1
                    u = (x + .5 - index * w / 2) / (w / 2)
                    expected = sampled_rgb(reference[generation, NAMES[index]], u, (y + .5) / h)
                    actual = list(raw[(y * w + x) * 4:(y * w + x + 1) * 4]); count += 1
                    if any(abs(a - b) > 2 for a, b in zip(actual, expected)) and len(failures) < 10:
                        failures.append(dict(x=x, y=y, expected=expected, actual=actual))
            check(label + ' independent bilinear-repeat GPU pixels', count > 1000 and not failures, dict(samples=count, failures=failures))
        check('all required native artifacts target-hash bound', required.issubset(files), sorted(required - set(files)))
        check('standalone and integrated native observations agree', json.loads((folder/f'shared-original-window-run-{run}.json').read_text()) == original)
        check('actual parent GPU state exactly restored', (folder/f'actual-gpu-run-{run}-parent-before.json').read_bytes() == (folder/f'actual-gpu-run-{run}-parent-after.json').read_bytes())
        check('two distinct original texture objects', len(set(texture_addresses.values())) == 2 and not set(texture_addresses.values()).intersection(addresses.values()))
        before, after = probe['log_before'], probe['log_after']
        for label, snapshot in (('before', before), ('after', after)):
            check('actual original log ' + label + ' hash', snapshot['attached'] is True and
                  hashlib.sha256(''.join(e['text'] for e in snapshot['entries']).encode()).hexdigest() == snapshot['textSha256'])
        check('original log prefix preserved', after['entries'][:len(before['entries'])] == before['entries'])
        check('original log was not cleared/suppressed/evicted', after['posted'] == before['posted'] + 4 and
              all(after[k] == before[k] for k in ('dropped', 'truncated', 'captureFailures', 'clearCount', 'clearedEntries')))
        delta = after['entries'][len(before['entries']):]
        check('exact four real loader notifications', len(delta) == 4 and [e['sequence'] for e in delta] == list(range(before['posted'] + 1, before['posted'] + 5)) and
              all(e['truncated'] is False for e in delta) and [e['text'].strip() for e in delta] == [name + ' loaded...' for name in NAMES] * 2)
        check('real PNG decoder allocations balanced', probe.get('decoder_allocations_balanced') is True)
        summary = dict(pid=pid, run=run, width=w, height=h, addresses=addresses, texture_addresses=texture_addresses,
                       phase_hashes=phase_hashes, restored=restored, log_before=before['posted'], log_after=after['posted'])
    except (OSError, ValueError, KeyError, TypeError, IndexError, AttributeError) as error:
        check('complete readable actual texture-load evidence', False, str(error))
    return dict(passed=bool(checks) and all(c['passed'] for c in checks), checks=checks, **summary)

def validate(folder, previous_folder=None):
    current = _single(folder); checks = list(current['checks']); prior = None
    def check(name, value, detail=None):
        checks.append(dict(name=name, passed=bool(value), detail=detail))
    if previous_folder is None:
        check('cold first load is not restoration proof', current.get('run') == 1 and current.get('restored') is False)
    else:
        prior = _single(previous_folder)
        check('both original window observations independently pass', prior['passed'] and current['passed'], [c for c in prior['checks'] if not c['passed']])
        check('same process consecutive separate context leases', current.get('pid') == prior.get('pid') and positive(current.get('run')) and current['run'] == prior.get('run', -2) + 1 and Path(folder).resolve() != Path(previous_folder).resolve())
        check('same unique owner renderer manager and two texture objects', current.get('addresses') == prior.get('addresses') and current.get('texture_addresses') == prior.get('texture_addresses'))
        check('same actual window and exact restored GPU framebuffer', current.get('restored') is True and current.get('width') == prior.get('width') and current.get('height') == prior.get('height') and
              current.get('phase_hashes', {}).get('restored-b') == prior.get('phase_hashes', {}).get('loaded-b'))
        check('original log notifications survive context handoff', current.get('log_before') == prior.get('log_after'))
    passed = current['passed'] and all(c['passed'] for c in checks)
    return dict(passed=passed, checks=checks, observation={k: v for k, v in current.items() if k not in ('passed', 'checks')},
                actual_context_migration_verified=bool(passed and prior is not None), full_port_complete=False,
                functional_completion_percent=None, physical_pc_verified=False,
                scope='Genuine original named UI texture loading/replacement and normal level-0 RGB 2D context restoration, real log delta and sampled bilinear-repeat actual-window pixels. Not full UI, minification GPU behavior, mipmaps, other formats or exceptional recovery.')
