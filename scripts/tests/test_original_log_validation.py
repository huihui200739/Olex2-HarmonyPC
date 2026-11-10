"""Clearly labelled, isolated synthetic log fixtures; never real runtime evidence."""
import copy
import hashlib
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from original_log_validation import *


class OriginalLogValidationTests(unittest.TestCase):
    def setUp(self):
        lines=[]
        for kind,count in [('macros',135),('functions',38)]:
            rows=[f'Synthetic_{i} - Synthetic_{i} arguments [none]\n' for i in range(count)]
            for i,row in enumerate(rows):lines.append(f'CATALOG_LINE {kind} {i} {row.encode().hex()}')
            raw=''.join(rows).encode();lines.append(f'CATALOG_DIGEST {kind} {count} {len(raw)} {hashlib.sha256(raw).hexdigest()}')
        lines.append(f'PASS original log bridge; {NATIVE_ASSERTIONS} assertions; 5 original severity cases; 2 real core catalog macros; 400 locally serialized caller posts; synthetic fixture only')
        self.raw='\n'.join(lines);self.reference=parse_catalog_log(self.raw)

    def audit(self,kind=None):
        a=dict(attached=True,posted=0,dropped=0,truncated=0,captureFailures=0,clearCount=0,clearedEntries=0,entryCount=0,retainedBytes=0,textSha256=EMPTY_HASH,source=LOG_SOURCE,error='',firstText='',lastText='')
        if kind:
            r=self.reference[kind];a.update(posted=r['entry_count'],entryCount=r['entry_count'],retainedBytes=r['bytes'],textSha256=r['sha256'],firstText=r['first'],lastText=r['last'])
        return a

    def sample(self,kind='macros'):
        a=self.audit(kind);return dict(synthetic=True,log=a,catalog=dict(sequence=1,passed=True,exposedMacros=2,postedBefore=0,postedAfter=a['posted'],input='core.LstMac' if kind=='macros' else 'core.LstFun',error='',source=CATALOG_SOURCE),visible_text=self.reference[kind]['text'])

    def test_synthetic_reference_two_catalogs(self):self.assertEqual((self.reference['macros']['entry_count'],self.reference['functions']['entry_count']),(135,38))
    def test_missing_ordinal_rejected(self):
        with self.assertRaises(ValueError):parse_catalog_log(self.raw.replace('CATALOG_LINE macros 0 ','CATALOG_LINE macros 1 ',1))
    def test_mutated_raw_bytes_rejected(self):
        with self.assertRaises(ValueError):parse_catalog_log(self.raw.replace('53796e746865746963','53796e746865746965',1))
    def test_native_count_not_inferred_from_digest(self):
        with self.assertRaises(ValueError):parse_catalog_log(self.raw.replace('PASS original log bridge; 1654','PASS original log bridge; 1653'))
    def test_duplicate_declared_digest_rejected(self):
        with self.assertRaises(ValueError):parse_catalog_log(self.raw+'\n'+next(line for line in self.raw.splitlines() if line.startswith('CATALOG_DIGEST macros')))
    def test_both_actual_catalog_samples_match(self):
        for kind in ['macros','functions']:self.assertTrue(catalog_sample_matches(self.sample(kind),self.reference,kind))
    def test_unknown_source_rejected(self):
        a=self.audit();a['source']='synthetic replacement source';self.assertFalse(audit_matches(a))
    def test_failed_capture_rejected(self):
        a=self.audit();a['captureFailures']=1;self.assertFalse(audit_matches(a))
    def test_accounting_hole_rejected(self):
        a=self.audit('macros');a['posted']+=1;self.assertFalse(audit_matches(a))
    def test_boolean_counter_rejected(self):
        a=self.audit();a['posted']=False;self.assertFalse(audit_matches(a))
    def test_entry_and_byte_budgets_required(self):
        for key,value in [('entryCount',257),('retainedBytes',65537)]:
            a=self.audit('macros');a[key]=value;self.assertFalse(audit_matches(a))
    def test_empty_snapshot_digest_and_text_required(self):
        for key,value in [('textSha256','0'*64),('firstText','synthetic leftover'),('retainedBytes',1)]:
            a=self.audit();a[key]=value;self.assertFalse(empty_audit_matches(a))
    def test_utf8_sample_budget_required(self):
        a=self.audit('macros');a['firstText']='汉'*1400;self.assertFalse(audit_matches(a))
        a=self.audit('macros');a['firstText']='\ud800';self.assertFalse(audit_matches(a))
    def test_catalog_macro_set_is_exactly_two(self):
        d=self.sample();d['catalog']['exposedMacros']=3;self.assertFalse(catalog_sample_matches(d,self.reference,'macros'))
    def test_success_flag_cannot_hide_missing_posts(self):
        d=self.sample();d['catalog']['postedAfter']-=1;self.assertFalse(catalog_sample_matches(d,self.reference,'macros'))
    def test_wrong_literal_original_macro_rejected(self):
        d=self.sample();d['catalog']['input']='core.Synthetic';self.assertFalse(catalog_sample_matches(d,self.reference,'macros'))
    def test_visible_text_is_independently_hashed(self):
        d=self.sample();self.assertTrue(visible_catalog_matches(d,self.reference,'macros'));d['visible_text']+='synthetic extra';self.assertFalse(visible_catalog_matches(d,self.reference,'macros'))
    def test_clear_preserves_sequence_and_accounts_removed_entries(self):
        before=self.audit('macros');after=self.audit();after.update(posted=135,clearedEntries=135,clearCount=1);d=dict(before=before,after=after);self.assertTrue(cleared_sample_matches(d));after['posted']=0;self.assertFalse(cleared_sample_matches(d))
    def test_restart_requires_new_process_and_empty_catalog_state(self):
        d=dict(log=self.audit(),catalog=dict(sequence=0,passed=False,exposedMacros=0,postedBefore=0,postedAfter=0,input='',error='',source=''),prior_pid='100',new_pid='101');self.assertTrue(restart_sample_matches(d));d['new_pid']='100';self.assertFalse(restart_sample_matches(d))
    def test_restart_cannot_replay_old_logs(self):
        d=dict(log=self.audit('macros'),catalog=dict(sequence=0,passed=False,exposedMacros=0,postedBefore=0,postedAfter=0,input='',error='',source=''),prior_pid='100',new_pid='101');self.assertFalse(restart_sample_matches(d))


    def app_fixture(self):
        m=self.sample('macros');f=self.sample('functions');f['log'].update(posted=173,clearCount=1,clearedEntries=135);f['catalog'].update(sequence=2,postedBefore=135,postedAfter=173)
        empty=self.audit();cleared=self.audit();cleared.update(posted=135,clearCount=1,clearedEntries=135)
        initial=dict(sequence=0,passed=False,exposedMacros=0,postedBefore=0,postedAfter=0,input='',error='',source='')
        details=[dict(log=empty,catalog=initial),m,copy.deepcopy(m),dict(before=m['log'],after=cleared),dict(log=cleared,visible_text='本次会话暂无原版日志'),f,copy.deepcopy(f),dict(log=f['log'],graphics=dict(surfaceReady=True,initialized=True)),dict(log=empty,catalog=initial,prior_pid='100',new_pid='101')]
        return dict(synthetic=True,result='passed: isolated synthetic fixtures only',checks=[dict(name=name,passed=True,details=copy.deepcopy(d)) for name,d in zip(SAMPLE_NAMES,details)])
    def test_complete_synthetic_app_evidence_matches(self):self.assertTrue(app_original_log_evidence_matches(self.app_fixture(),self.reference))
    def test_failed_app_cannot_reuse_good_log_records(self):
        a=self.app_fixture();a['result']='failed: isolated fixture';self.assertFalse(app_original_log_evidence_matches(a,self.reference))
    def test_duplicate_app_sample_rejected(self):
        a=self.app_fixture();a['checks'].append(copy.deepcopy(a['checks'][0]));self.assertFalse(app_original_log_evidence_matches(a,self.reference))
    def test_app_visible_bytes_cannot_be_replaced_by_success_flag(self):
        a=self.app_fixture();a['checks'][2]['details']['visible_text']='synthetic fabricated';self.assertFalse(app_original_log_evidence_matches(a,self.reference))
    def test_app_clear_sequence_reset_rejected(self):
        a=self.app_fixture();a['checks'][3]['details']['after']['posted']=0;self.assertFalse(app_original_log_evidence_matches(a,self.reference))
    def test_app_navigation_must_retain_same_notification_bytes(self):
        a=self.app_fixture();a['checks'][7]['details']['log']['textSha256']='0'*64;self.assertFalse(app_original_log_evidence_matches(a,self.reference))

if __name__=='__main__':unittest.main()
