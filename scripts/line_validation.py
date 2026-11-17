"""Independent source-edge index contracts. No native helper execution/import.

Only uncolored multiplier-one sgloLines drives the current actual EBO. This
validator says nothing about GPU wide-line/color/normal-state equivalence.
"""
import hashlib
import struct
from functools import lru_cache

SOURCE='upstream/glib/linesubmit.h: original TGlPrimitive::Draw sgloLines branch'

@lru_cache(maxsize=24,typed=True)
def expected_line_digest(kind, detail):
    if type(detail) is not int or detail not in range(6):
        raise ValueError('detail must be an integer in 0..5')
    triangles=[]
    edges=[]
    t=8*(detail+1)
    def quad(a,b,c,d):
        triangles.extend((a,b,c,a,c,d))
        edges.extend((a,b,b,c,c,d,d,a))
    if kind=='sphere':
        # The source sphere is already expanded into independent triangles.
        for base in range(0,24*4**detail,3):
            edges.extend((base,base+1,base+1,base+2,base+2,base))
    elif kind=='torus':
        # Original strips keep separate raw vertex identities across batches;
        # coincident positions are not merged and diagonals are not edges.
        for batch in range(t//2):
            base=batch*2*(t+1)
            for pair in range(t):
                a=base+2*pair
                quad(a,a+1,a+3,a+2)
    elif kind=='rims':
        base=0
        for raw_count in (4*t+2,4*t+2,4*t+4):
            for offset in range(0,raw_count-3,4):
                a=base+offset
                quad(a,a+1,a+2,a+3)
            base+=raw_count
    elif kind=='disks':
        for face in range(6):
            base=face*(t+1)
            for i in range(1,t-1):
                triangles.extend((base,base+i,base+i+1))
            for i in range(t):
                edges.extend((base+i,base+(i+1)%t))
    else:
        raise ValueError('unknown source primitive')
    if triangles:
        first={}
        for i,raw in enumerate(triangles):
            first.setdefault(raw,i)
        indices=[first[raw] for raw in edges]
    else:
        indices=edges
    data=struct.pack('<'+'I'*len(indices),*indices)
    return len(indices),hashlib.sha256(data).hexdigest()

def original_line_upload_matches(g):
    if not isinstance(g,dict) or type(g.get('wireframe')) is not bool:
        return False
    if g.get('originalLineSource')!=SOURCE:
        return False
    # Solid uploads and inactive surfaces must not retain current-line proof.
    if not g['wireframe']:
        return (g.get('originalLineVerified') is False and g.get('originalLineBufferVerified') is False
            and g.get('originalLineIndexSha256')=='' and all(type(g.get(k)) is int and g[k]==0 for k in
                ('originalLineVertices','originalLineColorCommands','originalLineWidthQueries','originalLineWidthCommands')))
    try:
        count,digest=expected_line_digest(g.get('meshKind'),g.get('meshGeneration'))
    except (ValueError,TypeError):
        return False
    return (g.get('originalLineVerified') is True and g.get('originalLineBufferVerified') is True
        and type(g.get('meshIndices')) is int and g['meshIndices']==count
        and type(g.get('originalLineVertices')) is int and g['originalLineVertices']==count
        and g.get('originalLineIndexSha256')==digest and all(type(g.get(k)) is int and g[k]==0 for k in
            ('originalLineColorCommands','originalLineWidthQueries','originalLineWidthCommands')))

def inactive_line_proof_cleared(g):
    return (g.get('originalLineVerified') is False and g.get('originalLineBufferVerified') is False
        and g.get('originalLineIndexSha256')=='')

def line_command_log_matches(text):
    import re
    rows=re.findall(r'^LINE_INDICES (sphere|torus|disks|rims) ([0-5]) (\d+) ([0-9a-f]{64})$',text,re.M)
    keys={(kind,int(detail)) for kind,detail,_,_ in rows}
    return (len(rows)==24 and len(keys)==24 and
        all((int(count),digest)==expected_line_digest(kind,int(detail)) for kind,detail,count,digest in rows) and
        'PASS original primitive line commands; 104199 assertions; 640 branch-state cases; 24 source-edge meshes;' in text and 'FAIL ' not in text)

def graphic_snapshots(value,path=()):
    if isinstance(value,dict):
        if all(k in value for k in ('meshKind','wireframe','surfaceReady','initialized')):
            yield path,value
        for key,child in value.items():
            yield from graphic_snapshots(child,path+(key,))
    elif isinstance(value,list):
        for i,child in enumerate(value):
            yield from graphic_snapshots(child,path+(i,))

def app_line_evidence_matches(app):
    if not isinstance(app,dict) or not app.get('result','').startswith('passed:'):
        return False
    checks=app.get('checks',[])
    named=[c for c in checks if c.get('name')=='original_line_branch_drives_actual_wire_upload']
    if len(named)!=1 or named[0].get('passed') is not True or not named[0].get('details',{}).get('wireframe') or not original_line_upload_matches(named[0]['details']):
        return False
    active=[]
    for _,g in graphic_snapshots(app):
        if g.get('surfaceReady') is True and g.get('initialized') is True:
            if not original_line_upload_matches(g):return False
            active.append(g)
        elif g.get('surfaceReady') is False and g.get('initialized') is False:
            if not inactive_line_proof_cleared(g):return False
        else:
            return False
    return len(active)>=120 and sum(g['wireframe'] for g in active)>=20
