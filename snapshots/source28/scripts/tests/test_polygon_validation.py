"""Isolated synthetic polygon reports only; no retained evidence is modified."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from polygon_validation import SOURCE,PROBE_SOURCE,POINTS,inside_contour,polygon_probe_matches,original_polygon_upload_matches
class PolygonValidationTests(unittest.TestCase):
    def fixture(self):
        samples=[]
        for i,(x,y) in enumerate(POINTS):samples.append([x,y,int(i<3),*([64,191,128,255] if i<3 else [0,0,0,255])])
        return dict(synthetic=True,polygonProbeVerified=True,polygonProbeContextRestored=True,polygonProbeStateRestored=True,polygonProbeSource=PROBE_SOURCE,polygonProbeInputVertices=8,polygonProbeTriangles=6,polygonProbeNormalCommands=1,polygonProbeColorCommands=0,polygonProbeSamples=samples)
    def test_independent_pixel_fixture(self):self.assertTrue(polygon_probe_matches(self.fixture()))
    def test_notch_must_stay_background_not_fan_fill(self):
        g=self.fixture();g['polygonProbeSamples'][3][3:]=[64,191,128,255];self.assertFalse(polygon_probe_matches(g))
    def test_inside_must_actually_draw(self):
        g=self.fixture();g['polygonProbeSamples'][0][3:]=[0,0,0,255];self.assertFalse(polygon_probe_matches(g))
    def test_flags_alone_are_not_evidence(self):
        g=self.fixture();del g['polygonProbeSamples'];self.assertFalse(polygon_probe_matches(g))
    def test_state_restore_required(self):
        g=self.fixture();g['polygonProbeStateRestored']=False;self.assertFalse(polygon_probe_matches(g))
    def test_application_context_must_be_verified(self):
        g=self.fixture();g['polygonProbeContextRestored']=False;self.assertFalse(polygon_probe_matches(g))
    def test_sample_coordinates_and_labels_not_trusted(self):
        for k,v in ((0,32),(2,0),(3,True),(6,254)):
            g=self.fixture();g['polygonProbeSamples'][0][k]=v;self.assertFalse(polygon_probe_matches(g))
    def test_source_and_counts_required(self):
        for k,v in (('polygonProbeSource','synthetic-wrong-source'),('polygonProbeTriangles',7),('polygonProbeInputVertices',True),('polygonProbeColorCommands',1)):
            g=self.fixture();g[k]=v;self.assertFalse(polygon_probe_matches(g))
    def test_ray_crossing_notch_and_arms(self):
        self.assertTrue(inside_contour(-.6,.4));self.assertTrue(inside_contour(.6,.4));self.assertTrue(inside_contour(0,-.5));self.assertFalse(inside_contour(0,.5));self.assertFalse(inside_contour(0,.9))
    def disk(self,level):return dict(synthetic=True,meshKind='disks',meshGeneration=level,originalPolygonVerified=True,originalPolygonSource=SOURCE,originalPolygonBatches=6,originalPolygonVertices=6*(8*(level+1)+1),originalPolygonNormalCommands=6,originalPolygonColorCommands=0)
    def test_disk_source_all_levels(self):
        for level in range(6):self.assertTrue(original_polygon_upload_matches(self.disk(level)))
    def test_wrong_disk_counters_rejected(self):
        for k,v in (('originalPolygonVertices',0),('originalPolygonBatches',True),('originalPolygonColorCommands',1),('originalPolygonVerified',False)):
            g=self.disk(3);g[k]=v;self.assertFalse(original_polygon_upload_matches(g))
    def test_nonpolygon_helpers_cannot_claim_original_polygon(self):
        g=self.disk(3);g.update(meshKind='torus');self.assertFalse(original_polygon_upload_matches(g));g.update(originalPolygonVerified=False,originalPolygonBatches=0,originalPolygonVertices=0,originalPolygonNormalCommands=0);self.assertTrue(original_polygon_upload_matches(g))
if __name__=='__main__':unittest.main()
