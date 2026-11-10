"""Independent actual-display size/binding reference; no screen observations fabricated."""
import math
import struct


def validate_metrics(value):
    if not isinstance(value,dict) or value.get('ready') is not True:
        raise ValueError('Observed main-window DPI is required')
    for key in ('display_id','revision'):
        if type(value.get(key)) is not int or value[key] < (1 if key=='revision' else 0) or value[key]>9007199254740991:
            raise ValueError('Invalid observed display ID/revision')
    for key,maximum in (('dpi_x',8192),('dpi_y',8192),('density_pixels',64)):
        number=value.get(key)
        if type(number) not in (int,float) or not math.isfinite(number) or not 0<number<=maximum:
            raise ValueError('Invalid actual display observation: '+key)
    if not .0625 <= value['dpi_x']/value['dpi_y'] <= 16:
        raise ValueError('Unsupported observed physical pixel aspect')
    if value.get('source')!='actual-main-window-display-xDPI-yDPI' or value.get('fixed_dpi_fallback') is not False:
        raise ValueError('Actual main-window source and no-fallback marker required')
    return value


def float32(value):
    return struct.unpack('<f',struct.pack('<f',value))[0]


def expected_parameters(points,metrics):
    metrics=validate_metrics(metrics)
    if type(points) is not int or not 1<=points<=256:
        raise ValueError('Effective point size outside exact bounded contract')
    size=points*metrics['dpi_y']/72.0
    if not 1<=size<=512:raise ValueError('Actual-DPI raster outside existing pixel bound')
    return dict(pixel_size=float32(size),scale_x=float32(metrics['dpi_x']/metrics['dpi_y']))


def validate_case(case,metrics):
    validate_metrics(metrics)
    if case.get('display_metrics')!=metrics:
        raise ValueError('Font raster came from a different display observation')
    parameters=expected_parameters(case['point_size'],metrics)
    for field,key in (('raster_pixel_size','pixel_size'),('raster_scale_x','scale_x')):
        value=case.get(field)
        if type(value) not in (int,float) or not math.isfinite(value) or float32(value)!=parameters[key]:
            raise ValueError('Exact float32 actual-display raster parameter differs: '+field)
    return parameters


def validate_snapshot(metrics,before,after):
    metrics=validate_metrics(metrics)
    expected=dict(ready=True,displayId=metrics['display_id'],revision=metrics['revision'],
                  dpiX=metrics['dpi_x'],dpiY=metrics['dpi_y'],densityPixels=metrics['density_pixels'],fixedDpiFallback=False)
    for snapshot in (before,after):
        actual=snapshot.get('graphics',{}).get('fontDisplay')
        if actual!=expected:raise ValueError('Actual UI before/after display snapshot does not bind font observation')
    return expected
