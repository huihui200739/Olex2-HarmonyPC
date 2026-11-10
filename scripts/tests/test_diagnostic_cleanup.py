"""Isolated synthetic cleanup checks; never connect to a device."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import diagnostic_cleanup as cleanup
from diagnostic_backup_audit import audit_local_backups
import test_diagnostic_backup_audit as audit_fixtures

class DiagnosticCleanupTests(unittest.TestCase):
    def setUp(self):
        self.base=audit_fixtures.DiagnosticBackupAuditTests('test_matching_local_and_device_receipts_only_authorize_nothing')
        self.base.setUp();self.addCleanup(self.base.doCleanups)
        self.audit=self.base.audit();self.row=self.audit['directories'][0]
        self.plan=dict(target=cleanup.TARGET,bundle_name=cleanup.BUNDLE,local_backups_must_be_retained=True,
                       failed_diagnostics_must_be_retained=True,current25_diagnostics_must_be_retained=True,
                       directory_count=1,file_count=self.audit['file_count'],payload_bytes=self.audit['payload_bytes'],
                       candidates=[{k:self.row[k] for k in ('directory','remote','bytes','receipt','receipt_sha256')}])
        self.plan['candidates'][0]['file_count']=len(self.row['files'])
        self.digest=hashlib.sha256(json.dumps(self.plan).encode()).hexdigest()
        self.auth=dict(authorized=True,user_request=cleanup.USER_REQUEST,plan_sha256=self.digest,target=cleanup.TARGET,
                       bundle_name=cleanup.BUNDLE,retain_local_backups=True)
    def test_exact_authorization(self):
        self.assertTrue(cleanup.validate_authorization(self.plan,self.digest,self.auth,self.audit))
    def test_missing_or_truthy_authorization(self):
        for key,value in [('authorized',False),('authorized',1),('target','other'),('plan_sha256','0'*64),('user_request','继续做'),('retain_local_backups',False)]:
            changed=dict(self.auth);changed[key]=value
            with self.assertRaises(ValueError):cleanup.validate_authorization(self.plan,self.digest,changed,self.audit)
    def test_changed_plan_or_expanded_candidates(self):
        for key,value in [('candidates',[]),('directory_count',2),('file_count',2),('payload_bytes',0),('local_backups_must_be_retained',False)]:
            changed=dict(self.plan);changed[key]=value
            with self.assertRaises(ValueError):cleanup.validate_authorization(changed,self.digest,self.auth,self.audit)
    def test_arbitrary_paths_and_hashes_rejected(self):
        for key,value in [('directory','olex2-runtime-v1'),('remote','/data/other'),('pid',456),('file_count',3)]:
            row=dict(self.row);row[key]=value
            with self.assertRaises(ValueError):cleanup.deletion_script(row)
        for name in ('../escape','x//y','/tmp/out','x;rm'):
            row=copy.deepcopy(self.row);row['files']={name:next(iter(row['files'].values()))}
            with self.assertRaises(ValueError):cleanup.deletion_script(row)
    def test_no_recursive_wildcard_or_local_cleanup(self):
        script=cleanup.deletion_script(self.row)
        self.assertNotIn('rm -',script);self.assertNotIn('find -delete',script)
        self.assertNotIn(str(self.base.root),script);self.assertIn('sha256sum "$file"',script)
        self.assertIn('rmdir ',script);self.assertIn('pidof ',script)
    def receipt(self):
        return '\n'.join([f'__DELETED__{self.row["remote"]}/{name}' for name in sorted(self.row['files'])]+[
            f'__DIRECTORY_REMOVED__{self.row["remote"]}','__CLEANUP_STATUS__0',''])
    def test_exact_deletion_receipt(self):self.assertEqual(len(cleanup.parse_deletion(self.row,self.receipt(),0)),1)
    def test_failure_or_incomplete_receipt(self):
        for text,code in [(self.receipt(),1),(self.receipt().replace('__CLEANUP_STATUS__0',''),0),(self.receipt()+'unexpected\n',0),('',0)]:
            with self.assertRaises(ValueError):cleanup.parse_deletion(self.row,text,code)
    def test_foreign_or_duplicate_deletion_rejected(self):
        for text in [self.receipt().replace('synthetic.rgba','other'),self.receipt().splitlines()[0]+'\n'+self.receipt()]:
            with self.assertRaises(ValueError):cleanup.parse_deletion(self.row,text,0)
    def test_preservation_requires_complete_digest_listing(self):
        good='__OPAQUE_RUNTIME_METADATA__\n1:4096:700:123:456 '+cleanup.REMOTE_FILES+'/olex2-runtime-v1\n__PROTECTED_FILE_HASHES__\n'+'0'*64+'  '+cleanup.REMOTE_FILES+'/readable.txt\n__PRESERVATION_STATUS__0\n'
        self.assertEqual(len(cleanup.parse_preservation(good)['readable_files']),1)
        for text in ['',good.replace('__PRESERVATION_STATUS__0',''),good.replace(cleanup.REMOTE_FILES,'/other'),good.replace('__PRESERVATION_STATUS__0',good.splitlines()[3]+'\n__PRESERVATION_STATUS__0')]:
            with self.assertRaises(ValueError):cleanup.parse_preservation(text)
    def test_runtime_explicitly_pruned_and_not_hashed(self):
        script=cleanup.preservation_script(self.audit)
        self.assertIn('-path '+cleanup.REMOTE_FILES+'/olex2-runtime-v1',script)
        self.assertIn('stat -c ',script)
        self.assertNotIn('chmod',script)
    def test_debug_commands_only_exact_files_and_empty_approved_parents(self):
        remove,dirs=cleanup.app_file_commands(self.row)
        self.assertEqual(remove,['rm',cleanup.APP_FILES+'/'+self.row['directory']+'/synthetic.rgba'])
        self.assertEqual(dirs,['rmdir',cleanup.APP_FILES+'/'+self.row['directory']])
        self.assertNotIn('-r',remove);self.assertNotIn('-f',remove)
    def test_debug_commands_reject_foreign_directory(self):
        row=dict(self.row,remote='/data/other')
        with self.assertRaises(ValueError):cleanup.app_file_commands(row)
    def test_debug_nested_parents_deepest_first(self):
        row=copy.deepcopy(self.row);row['files']['a/b/c.rgba']=row['files'].pop('synthetic.rgba')
        _,dirs=cleanup.app_file_commands(row)
        base=cleanup.APP_FILES+'/'+row['directory']
        self.assertEqual(dirs,['rmdir',base+'/a/b',base+'/a',base])
    def run_isolated(self,mode):
        with tempfile.TemporaryDirectory(prefix='isolated-cleanup-') as folder:
            root=Path(folder);directory=root/self.row['directory'];directory.mkdir();file=directory/'synthetic.rgba'
            file.write_bytes(b'isolated synthetic pixels' if mode!='changed' else b'changed')
            protected=root/'olex2-runtime-v1';protected.mkdir();(protected/'data').write_bytes(b'keep')
            if mode=='symlink':
                file.unlink();file.symlink_to(protected/'data')
            row=copy.deepcopy(self.row);row['remote']=str(directory)
            with patch.object(cleanup,'REMOTE_FILES',str(root)):
                script=cleanup.deletion_script(row).replace(f'pidof {cleanup.BUNDLE}',f'printf {123 if mode=="active" else 456}')
            run=subprocess.run(['/bin/sh','-c',script],capture_output=True,text=True)
            self.assertEqual((protected/'data').read_bytes(),b'keep')
            self.assertTrue((self.base.folder/'synthetic.rgba').is_file())
            if mode=='success':self.assertEqual(run.returncode,0);self.assertFalse(directory.exists())
            else:self.assertNotEqual(run.returncode,0);self.assertTrue(file.exists())
    def test_isolated_exact_unlink_retains_backups_and_runtime(self):self.run_isolated('success')
    def test_isolated_changed_file_not_deleted(self):self.run_isolated('changed')
    def test_isolated_symlink_not_deleted(self):self.run_isolated('symlink')
    def test_isolated_active_pid_not_deleted(self):self.run_isolated('active')

if __name__=='__main__':unittest.main()
