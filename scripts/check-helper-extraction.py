#!/usr/bin/env python3
"""Compare shared helper loop bodies to the pinned glutil.h, ignoring routing/whitespace."""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
from zoneinfo import ZoneInfo
ROOT = Path(__file__).resolve().parents[1]
lock = json.loads((ROOT / 'source.lock.json').read_text())
original = subprocess.check_output(['git', '-C', str(ROOT / 'upstream'), 'show', lock['commit'] + ':glib/glutil.h'], text=True)
shared = (ROOT / 'upstream/glib/primitivesubmit.h').read_text()
def body(source, signature):
    start = source.index('{', source.index(signature)) + 1
    cursor, depth = start, 1
    while depth:
        depth += (source[cursor] == '{') - (source[cursor] == '}')
        cursor += 1
    return source[start:cursor-1]
def normalize(value):
    return re.sub(r'\s+', '', value)
def route(value):
    for gl, topology in [('GL_POLYGON', 'ConvexPolygon'), ('GL_QUADS', 'Quads'), ('GL_QUAD_STRIP', 'QuadStrip')]:
        value = value.replace('olx_gl::begin(' + gl + ')', 'sink.Begin(PrimitiveTopology::' + topology + ')')
    return value.replace('olx_gl::normal(', 'sink.Normal(').replace('olx_gl::vertex(', 'sink.Vertex(').replace('olx_gl::end()', 'sink.End()')
checks = []
for name, signature in [('disks', 'RenderDisks('), ('rims', 'RenderRims('), ('torus', 'Render(double tr')]:
    old = body(original, signature)
    actual = normalize(body(shared, signature))
    expected = normalize(route(old))
    checks.append({'name': name, 'passed': actual == expected,
                   'pinned_original_body_sha256': hashlib.sha256(old.encode()).hexdigest(),
                   'shared_normalized_body_sha256': hashlib.sha256(actual.encode()).hexdigest()})
report = {'checked_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),
          'pinned_commit': lock['commit'], 'source': 'upstream/glib/glutil.h at pinned commit',
          'shared_header': 'upstream/glib/primitivesubmit.h',
          'allowed_difference': 'only backend output calls and whitespace; loop bounds, expressions and batch boundaries unchanged',
          'checks': checks, 'passed': all(c['passed'] for c in checks)}
(ROOT / 'reports/upstream-helper-extraction.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(0 if report['passed'] else 1)
