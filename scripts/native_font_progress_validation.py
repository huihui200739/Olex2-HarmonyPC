"""Offline revalidation of exact19 font evidence; never controls the emulator."""
import hashlib
import json
import re
from pathlib import Path

from layout_validation import attributes, controls_unclipped, rectangle
from native_font_raster_window_validation import IDENTITY, validate

ROOT = Path(__file__).resolve().parents[1]


def require(value, message):
    if not value:
        raise ValueError(message)


def canonical(value):
    return json.loads(json.dumps(value, ensure_ascii=False, allow_nan=False))


def bound(path, root=ROOT):
    path, root = Path(path).resolve(), Path(root).resolve()
    require(path.is_relative_to(root), 'Evidence path escapes this project')
    data = path.read_bytes()
    return dict(path=path.relative_to(root).as_posix(), bytes=len(data),
                sha256=hashlib.sha256(data).hexdigest())


def check_bound(record, root=ROOT):
    actual = bound(Path(root) / record['path'], root)
    require(actual == record, 'Bound evidence changed: ' + record['path'])
    return Path(root) / record['path']


def visible(layout):
    ok, details = controls_unclipped(layout)
    nodes = list(attributes(layout))
    panels = [x for x in nodes if x.get('id') == 'olex2-viewport-panel']
    panel = rectangle(panels[0].get('bounds')) if len(panels) == 1 else None
    for identifier in ('olex2-original-texture-load-probe', 'olex2-original-font-probe'):
        found = [x for x in nodes if x.get('id') == identifier]
        node = found[0] if len(found) == 1 else {}
        box = rectangle(node.get('bounds'))
        passed = bool(node.get('visible') == 'true' and node.get('enabled') == 'true'
                      and node.get('type') == 'Button' and box
                      and box == rectangle(node.get('origBounds')) and panel
                      and panel[0] <= box[0] < box[2] <= panel[2]
                      and panel[1] <= box[1] < box[3] <= panel[3])
        details[identifier] = dict(passed=passed, bounds=box,
                                   original_bounds=rectangle(node.get('origBounds')))
        ok = ok and passed
    return ok, details


def instruction_words(text):
    """Only instruction addresses/words; headers and path labels are not instructions."""
    return [(int(a, 16), int(w, 16)) for a, w in
            re.findall(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+', text, re.M)]


def validate_disassembly(retained, fresh, start, end):
    a, b = instruction_words(retained), instruction_words(fresh)
    require(a and a == b, 'Actual DSO disassembly differs from retained evidence')
    require(a[0][0] == start and all(start <= address < end for address, _ in a),
            'Disassembly escapes the bound raster function')
    fmla = re.findall(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+fmla\s+v\d+\.2s,\s*v\d+\.2s,\s*v\d+\.2s\s*$', fresh, re.M)
    require(len(fmla) == 1, 'Expected actual binary32 vector viewport FMLA')
    return dict(instructions=len(a), viewport_fmla_address=int(fmla[0][0], 16),
                retained_instruction_words_equal_actual_DSO=True)


def revalidate_window(report, hap_sha):
    report = Path(report).resolve()
    raw = json.loads(report.read_text())
    require(raw['passed'] is True and raw['identity'] == IDENTITY,
            'Only successful exact19 font window evidence accepted')
    require(raw['target'] == '127.0.0.1:5555' and raw['hap_sha256'] == hap_sha,
            'Font target/HAP differs')
    require(raw['full_port_complete'] is False and
            raw['functional_completion_percent'] is None and
            raw['full_ui_qualification'] is False,
            'Partial font proof cannot certify full UI or full adaptation')
    require(len(raw['rounds']) == 3, 'Exactly three actual font rounds required')
    previous = None
    initial_pid = None
    results = []
    evidence = [bound(report)]
    for number, recorded in enumerate(raw['rounds'], 1):
        folder = (report.parent / 'runtime' / ('font-' + str(number))).resolve()
        require(ROOT / recorded['folder'] == folder, 'Font round directory differs')
        before = folder.with_name(folder.name + '-before')
        after = folder.with_name(folder.name + '-after')
        before_ok, before_details = visible(json.loads((before / 'layout.json').read_text()))
        after_ok, after_details = visible(json.loads((after / 'layout.json').read_text()))
        require(before_ok and after_ok, 'Original/new controls are clipped')
        current = validate(folder, previous)
        before_state = json.loads((before / 'native-verification.json').read_text())
        after_state = json.loads((after / 'native-verification.json').read_text())
        observed = json.loads((folder / ('shared-original-font-run-' + str(number) + '.json')).read_text())
        pid = observed['original']['pid']
        if initial_pid is None:
            initial_pid = pid
        current['layout_before'] = before_details
        current['layout_after'] = after_details
        current['checks'] += [
            dict(name='all original and new controls remain fully visible', passed=bool(after_ok)),
            dict(name='saved workspace and original initialized runtime preserved',
                 passed=before_state['workspaceState'] == after_state['workspaceState']
                 and before_state['applicationRuntime'] == after_state['applicationRuntime']),
            dict(name='all font rounds use same actual PID', passed=pid == initial_pid)]
        current['passed'] = all(check['passed'] for check in current['checks'])
        current['folder'] = folder.relative_to(ROOT).as_posix()
        require(current['passed'] and canonical(current) == recorded,
                'Independent raw font verification differs in round ' + str(number))
        require(current['run'] == number and current['pid'] == pid,
                'Font run/PID differs')
        results.append(current)
        for capture in (before, after):
            captured_pid = (capture / 'pid.txt').read_text().strip()
            require(captured_pid == str(pid), 'Captured runtime PID differs')
            evidence += [bound(capture / name) for name in
                         ('layout.json', 'native-verification.json', 'pid.txt',
                          'process-scoped-report.log')]
        evidence += [bound(folder / 'received-artifacts.json')]
        previous = folder
    require(sum(r['actual_context_continuation_verified'] for r in results) == 2,
            'Two actual context continuations required')
    for name in ('native_font_raster_window_validation.py', 'native_font_window_validation.py'):
        require((report.parent / name).read_bytes() == (ROOT / 'scripts' / name).read_bytes(),
                'Captured analyzer changed: ' + name)
        evidence.append(bound(report.parent / name))
    return dict(passed=True, identity=IDENTITY, hap_sha256=hap_sha,
                target=raw['target'], device_identity=raw['device_identity'], pid=initial_pid,
                rounds=results, checks=sum(len(r['checks']) for r in results),
                actual_context_continuations=2, full_port_complete=False,
                functional_completion_percent=None, full_ui_qualification=False,
                evidence=evidence)
