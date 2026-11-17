"""Isolated package identities; not installation or runtime evidence."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from renderer_probe_identity import package_identity,installed_identity

class PackageIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='synthetic-renderer-identity-');self.addCleanup(self.temp.cleanup)
        self.hap=Path(self.temp.name)/'synthetic.hap'
        self.identity=dict(bundleName='isolated.synthetic',versionName='synthetic',versionCode=1)
    def package(self,identity):
        with zipfile.ZipFile(self.hap,'w') as package:package.writestr('module.json',json.dumps(dict(app=identity)))
        return package_identity(self.hap)
    def test_explicit_identity_from_package(self):self.assertEqual(self.package(self.identity),self.identity)
    def test_reject_invalid_identity_fields(self):
        for key,value in [('bundleName',''),('bundleName','x; command'),('bundleName','/wrong/path'),('versionName',''),('versionCode',True),('versionCode',-1),('versionCode','1')]:
            with self.subTest(key=key,value=value):
                with self.assertRaises(ValueError):self.package(dict(self.identity,**{key:value}))
    def test_missing_identity_cannot_fallback(self):
        for key in self.identity:
            changed=copy.deepcopy(self.identity);del changed[key]
            with self.assertRaises(KeyError):self.package(changed)
    def test_runtime_metadata_exact(self):self.assertEqual(installed_identity(self.identity),dict(self.identity,error='',source='bundleManager.getBundleInfoForSelfSync'))
