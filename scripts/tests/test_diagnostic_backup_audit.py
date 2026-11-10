"""Isolated synthetic backup fixtures; not emulator or application evidence."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from diagnostic_backup_audit import (REMOTE_FILES, audit_local_backups,
                                    readonly_device_query, audit_device_inventory, create_owned_directory)


class DiagnosticBackupAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='isolated-synthetic-backup-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.report = self.root / 'reports/emulator/isolated-synthetic-success/result.json'
        self.report.parent.mkdir(parents=True)
        self.report.write_text('{"passed": true}')
        self.folder = self.report.parent / 'runtime/font-1'
        self.folder.mkdir(parents=True)
        self.directory = 'original-font-pid-123-run-1'
        self.sidecar = 'shared-original-font-run-1.json'
        (self.folder / 'synthetic.rgba').write_bytes(b'isolated synthetic pixels')
        (self.folder / self.sidecar).write_text(json.dumps(dict(run=1, original=dict(
            pid=123, run=1, artifact_subdirectory=self.directory))))
        self.receipt = dict(pid=123, run=1, artifact_subdirectory=self.directory, files={})
        for name in ('synthetic.rgba', self.sidecar):
            raw = (self.folder / name).read_bytes()
            digest = hashlib.sha256(raw).hexdigest()
            self.receipt['files'][name] = dict(bytes=len(raw),sha256=digest,target_sha256=digest)
        self.save_receipt()

    def save_receipt(self):
        (self.folder / 'received-artifacts.json').write_text(json.dumps(self.receipt))

    def audit(self):
        return audit_local_backups(self.root, [self.report])

    def inventory(self, audit, pid='456'):
        row = audit['directories'][0]
        lines = ['__AUDIT_PID__', pid, '__AUDIT_DF__', 'Filesystem 1K-blocks Used Available Use% Mounted',
                 '/isolated-synthetic 1000 900 100 90% /data', '__AUDIT_LINKS__', '__AUDIT_HASHES__']
        if not pid:lines.remove('')
        lines += [f"{v['sha256']}  {row['remote']}/{k}" for k,v in row['files'].items()]
        return '\n'.join(lines + ['__AUDIT_STATUS__0,0', ''])

    def test_matching_local_and_device_receipts_only_authorize_nothing(self):
        audit = self.audit()
        self.assertEqual((audit['directory_count'], audit['file_count']), (1, 1))
        self.assertNotIn(self.sidecar, audit['directories'][0]['files'])
        checked = audit_device_inventory(audit, self.inventory(audit))
        self.assertTrue(checked['exact_device_file_sets_verified'])
        self.assertFalse(checked['deletion_authorized'])
        self.assertEqual(checked['deleted_files'], 0)

    def test_failed_report_cannot_supply_candidates(self):
        self.report.write_text('{"passed": false}')
        with self.assertRaises(ValueError):self.audit()

    def test_truthy_success_is_not_boolean_success(self):
        self.report.write_text('{"passed": 1}')
        with self.assertRaises(ValueError):self.audit()

    def test_changed_local_backup_rejected(self):
        (self.folder/'synthetic.rgba').write_bytes(b'changed')
        with self.assertRaises(ValueError):self.audit()

    def test_missing_local_backup_rejected(self):
        (self.folder/'synthetic.rgba').unlink()
        with self.assertRaises(ValueError):self.audit()

    def test_original_hash_contradiction_rejected(self):
        self.receipt['files']['synthetic.rgba']['target_sha256'] = '0'*64
        self.save_receipt()
        with self.assertRaises(ValueError):self.audit()

    def test_unowned_directory_and_pid_mismatch_rejected(self):
        for name in ('olex2-runtime-v1', 'original-font-pid-124-run-1', '../original-font-pid-123-run-1'):
            self.receipt['artifact_subdirectory'] = name;self.save_receipt()
            with self.assertRaises(ValueError):self.audit()

    def test_symlink_file_and_ancestor_rejected(self):
        source=self.folder/'synthetic.rgba';target=self.root/'elsewhere';target.write_bytes(source.read_bytes())
        source.unlink();source.symlink_to(target)
        with self.assertRaises(ValueError):self.audit()
        source.unlink();source.write_bytes(target.read_bytes())
        moved=self.folder.parent/'moved';self.folder.rename(moved);self.folder.symlink_to(moved,target_is_directory=True)
        with self.assertRaises(ValueError):self.audit()

    def test_path_traversal_or_noncanonical_name_rejected(self):
        for name in ('../escape','a//b','./synthetic.rgba','/tmp/file','a/./b','a b'):
            changed=copy.deepcopy(self.receipt);changed['files'][name]=changed['files'].pop('synthetic.rgba')
            (self.folder/'received-artifacts.json').write_text(json.dumps(changed))
            with self.assertRaises(ValueError):self.audit()

    def test_observation_identity_mismatch_rejected_even_with_valid_hash(self):
        p=self.folder/self.sidecar;p.write_text(json.dumps(dict(run=1,original=dict(pid=124,run=1,artifact_subdirectory=self.directory))))
        raw=p.read_bytes();digest=hashlib.sha256(raw).hexdigest()
        self.receipt['files'][self.sidecar]=dict(bytes=len(raw),sha256=digest,target_sha256=digest);self.save_receipt()
        with self.assertRaises(ValueError):self.audit()

    def test_other_application_identity_rejected(self):
        self.receipt['identity']=dict(bundleName='isolated.other.application');self.save_receipt()
        with self.assertRaises(ValueError):self.audit()

    def test_current_process_directory_rejected(self):
        audit=self.audit()
        with self.assertRaises(ValueError):audit_device_inventory(audit,self.inventory(audit,pid='123'))

    def test_no_current_process_can_be_observed(self):
        audit=self.audit();self.assertEqual(audit_device_inventory(audit,self.inventory(audit,pid=''))['current_processes'],[])

    def test_extra_missing_duplicate_or_changed_remote_file_rejected(self):
        audit=self.audit();valid=self.inventory(audit);line=valid.splitlines()[-2]
        invalid=[valid.replace(line+'\n',''),valid.replace(line,line+'\n'+line),
                 valid.replace(line,line+'\n'+'0'*64+'  '+REMOTE_FILES+'/'+self.directory+'/extra.rgba'),
                 valid.replace(line,'0'*64+line[64:])]
        for text in invalid:
            with self.assertRaises(ValueError):audit_device_inventory(audit,text)

    def test_incomplete_failed_or_duplicated_remote_observation_rejected(self):
        audit=self.audit();valid=self.inventory(audit)
        for text in (valid.replace('__AUDIT_STATUS__0,0','__AUDIT_STATUS__1,0'),valid+'truncated\n',
                     valid.replace('__AUDIT_HASHES__','__AUDIT_HASHES__\n__AUDIT_HASHES__'),
                     valid.replace('__AUDIT_PID__','missing'),valid.replace('__AUDIT_LINKS__','__AUDIT_LINKS__\n/a/link')):
            with self.assertRaises(ValueError):audit_device_inventory(audit,text)

    def test_query_contains_only_owned_readonly_file_operations(self):
        text=readonly_device_query(self.audit())
        self.assertIn(f'{REMOTE_FILES}/{self.directory}',text)
        for forbidden in ('rm ', 'install ', 'force-stop', 'uninstall', 'olex2-runtime', 'reset', 'pushd'):
            self.assertNotIn(forbidden,text)
        self.assertIn('-exec sha256sum {} +',text)

    def test_empty_or_tampered_query_rejected(self):
        with self.assertRaises(ValueError):readonly_device_query(dict(directories=[]))
        audit=self.audit();audit['directories'][0]['remote']='/data/other'
        with self.assertRaises(ValueError):readonly_device_query(audit)

    def test_report_outside_owned_evidence_or_project_rejected(self):
        other=self.root/'unrelated/result.json';other.parent.mkdir();other.write_text('{"passed": true}')
        with self.assertRaises(ValueError):audit_local_backups(self.root,[other])
        with self.assertRaises(ValueError):audit_local_backups(self.root,[Path('/tmp/unrelated/result.json')])

    def test_new_output_is_project_owned_and_never_overwrites(self):
        relative=Path('out/isolated-synthetic-audit')
        output=create_owned_directory(self.root,relative)
        self.assertEqual(output,self.root/relative)
        with self.assertRaises(FileExistsError):create_owned_directory(self.root,relative)

    def test_output_cannot_traverse_or_leave_project(self):
        for path in (Path('../outside'),Path('out/../../outside'),Path('/tmp/outside'),self.root):
            with self.assertRaises(ValueError):create_owned_directory(self.root,path)

    def test_output_cannot_follow_existing_symlink(self):
        target=self.root/'other';target.mkdir();(self.root/'link').symlink_to(target,target_is_directory=True)
        with self.assertRaises(ValueError):create_owned_directory(self.root,Path('link/new'))
        self.assertFalse((target/'new').exists())

    def test_receipts_require_individual_qualification(self):
        (self.folder.parent/'result.json').write_text('{"passed": false}')
        with self.assertRaises(ValueError):self.audit()


if __name__ == '__main__':unittest.main()
