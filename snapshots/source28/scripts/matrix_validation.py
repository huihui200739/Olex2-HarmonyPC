"""Independent analytic check of diagnostic camera state and GPU readback.

No upstream implementation, exported native matrix helper, or report booleans
are used to compute the expected coefficients. Logical model-view and actual
MVP/normal uniform readback have deliberately different evidence labels.
"""
import math

SOURCE = 'upstream/sdl/ebasis.h/.cpp: original TEBasis RotateX/RotateY and owned CopyMData'
TOLERANCE = 2.e-5


def analytic_camera(yaw, pitch, distance, width, height):
    y, x = math.radians(yaw), math.radians(pitch)
    cy, sy, cx, sx = math.cos(y), math.sin(y), math.cos(x), math.sin(x)
    rotation = [[cy, sy*sx, sy*cx, 0], [0, cx, -sx, 0],
                [-sy, cy*sx, cy*cx, 0], [0, 0, 0, 1]]
    model_view = [row[:] for row in rotation]
    model_view[2][3] = -distance
    f = 1/math.tan(.7/2)
    projection = [[f/(width/height), 0, 0, 0], [0, f, 0, 0],
                  [0, 0, -100.1/99.9, -.2*100/99.9], [0, 0, -1, 0]]
    mvp = [[sum(projection[r][k]*model_view[k][c] for k in range(4))
            for c in range(4)] for r in range(4)]
    column_major = lambda a: [a[r][c] for c in range(4) for r in range(4)]
    return dict(zip(('modelViewMatrix', 'mvpMatrix', 'normalMatrix'),
                    map(column_major, (model_view, mvp, rotation))))


def finite_number(value):
    return type(value) in (int, float) and math.isfinite(value)


def original_matrices_match(g):
    """Fail closed for missing, malformed, transposed or stale readback."""
    if not isinstance(g, dict):
        return False
    if not (g.get('basisMatrixVerified') is True and g.get('matrixUniformVerified') is True
            and g.get('basisMatrixSource') == SOURCE
            and type(g.get('modelViewDepth')) is int and g['modelViewDepth'] == 1
            and type(g.get('projectionDepth')) is int and g['projectionDepth'] == 1
            and finite_number(g.get('matrixUniformMaxError'))
            and 0 <= g['matrixUniformMaxError'] <= TOLERANCE):
        return False
    if not all(finite_number(g.get(k)) for k in ('yaw', 'pitch', 'distance')):
        return False
    if not (g['distance'] > 0 and all(type(g.get(k)) is int and g[k] > 0 for k in ('width', 'height'))):
        return False
    try:
        expected = analytic_camera(*(g[k] for k in ('yaw', 'pitch', 'distance', 'width', 'height')))
    except (ValueError, OverflowError, ZeroDivisionError):
        return False
    for key, coefficients in expected.items():
        values = g.get(key)
        if not (isinstance(values, list) and len(values) == 16
                and all(finite_number(a) and abs(a-b) <= TOLERANCE
                        for a, b in zip(values, coefficients))):
            return False
    return True
