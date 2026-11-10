"""Isolated synthetic matrix reports; never edit retained runtime evidence."""
import copy
import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from matrix_validation import SOURCE, analytic_camera, original_matrices_match

class MatrixValidationTests(unittest.TestCase):
    def fixture(self, yaw=32, pitch=-22, distance=4.4, width=900, height=700):
        g = dict(synthetic=True, yaw=yaw, pitch=pitch, distance=distance, width=width, height=height,
                 basisMatrixVerified=True, matrixUniformVerified=True, basisMatrixSource=SOURCE,
                 modelViewDepth=1, projectionDepth=1, matrixUniformMaxError=0)
        g.update(analytic_camera(yaw,pitch,distance,width,height))
        return g
    def test_analytic_pose_grid(self):
        for y in (-170,-90,0,32,180):
            for p in (-80,-22,0,45,80):
                for w,h in ((700,900),(900,700),(1000,1000)):
                    self.assertTrue(original_matrices_match(self.fixture(y,p,4.4,w,h)))
    def test_missing_values(self):
        for key in self.fixture():
            if key=='synthetic': continue
            g=self.fixture();del g[key];self.assertFalse(original_matrices_match(g),key)
    def test_transposed_mvp(self):
        g=self.fixture();v=g['mvpMatrix'];g['mvpMatrix']=[v[r*4+c] for c in range(4) for r in range(4)]
        self.assertFalse(original_matrices_match(g))
    def test_wrong_rotation_order(self):
        g=self.fixture(); y,x=map(math.radians,(g['yaw'],g['pitch']));cy,sy,cx,sx=math.cos(y),math.sin(y),math.cos(x),math.sin(x)
        g['normalMatrix']=[cy,sx*sy,-cx*sy,0, 0,cx,sx,0, sy,-sx*cy,cx*cy,0, 0,0,0,1]
        self.assertFalse(original_matrices_match(g))
    def test_wrong_pitch_sign(self):
        g=self.fixture();g.update(analytic_camera(32,22,4.4,900,700));self.assertFalse(original_matrices_match(g))
    def test_stale_gpu_uniform_despite_flags(self):
        for key in ('mvpMatrix','normalMatrix','modelViewMatrix'):
            g=self.fixture();g[key][6]+=.01;self.assertFalse(original_matrices_match(g))
    def test_normal_must_exclude_translation(self):
        g=self.fixture();g['normalMatrix']=g['modelViewMatrix'];self.assertFalse(original_matrices_match(g))
    def test_nonfinite_or_boolean_coefficients(self):
        for bad in (float('nan'),float('inf'),True,'0',None):
            g=self.fixture();g['mvpMatrix'][0]=bad;self.assertFalse(original_matrices_match(g))
    def test_invalid_pose_dimensions(self):
        for key,bad in (('yaw',float('nan')),('pitch',float('inf')),('distance',0),('height',0),('width',True)):
            g=self.fixture();g[key]=bad;self.assertFalse(original_matrices_match(g))
    def test_inaccurate_uniform_or_unverified_state(self):
        for key,bad in (('matrixUniformMaxError',.1),('matrixUniformMaxError',-1),('matrixUniformVerified',False),('basisMatrixVerified',False),('modelViewDepth',2),('basisMatrixSource','synthetic-wrong-source')):
            g=self.fixture();g[key]=bad;self.assertFalse(original_matrices_match(g))
    def test_small_float_roundoff_allowed(self):
        g=self.fixture();g['mvpMatrix'][0]+=1.e-6;self.assertTrue(original_matrices_match(g))

if __name__=='__main__':unittest.main()
