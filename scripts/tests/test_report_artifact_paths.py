"""Isolated synthetic path fixtures, never build artifacts or real summaries."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from report_artifact_paths import baseline_artifact_paths


class BaselineArtifactPathTests(unittest.TestCase):
    def test_hap_remains_in_app_directory(self):
        self.assertEqual(baseline_artifact_paths(['out/app/entry-default-unsigned.hap'],'out/fresh',['olex2_fixture']),['out/app/entry-default-unsigned.hap','out/fresh/olex2_fixture'])

    def test_native_libraries_and_executables_use_fresh_directory(self):
        self.assertEqual(baseline_artifact_paths(['out/old/libolex2_fixture.a','out/old/olex2_fixture'],'out/fresh',['olex2_fixture']),['out/fresh/libolex2_fixture.a','out/fresh/olex2_fixture'])

    def test_old_and_current_names_deduplicate(self):
        self.assertEqual(baseline_artifact_paths(['out/old/olex2_fixture','out/fresh/olex2_fixture'],'out/fresh',['olex2_fixture','olex2_fixture']),['out/fresh/olex2_fixture'])

    def test_plan_is_idempotent(self):
        once=baseline_artifact_paths(['out/app/entry-default-unsigned.hap','out/old/libolex2_fixture.a'],'out/fresh',['olex2_fixture'])
        self.assertEqual(baseline_artifact_paths(once,'out/fresh',['olex2_fixture']),once)

    def test_unknown_or_outside_paths_rejected(self):
        for previous,build,names in [(['out/other.json'],'out/fresh',['olex2_fixture']),([], '../out/fresh',['olex2_fixture']),([], '/out/fresh',['olex2_fixture']),([], 'out/fresh',['../olex2_fixture']),(['/tmp/olex2_fixture'],'out/fresh',['olex2_fixture'])]:
            with self.subTest(previous=previous,build=build,names=names), self.assertRaises(ValueError):
                baseline_artifact_paths(previous,build,names)


if __name__ == '__main__':
    unittest.main()
