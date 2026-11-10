"""Three independent route counters, cumulative leases and exact observed font counts.

Only explicit mixed-action ordinals replace the single-route lease assumptions.
No font zero-check is deleted: it becomes the exact cumulative planned count.
"""
import inspect
import json
from pathlib import Path
from persistent_gx_window_validation import SOURCE, original_profile, replace_once, validate_lifetime
from native_font_raster_window_validation import IDENTITY, expected_frame_with_raster, raster_position

ROOT = Path(__file__).resolve().parents[1]
_base = ROOT / 'scripts/texture_load_window_validation.py'
_text = replace_once(_base.read_text(), "versionName='0.22.15', versionCode=221500",
                     "versionName='0.22.19', versionCode=221900")
_text = replace_once(_text, "'explicit candidate15 package'", "'explicit candidate19 package'")
_load = dict(__name__='candidate19_exact_mixed_loader', __file__=str(_base))
exec(compile(_text, str(_base) + ' [exact19 mixed]', 'exec'), _load)
validate_texture_load = _load['validate']

_lifetime_text = inspect.getsource(validate_lifetime)
_lifetime_text = replace_once(_lifetime_text,
    'def validate_lifetime(proof,pid,run,snapshots,previous=None):',
    'def validate_lifetime(proof,pid,run,snapshots,expected_lease,expected_font_requests,previous=None):')
_lifetime_text = replace_once(_lifetime_text, "proof['binding_after']==proof['binding_before']+1==run",
                             "proof['binding_after']==proof['binding_before']+1==expected_lease")
_lifetime_text = replace_once(_lifetime_text, "proof['font_requests']==0",
                             "type(expected_font_requests) is int and expected_font_requests>=0 and proof['font_requests']==expected_font_requests")
_lifetime_text = replace_once(_lifetime_text, "'persistent GX no premature destruction or font claims'",
                             "'persistent GX no premature destruction and exact planned cumulative font requests'")
_lifetime_text = replace_once(_lifetime_text, "'one balanced context lease per actual run'",
                             "'one balanced global context lease per mixed action'")
_lifetime = dict(SOURCE=SOURCE)
exec(compile(_lifetime_text, 'candidate19_exact_global_lease_and_font_count', 'exec'), _lifetime)
validate_lifetime_mixed = _lifetime['validate_lifetime']

_original = dict(__name__='candidate19_original126_mixed', __file__=str(ROOT / 'scripts/original_renderer_validation.py'))
exec(compile(original_profile(), _original['__file__'], 'exec'), _original)
validate_original_window = _original['validate']

_font_base = ROOT / 'scripts/native_font_window_validation.py'
_font_text = replace_once(_font_base.read_text(), "versionName='0.22.17',versionCode=221700",
                          "versionName='0.22.19',versionCode=221900")
_font_text = replace_once(_font_text, 'def validate(folder,previous=None):',
                          'def validate(folder,expected_lease,previous=None):')
_font_text = replace_once(_font_text,
    "legacy['binding_before']==legacy['detachment_before']==run-1 and legacy['binding_after']==legacy['detachment_after']==run",
    "type(expected_lease) is int and expected_lease>0 and legacy['binding_before']==legacy['detachment_before']==expected_lease-1 and legacy['binding_after']==legacy['detachment_after']==expected_lease")
_font = dict(__name__='candidate19_font_glyphs_explicit_mixed_lease', __file__=str(_font_base))
exec(compile(_font_text, str(_font_base) + ' [explicit mixed lease]', 'exec'), _font)
_font['expected_frame'] = expected_frame_with_raster


def validate_font(folder, expected_lease, previous=None):
    checked = _font['validate'](folder, expected_lease, previous)
    legacy = json.loads((Path(folder) / ('shared-original-font-run-' + str(checked['run']) + '.json')).read_text())['original']['legacy']
    checked['raster_reference'] = [dict(label=case['label'], requested_integer_origin=case['origin'],
        independent_float32_fmla_window=raster_position(case, legacy['width'], legacy['height']))
        for case in legacy['font']['cases']]
    checked['expected_global_lease'] = expected_lease
    return checked


def action_plan(first):
    orders = {'original': ('original', 'load', 'font'), 'load': ('load', 'original', 'font')}
    if first not in orders:
        raise ValueError('Only the two explicit opposite three-route orders accepted')
    return orders[first] * 3


def expected_counters(plan):
    counts = dict(font=0, original=0, load=0)
    output = []
    for lease, route in enumerate(plan, 1):
        if route not in counts:
            raise ValueError('Unknown route')
        counts[route] += 1
        output.append(dict(route=route, run=counts[route], global_lease=lease,
                           expected_font_requests=3 * counts['font']))
    return output
