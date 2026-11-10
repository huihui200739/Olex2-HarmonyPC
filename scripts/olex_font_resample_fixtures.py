"""Labeled synthetic font geometry and ordinary Python ZIP writer; no research data."""
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile

STYLES = ('nr__', 'nrb_', 'ni__', 'nib_', 'fr__', 'frb_', 'fi__', 'fib_')
POINTS = (8, 18, 27, 72)


def glyph_payload(points, style):
    if points not in POINTS or style not in STYLES:
        raise ValueError('Only explicit isolated8/18/27/72 synthetic styles')
    width, height = {8: (6, 8), 18: (12, 16), 27: (18, 24), 72: (48, 64)}[points]
    bits = bytearray(width * height * 32)
    def ink(code, x, y):
        if not (0 <= x < width and 0 <= y < height):
            raise ValueError('Synthetic fixture outside bounded cell')
        index = (code * height + y) * width + x
        bits[index // 8] |= 1 << (index % 8)
    for code in range(33, 256):
        left = 1 + code % 3
        for y in range(2, height - 2):
            slant = (height - 3 - y) // 6 if style[1] == 'i' else 0
            ink(code, left + slant, y)
            if style[2] == 'b':
                ink(code, left + slant + 1, y)
        for x in range(left, width - 2):
            ink(code, x, height - 3)
        for bit in range(8):
            if code & (1 << bit):
                ink(code, min(width - 3, left + 2 + bit % 4), min(height - 3, 3 + bit))
    return struct.pack('<HH', width, height) + bits


class StreamingBuffer(io.BytesIO):
    def seek(self, *args):
        raise OSError('Clearly labeled non-seekable synthetic ZIP output')


def make_archive(*, stored=False, streamed=False, absolute_offsets=True):
    buffer = StreamingBuffer() if streamed else io.BytesIO()
    if absolute_offsets:
        buffer.write(b'ofnt')
    method = zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED
    with zipfile.ZipFile(buffer, 'w', compression=method, compresslevel=9, allowZip64=False) as archive:
        for style in STYLES:
            for points in POINTS:
                info = zipfile.ZipInfo(style + str(points), date_time=(2000, 1, 1, 0, 0, 0))
                info.compress_type = method
                info.create_system = 3
                info.external_attr = 0o100600 << 16
                archive.writestr(info, glyph_payload(points, style))
    data = buffer.getvalue()
    return data if absolute_offsets else b'ofnt' + data


def write_fixtures(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    files = {}
    for name, options in (
        ('synthetic-resample-deflate.fnt', {}),
        ('synthetic-resample-stored-relative.fnt', dict(stored=True, absolute_offsets=False)),
        ('synthetic-resample-streamed.fnt', dict(streamed=True)),
    ):
        data = make_archive(**options)
        (output / name).write_bytes(data)
        files[name] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), options=options)
    data = (output / 'synthetic-resample-deflate.fnt').read_bytes()
    lines = ['#pragma once', '// Isolated synthetic font geometry only. Not a system typeface or scientific data.',
             'namespace olex2::harmony {', 'inline constexpr unsigned char OlexFontResampleFixture[] = {']
    lines += [','.join(str(value) for value in data[start:start + 24]) + ',' for start in range(0, len(data), 24)]
    lines += ['};', '}']
    (output / 'olex_font_resample_fixture.h').write_text('\n'.join(lines) + '\n')
    entries = []
    for style in STYLES:
        for points in POINTS:
            payload = glyph_payload(points, style)
            entries.append(dict(entry=style+str(points), points=points, width=struct.unpack_from('<H',payload)[0],
                height=struct.unpack_from('<H',payload,2)[0], bit_bytes=len(payload)-4,
                payload_sha256=hashlib.sha256(payload).hexdigest()))
    result = dict(scope='Clearly labeled isolated synthetic font color/geometry only; no actual records or research data',
        files=files, entries=entries, original_GL_executed=False, target_executed=False, full_port_complete=False)
    (output / 'manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result
