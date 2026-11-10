"""Clearly synthetic summary metadata; never used as actual runtime evidence."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from original_renderer_integration_validation import summarize_application,validate_application,ROUNDTRIPS

class IntegrationSummaryTests(unittest.TestCase):
    def setUp(self):
        self.identity=dict(bundleName='isolated.synthetic',versionName='synthetic',versionCode=1)
        original=json.loads((ROOT/'scripts/original_ui_checks.json').read_text())['checks']
        checks=[dict(name=n,passed=True,details={}) for n in original]
        checks.append(dict(name='shared_named_controls_unclipped',passed=True,details={}))
        for index,label in enumerate(ROUNDTRIPS):
            checks.append(dict(name='shared_original_'+label,passed=True,details=dict(passed=True,
                full_port_complete=False,functional_completion_percent=None,validated_identity=self.identity,
                pid=str(min(index+1,4)),run=max(1,index-2),
                checks=[dict(name='explicit-synthetic-check-'+str(n),passed=True) for n in range(126)])))
        self.app=dict(checks=checks,result='passed: EXPLICIT SYNTHETIC SUMMARY, NOT A DEVICE RUN')
    def summary(self):return summarize_application(self.app,ROOT,self.identity)
    def detail(self,index):return next(i['details'] for i in self.app['checks'] if i['name']=='shared_original_'+ROUNDTRIPS[index])
    def reject(self):
        with self.assertRaises((ValueError,KeyError)):self.summary()
    def test_valid_synthetic_summary(self):self.assertEqual(self.summary()['original_ui_checks_preserved'],187)
    def test_original_check_removed(self):self.app['checks'].pop(0);self.reject()
    def test_original_order_changed(self):self.app['checks'][0],self.app['checks'][1]=self.app['checks'][1],self.app['checks'][0];self.reject()
    def test_missing_new_round(self):self.app['checks'].pop();self.reject()
    def test_wrong_identity(self):self.detail(0)['validated_identity']=dict(self.identity,versionCode=2);self.reject()
    def test_wrong_same_pid_cold_restart(self):self.detail(1)['pid']=self.detail(0)['pid'];self.reject()
    def test_wrong_different_pid_repeat(self):self.detail(5)['pid']='999';self.reject()
    def test_wrong_repeated_run_sequence(self):self.detail(5)['run']=1;self.reject()
    def test_missing_subcheck(self):self.detail(0)['checks'].pop();self.reject()
    def test_duplicate_subcheck(self):self.detail(0)['checks'][0]['name']=self.detail(0)['checks'][1]['name'];self.reject()
    def test_failed_subcheck(self):self.detail(0)['checks'][0]['passed']=False;self.reject()
    def test_false_full_completion_claim(self):self.detail(0)['full_port_complete']=True;self.reject()
    def test_false_percentage_claim(self):self.detail(0)['functional_completion_percent']=100;self.reject()
    def test_failed_result(self):self.app['result']='failed';self.reject()

    def synthetic_json_comparison(self,changed=False):
        # No filesystem/device evidence is fabricated: this isolated unit only
        # exercises Python -> JSON type normalization before exact comparison.
        self.app['report_directory']='reports/app/explicit-synthetic-unused-path'
        self.detail(0)['checks'][0]['detail']={'compiled':{'1':1},'values':[[1,2]]}
        results=[copy.deepcopy(self.detail(i)) for i in range(6)]
        results[0]['checks'][0]['detail']={'compiled':{1:0 if changed else 1},'values':[(1,2)]}
        with patch('original_renderer_validation.validate',side_effect=results):
            return validate_application(self.app,ROOT,self.identity)

    def test_exact_json_types_are_not_false_mismatches(self):
        self.assertTrue(self.synthetic_json_comparison()['passed'])

    def test_json_normalization_does_not_ignore_changed_values(self):
        with self.assertRaises(ValueError):self.synthetic_json_comparison(changed=True)
