"""Isolated synthetic parser/negative tests: no device, GL or runtime evidence."""
import hashlib
import json
import runpy
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'scripts'))
from texture_load_window_validation import IDENTITY, NAMES, cpu_expected, positive, sampled_rgb, validate

GENERATOR = runpy.run_path(str(ROOT/'scripts/generate-texture-load-fixtures.py'))
FIXTURES = json.loads((ROOT/'experiments/gx-texture-load/synthetic-fixtures.json').read_text())['fixtures']
REFERENCE = {(f['generation'], f['name']): f for f in FIXTURES}

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

class TextureLoadValidationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='isolated-texture-parser-fixture-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.first = self.make('first', 1, False)
        self.second = self.make('second', 2, True)

    def make(self, name, run, restored):
        folder = self.root/name; folder.mkdir()
        files = {}
        def save(name, raw):
            path = folder/name; path.parent.mkdir(parents=True, exist_ok=True)
            if not isinstance(raw, bytes):
                raw = json.dumps(raw).encode()
            path.write_bytes(raw)
            files[name] = dict(bytes=len(raw), sha256=digest(raw), target_sha256=digest(raw))
        phases = []
        for label in (['restored-b'] if restored else []) + ['loaded-a', 'loaded-b']:
            generation = 'a' if label == 'loaded-a' else 'b'
            textures = []
            for index, texture_name in enumerate(NAMES):
                fixture = REFERENCE[generation, texture_name]
                textures.append(dict(name=texture_name, width=fixture['width'], height=fixture['width'],
                    texture_address=200+index, resident_id=index+1, min_filter=9728, mag_filter=9729, wrap_s=10497, wrap_t=10497))
                save(f'load-textures-run-{run}-{label}-{texture_name}-cpu.rgba', cpu_expected(fixture))
            phases.append(dict(phase=label, textures=textures))
            # Clearly synthetic data exercising the verifier, not observed pixels.
            raw = bytearray()
            for y in range(64):
                for x in range(128):
                    index = int(x >= 64)
                    raw.extend(round(c) for c in sampled_rgb(REFERENCE[generation, NAMES[index]],
                               (x + .5 - index*64)/64, (y+.5)/64))
            save(f'load-textures-run-{run}-{label}.rgba', bytes(raw))
            save(f'actual-gpu-run-{run}-load-textures-{label}.json', dict(
                program=9, framebuffer=0, viewport=[0, 0, 128, 64], link_status=1, program_info='',
                driver_error_before_snapshot=0, driver_error=0,
                active_uniform_values=[dict(name='isolated_synthetic_sampler', type=35678, size=1)]))
        for generation in ('a', 'b'):
            for texture_name in NAMES:
                fixture = REFERENCE[generation, texture_name]
                save(f'texture-input-{generation}/{texture_name}.png', GENERATOR['png'](fixture['width'], bytes(fixture['rgb'])))
        def log(posted):
            entries = [dict(sequence=i+1, text=NAMES[i % 2]+' loaded...\n', truncated=False) for i in range(posted)]
            return dict(attached=True, posted=posted, entries=entries, dropped=0, truncated=0, captureFailures=0,
                        clearCount=0, clearedEntries=0, textSha256=digest(''.join(e['text'] for e in entries).encode()))
        probe = dict(schema=1, route='original-TGXApp-LoadTextures-real-window', offscreen=False,
                     restored_from_prior_context=restored, manager_address=103, phases=phases,
                     decoder_allocations_balanced=True, log_before=log((run-1)*4), log_after=log(run*4))
        legacy = dict(run=run, width=128, height=64, shared_existing_runtime=True, unique_owner=True,
                      owner_address=100, renderer_address=101, scene_address=102, binding_before=run-1,
                      binding_after=run, detachment_before=run-1, detachment_after=run,
                      textures_detached_before_gl4es_close=True, scene_unbound=True, scene_disabled=True,
                      texture_load=probe, execution_completed=True, error='')
        original = dict(run=run, pid=1234, artifact_subdirectory=f'original-texture-load-pid-1234-run-{run}',
                        shared_existing_runtime=True, same_main_window_surface=True, original_context_isolated=True,
                        swapped=True, temporary_context_detached=True, temporary_context_destroyed=True,
                        parent_state_restored=True, legacy=legacy, error='')
        integrated = dict(run=run, limited_path_verified=True, main_graphics_revalidated=True,
                          main_selection_unchanged=True, external_egl_restored=True,
                          main_frames_before=3, main_frames_after=4, original=original)
        save(f'shared-original-texture-load-run-{run}.json', integrated)
        save(f'shared-original-window-run-{run}.json', original)
        for phase in ('before', 'after'):
            save(f'actual-gpu-run-{run}-parent-{phase}.json', dict(isolated_synthetic_fixture=True))
        save(f'actual-driver-trace-run-{run}.log', b'Isolated synthetic parser fixture. No actual GPU or driver executed.\n')
        (folder/'received-artifacts.json').write_text(json.dumps(dict(pid=1234, run=run, identity=IDENTITY,
            artifact_subdirectory=original['artifact_subdirectory'], files=files)))
        return folder

    def mutate(self, folder, callback, rebind=True):
        manifest = json.loads((folder/'received-artifacts.json').read_text()); run=manifest['run']
        path = folder/f'shared-original-texture-load-run-{run}.json'
        d = json.loads(path.read_text()); callback(d)
        path.write_text(json.dumps(d))
        original_path = folder/f'shared-original-window-run-{run}.json'
        original_path.write_text(json.dumps(d['original']))
        if rebind:
            self.rebind(folder, path.name)
            self.rebind(folder, original_path.name)

    def rebind(self, folder, name):
        path=folder/'received-artifacts.json'; manifest=json.loads(path.read_text()); raw=(folder/name).read_bytes()
        manifest['files'][name]=dict(bytes=len(raw),sha256=digest(raw),target_sha256=digest(raw));path.write_text(json.dumps(manifest))

    def rejects(self, folder=None, previous=None):
        result = validate(folder or self.first, previous)
        self.assertFalse(result['passed'], result)
        return result

    def test_isolated_positive_shape_is_not_migration(self):
        result=validate(self.first);self.assertTrue(result['passed'],result)
        self.assertFalse(result['actual_context_migration_verified']);self.assertFalse(result['full_port_complete'])
    def test_isolated_consecutive_shape(self):
        result=validate(self.second,self.first);self.assertTrue(result['passed'],result)
        self.assertTrue(result['actual_context_migration_verified']);self.assertIsNone(result['functional_completion_percent'])
    def test_truth_values_are_not_integer_identities(self):
        self.assertFalse(positive(True));self.assertFalse(positive(0));self.assertFalse(positive('3'))
    def test_manual_non_symmetric_transpose(self):
        f=dict(width=2,rgb=[10,11,12,20,21,22,30,31,32,40,41,42])
        self.assertEqual(cpu_expected(f),bytes([10,11,12,255,30,31,32,255,20,21,22,255,40,41,42,255]))
        self.assertEqual(sampled_rgb(f,.75,.25),[30,31,32,255])
        self.assertEqual(sampled_rgb(f,.25,.75),[20,21,22,255])
        self.assertEqual(sampled_rgb(f,0,.25),[20,21,22,255])
    def test_rejects_malformed_manifest(self):
        (self.first/'received-artifacts.json').write_text('{');self.rejects()
    def test_rejects_wrong_package(self):
        p=self.first/'received-artifacts.json';d=json.loads(p.read_text());d['identity']['versionName']='0.22.14';p.write_text(json.dumps(d));self.rejects()
    def test_rejects_target_hash_change(self):
        p=self.first/'received-artifacts.json';d=json.loads(p.read_text());next(iter(d['files'].values()))['target_sha256']='0'*64;p.write_text(json.dumps(d));self.rejects()
    def test_rejects_standalone_rebind_without_integrated(self):
        self.mutate(self.first,lambda d:d.update(main_graphics_revalidated=False),False);self.rejects()
    def test_rejects_unrestored_parent(self):
        self.mutate(self.first,lambda d:d['original'].update(parent_state_restored=False));self.rejects()
    def test_rejects_missing_context_destruction(self):
        self.mutate(self.first,lambda d:d['original'].update(temporary_context_destroyed=False));self.rejects()
    def test_rejects_offscreen_substitution(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load'].update(offscreen=True));self.rejects()
    def test_rejects_unbalanced_scene_lease(self):
        self.mutate(self.first,lambda d:d['original']['legacy'].update(detachment_after=0));self.rejects()
    def test_rejects_undetached_textures(self):
        self.mutate(self.first,lambda d:d['original']['legacy'].update(textures_detached_before_gl4es_close=False));self.rejects()
    def test_rejects_different_replacement_object(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['phases'][1]['textures'][0].update(texture_address=300));self.rejects()
    def test_rejects_wrong_filter(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['phases'][0]['textures'][0].update(mag_filter=9728));self.rejects()
    def test_rejects_wrong_wrapping(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['phases'][0]['textures'][0].update(wrap_s=33071));self.rejects()
    def test_rejects_cpu_bytes_even_if_rehashed(self):
        p=self.first/'load-textures-run-1-loaded-a-LockedAtoms-cpu.rgba';b=bytearray(p.read_bytes());b[0]^=255;p.write_bytes(b);self.rebind(self.first,p.name);self.rejects()
    def test_rejects_gpu_pixels_even_if_rehashed(self):
        p=self.first/'load-textures-run-1-loaded-a.rgba';p.write_bytes(bytes(len(p.read_bytes())));self.rebind(self.first,p.name);self.rejects()
    def test_rejects_wrong_png_even_if_rehashed(self):
        p=self.first/'texture-input-a/LockedAtoms.png';b=bytearray(p.read_bytes());b[-1]^=1;p.write_bytes(b);self.rebind(self.first,'texture-input-a/LockedAtoms.png');self.rejects()
    def test_rejects_missing_manifest_binding(self):
        p=self.first/'received-artifacts.json';d=json.loads(p.read_text());d['files'].pop('load-textures-run-1-loaded-a.rgba');p.write_text(json.dumps(d));self.rejects()
    def test_rejects_rebound_non_default_framebuffer(self):
        p=self.first/'actual-gpu-run-1-load-textures-loaded-a.json';d=json.loads(p.read_text());d['framebuffer']=7;p.write_text(json.dumps(d));self.rebind(self.first,p.name);self.rejects()
    def test_rejects_removed_log_notifications(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['log_after'].update(posted=0));self.rejects()
    def test_rejects_cleared_log(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['log_after'].update(clearCount=1));self.rejects()
    def test_rejects_log_hash_change(self):
        self.mutate(self.first,lambda d:d['original']['legacy']['texture_load']['log_after'].update(textSha256='0'*64));self.rejects()
    def test_rejects_changed_previous_context(self):
        self.mutate(self.first,lambda d:d['original'].update(temporary_context_destroyed=False));self.rejects(self.second,self.first)
    def test_rejects_invented_restoration(self):
        self.mutate(self.second,lambda d:d['original']['legacy']['texture_load'].update(restored_from_prior_context=False));self.rejects(self.second,self.first)
    def test_rejects_run_gap(self):
        p=self.second/'received-artifacts.json';d=json.loads(p.read_text());d['run']=4;p.write_text(json.dumps(d));self.rejects(self.second,self.first)
    def test_rejects_same_folder_continuation(self):
        self.rejects(self.first,self.first)
    def test_fixture_generation_replays_exactly(self):
        header,reference=GENERATOR['render']()
        self.assertEqual(header,(ROOT/'experiments/gx-texture-load/native/original_texture_load_fixtures.h').read_text())
        self.assertEqual(reference,(ROOT/'experiments/gx-texture-load/synthetic-fixtures.json').read_text())

if __name__=='__main__':
    unittest.main()
