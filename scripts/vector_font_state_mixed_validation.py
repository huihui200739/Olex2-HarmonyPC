"""Exact21 native/vector mixed routes with independent local runs and global leases.

The complete21 native/vector/mask verifier is isolated for each call. Only its
single-route lease equation is replaced by the explicitly planned global lease.
All original six-request, glyph, pixel, context and GLint-mask gates remain.
"""
import json
from pathlib import Path
from native_font_mixed_validation import (
    action_plan, validate_original_window, validate_lifetime_mixed)
from persistent_gx_window_validation import replace_once
from vector_font_state_window_validation import IDENTITY
from native_font_raster_window_validation import expected_frame_with_raster, raster_position
from native_font_progress_validation import require

ROOT = Path(__file__).resolve().parents[1]

RECIPE_FILES = (
    'vector_font_state_mixed_validation.py', 'run-vector-font-state-mixed-smoke.py',
    'vector_font_state_mixed_progress_validation.py', 'vector_font_state_window_validation.py',
    'original_vector_font_window_validation.py', 'native_font_raster_window_validation.py',
    'native_font_window_validation.py', 'native_font_mixed_validation.py',
    'persistent_gx_window_validation.py', 'original_renderer_validation.py',
    'texture_load_window_validation.py', 'texture_1d_window_validation.py',
    'texture_context_window_validation.py', 'native_font_progress_validation.py',
    'prepare_texture_load_support.py', 'original_renderer_capture.py',
    'original_renderer_click.py', 'app_report_log.py', 'renderer_probe_identity.py',
    'gx_target_identity.py', 'layout_validation.py')
_load_base = ROOT / 'scripts/texture_load_window_validation.py'
_load_text = replace_once(_load_base.read_text(), "versionName='0.22.15', versionCode=221500",
                          "versionName='0.22.21', versionCode=222100")
_load_text = replace_once(_load_text, "'explicit candidate15 package'", "'explicit candidate21 package'")
_load_namespace = dict(__name__='candidate21_exact_mixed_load', __file__=str(_load_base))
exec(compile(_load_text, str(_load_base) + ' [exact21 mixed]', 'exec'), _load_namespace)
validate_texture_load = _load_namespace['validate']


def font_profile(expected_lease):
    require(type(expected_lease) is int and expected_lease > 0, 'Explicit positive global lease required')
    path = ROOT / 'scripts/vector_font_state_window_validation.py'
    namespace = dict(__name__='candidate21_isolated_complete_font_mixed_profile', __file__=str(path))
    exec(compile(path.read_text(), str(path) + ' [isolated mixed]', 'exec'), namespace)
    vector = namespace['_namespace']
    native_text = replace_once(vector['text'],
        "legacy['binding_before']==legacy['detachment_before']==run-1 and legacy['binding_after']==legacy['detachment_after']==run",
        "legacy['binding_before']==legacy['detachment_before']==EXPECTED_GLOBAL_LEASE-1 and legacy['binding_after']==legacy['detachment_after']==EXPECTED_GLOBAL_LEASE")
    native_path = ROOT / 'scripts/native_font_window_validation.py'
    native = dict(__name__='candidate21_complete_native_gates_explicit_global_lease',
                  __file__=str(native_path), EXPECTED_GLOBAL_LEASE=expected_lease)
    exec(compile(native_text, str(native_path) + ' [explicit global mixed lease]', 'exec'), native)
    native['expected_frame'] = expected_frame_with_raster
    vector['_namespace'] = native
    namespace['native_mixed_source'] = native_text
    require(namespace['IDENTITY'] == native['IDENTITY'] == IDENTITY,
            'Complete mixed font verifier lost exact21 identity')
    return namespace


def validate_font(folder, expected_lease, previous=None, source=None, expected_font_requests=None):
    require(source is not None, 'Exact frozen21 source required for mixed vector fonts')
    folder = Path(folder)
    manifest = json.loads((folder / 'received-artifacts.json').read_text())
    run = manifest['run']
    require(type(run) is int and run > 0 and type(expected_font_requests) is int and
            expected_font_requests == 6 * run, 'Each exact21 font action must account for six genuine requests')
    checked = font_profile(expected_lease)['validate'](folder, previous, source)
    legacy = json.loads((folder / ('shared-original-font-run-' + str(run) + '.json')).read_text())['original']['legacy']
    checked['raster_reference'] = [dict(label=case['label'], requested_integer_origin=case['origin'],
        independent_float32_fmla_window=raster_position(case, legacy['width'], legacy['height']))
        for case in legacy['font']['cases']]
    checked['expected_global_lease'] = expected_lease
    checked['expected_cumulative_font_requests'] = expected_font_requests
    return checked


def expected_counters(plan):
    counts = dict(font=0, original=0, load=0)
    result = []
    for lease, route in enumerate(plan, 1):
        require(route in counts, 'Unknown mixed route')
        counts[route] += 1
        result.append(dict(route=route, run=counts[route], global_lease=lease,
                           expected_font_requests=6 * counts['font']))
    return result
