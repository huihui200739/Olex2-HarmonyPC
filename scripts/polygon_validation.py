"""Check retained original polygon provenance and isolated actual FBO pixels.

The simple concave U contour below is explicitly synthetic. This verifies one
GPU fixture and actual disk-helper source routing, not all polygons/GLU.
"""
SOURCE = 'upstream/glib/polygonsubmit.h: original TGlPrimitive::Draw sgloPolygon branch'
PROBE_SOURCE = 'synthetic U polygon -> original sgloPolygon -> simple-planar tessellation -> original sgloTriangles -> actual main-context GLES3 RGBA8 FBO readback'
CONTOUR = ((-.8,-.8),(.8,-.8),(.8,.8),(.2,.8),(.2,-.2),(-.2,-.2),(-.2,.8),(-.8,.8))
POINTS = ((12,44),(51,44),(32,12),(32,48),(32,62),(32,1))


def inside_contour(x,y):
    # Independent ray crossings; not ear clipping or a native success flag.
    inside=False
    for a,b in zip(CONTOUR,CONTOUR[1:]+CONTOUR[:1]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            inside=not inside
    return inside


def polygon_probe_matches(g):
    if not isinstance(g,dict) or not (g.get('polygonProbeVerified') is True and g.get('polygonProbeStateRestored') is True and g.get('polygonProbeContextRestored') is True and g.get('polygonProbeSource')==PROBE_SOURCE):
        return False
    for key,value in (('polygonProbeInputVertices',8),('polygonProbeTriangles',6),('polygonProbeNormalCommands',1),('polygonProbeColorCommands',0)):
        if type(g.get(key)) is not int or g[key]!=value:return False
    samples=g.get('polygonProbeSamples')
    if not isinstance(samples,list) or len(samples)!=6:return False
    for sample,(x,y) in zip(samples,POINTS):
        if not isinstance(sample,list) or len(sample)!=7 or not all(type(v) is int for v in sample):return False
        actual_x,actual_y,reported_inside,*rgba=sample
        inside=inside_contour((x+.5)/32-1,(y+.5)/32-1)
        if (actual_x,actual_y)!=(x,y) or reported_inside!=int(inside):return False
        expected=(64,191,128,255) if inside else (0,0,0,255)
        if not all(0<=v<=255 and abs(v-e)<=1 for v,e in zip(rgba,expected)) or rgba[3]!=255:return False
    return True


def original_polygon_upload_matches(g):
    if not isinstance(g,dict) or g.get('originalPolygonSource')!=SOURCE:return False
    keys=('originalPolygonBatches','originalPolygonVertices','originalPolygonNormalCommands','originalPolygonColorCommands')
    if not all(type(g.get(k)) is int for k in keys):return False
    if g.get('meshKind')=='disks':
        level=g.get('meshGeneration')
        return (type(level) is int and level in range(6) and g.get('originalPolygonVerified') is True
                and tuple(g[k] for k in keys)==(6,6*(8*(level+1)+1),6,0))
    return g.get('meshKind') in ('sphere','torus','rims') and g.get('originalPolygonVerified') is False and all(g[k]==0 for k in keys)
