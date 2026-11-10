"""Isolated synthetic contract fixtures, never edits actual runtime evidence."""
from copy import deepcopy
import hashlib
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import line_validation as line

def wire(kind='sphere',detail=0):
    count,digest=line.expected_line_digest(kind,detail)
    return dict(meshKind=kind,meshGeneration=detail,meshIndices=count,wireframe=True,surfaceReady=True,initialized=True,
        originalLineSource=line.SOURCE,originalLineVerified=True,originalLineBufferVerified=True,
        originalLineVertices=count,originalLineColorCommands=0,originalLineWidthQueries=0,originalLineWidthCommands=0,originalLineIndexSha256=digest)

def solid():
    g=wire();g.update(wireframe=False,originalLineVerified=False,originalLineBufferVerified=False,originalLineVertices=0,originalLineIndexSha256='');return g

def fixture():
    return dict(result='passed: explicitly synthetic fixture',checks=[dict(name='original_line_branch_drives_actual_wire_upload',passed=True,details=wire())],samples=[wire() for _ in range(24)]+[solid() for _ in range(120)])

class LineValidationTests(unittest.TestCase):
    def test_independent_sphere_zero_bytes(self):
        indices=[n+d for n in range(0,24,3) for d in (0,1,1,2,2,0)]
        self.assertEqual(line.expected_line_digest('sphere',0),(48,hashlib.sha256(struct.pack('<48I',*indices)).hexdigest()))
    def test_all_24_expected_counts(self):
        for detail in range(6):
            t=8*(detail+1)
            for kind,count in dict(sphere=48*4**detail,torus=4*t*t,disks=12*t,rims=24*t+8).items():
                self.assertEqual(line.expected_line_digest(kind,detail)[0],count)
                self.assertTrue(line.original_line_upload_matches(wire(kind,detail)))
    def test_solid_explicitly_has_no_line_claim(self):self.assertTrue(line.original_line_upload_matches(solid()))
    def test_reject_wrong_digest(self):
        g=wire();g['originalLineIndexSha256']='0'*64;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_missing_readback(self):
        g=wire();g['originalLineBufferVerified']=False;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_count_change(self):
        g=wire();g['originalLineVertices']-=2;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_false_scalar_types(self):
        g=wire();g['originalLineColorCommands']=False;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_wide_line_claim(self):
        g=wire();g['originalLineWidthCommands']=2;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_solid_stale_line_proof(self):
        g=solid();g['originalLineBufferVerified']=True;self.assertFalse(line.original_line_upload_matches(g))
    def test_reject_invalid_detail(self):
        for bad in (True,1.0,-1,6,'1'):
            with self.assertRaises(ValueError):line.expected_line_digest('sphere',bad)
    def test_reject_unknown_kind(self):
        with self.assertRaises(ValueError):line.expected_line_digest('other',0)
    def test_log_all_24_independently_matches(self):
        rows=['LINE_INDICES %s %d %d %s'%(kind,detail,*line.expected_line_digest(kind,detail)) for kind in ('sphere','torus','disks','rims') for detail in range(6)]
        text='\n'.join(rows)+'\nPASS original primitive line commands; 104199 assertions; 640 branch-state cases; 24 source-edge meshes; synthetic fixture\n'
        self.assertTrue(line.line_command_log_matches(text));self.assertFalse(line.line_command_log_matches(text+'\n'+rows[0]));self.assertFalse(line.line_command_log_matches(text.replace(rows[0],'')))
    def test_full_app_fixture(self):self.assertTrue(line.app_line_evidence_matches(fixture()))
    def test_reject_hidden_active_failure(self):
        app=fixture();app['samples'][20]['originalLineIndexSha256']='f'*64;self.assertFalse(line.app_line_evidence_matches(app))
    def test_reject_false_named_success(self):
        app=fixture();app['checks'][0]['passed']=False;self.assertFalse(line.app_line_evidence_matches(app))
    def test_reject_missing_named_check(self):
        app=fixture();app['checks']=[];self.assertFalse(line.app_line_evidence_matches(app))
    def test_inactive_requires_cleared_current_proof(self):
        app=fixture();g=wire();g.update(surfaceReady=False,initialized=False,originalLineVerified=False,originalLineBufferVerified=False,originalLineIndexSha256='');app['inactive']=g
        self.assertTrue(line.app_line_evidence_matches(app));g['originalLineBufferVerified']=True;self.assertFalse(line.app_line_evidence_matches(app))
    def test_reject_ambiguous_surface(self):
        app=fixture();app['samples'][0]['surfaceReady']=False;self.assertFalse(line.app_line_evidence_matches(app))

if __name__=='__main__':unittest.main()
