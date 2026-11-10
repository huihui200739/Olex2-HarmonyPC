"""Clearly isolated synthetic negative audit cases, not actual SDK observations."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from gx_application_validation import validate_image_resource_audit

def synthetic_audit():
    return dict(balanced=True,overflow=False,file=dict(open_attempted=True,opened=True,descriptor=21,open_errno=0,
        identity_known=True,device=24,inode=7000,mode=0o100600,identity_errno=0,close_attempted=True,
        close_code=0,close_errno=0,scan_completed=True,remaining_input_descriptors=0),
        handles=[dict(name=n,creation_observed=True,created=True,creation_code=0,release_attempted=True,release_code=0)
                 for n in ('source','source-info','options','pixelmap','pixel-info')])
class ImageResourceValidationTests(unittest.TestCase):
    def setUp(self):self.audit=synthetic_audit()
    def reject(self):
        with self.assertRaises(ValueError):validate_image_resource_audit(self.audit)
    def test_synthetic_complete(self):self.assertEqual(validate_image_resource_audit(self.audit)['released'],5)
    def test_synthetic_posix_scope_explicitly_has_no_image_handles(self):
        self.audit['handles']=[];self.reject()
        self.assertEqual(validate_image_resource_audit(self.audit,require_image_handles=False)['created'],0)
    def test_synthetic_failed_release(self):self.audit['handles'][3]['release_code']=17;self.reject()
    def test_synthetic_missing_release(self):self.audit['handles'][0]['release_attempted']=False;self.reject()
    def test_synthetic_unobserved_creation(self):self.audit['handles'][0]['creation_observed']=False;self.reject()
    def test_synthetic_close_failure(self):self.audit['file']['close_code']=-1;self.audit['file']['close_errno']=9;self.reject()
    def test_synthetic_remaining_duplicate(self):self.audit['file']['remaining_input_descriptors']=1;self.reject()
    def test_synthetic_missing_scan(self):self.audit['file']['scan_completed']=False;self.reject()
    def test_synthetic_unknown_inode(self):self.audit['file']['identity_known']=False;self.reject()
    def test_synthetic_missing_handle(self):self.audit['handles'].pop();self.reject()
    def test_synthetic_boolean_errno(self):self.audit['file']['close_errno']=False;self.reject()
    def test_synthetic_failed_open(self):
        self.audit['file'].update(opened=False,descriptor=-1,open_errno=2,identity_known=False,device=0,inode=0,mode=0,close_attempted=False,scan_completed=False)
        self.audit['handles']=[];self.assertFalse(validate_image_resource_audit(self.audit)['input_opened'])
    def test_synthetic_failed_open_cannot_own_object(self):
        self.audit['file'].update(opened=False,descriptor=-1,open_errno=2,identity_known=False,device=0,inode=0,mode=0,close_attempted=False,scan_completed=False);self.reject()
    def test_synthetic_bad_handle_order(self):self.audit['handles'].reverse();self.reject()
    def test_synthetic_success_with_null_object(self):
        self.audit['handles'][4].update(created=False,release_attempted=False);self.reject()
    def test_synthetic_success_cannot_skip_pixelmap(self):
        self.audit['handles'][3].update(creation_observed=False,created=False,release_attempted=False)
        with self.assertRaises(ValueError):validate_image_resource_audit(self.audit,['source','source-info','options','pixelmap','pixel-info'])
    def test_synthetic_failed_open_stale_inode(self):
        self.audit['file'].update(opened=False,descriptor=-1,open_errno=2,identity_known=False,close_attempted=False,scan_completed=False);self.audit['handles']=[];self.reject()
    def test_synthetic_audit_overflow(self):self.audit['overflow']=True;self.reject()
    def test_synthetic_socket_not_input_file(self):self.audit['file']['mode']=0o140777;self.reject()

class OwnedMemoryResourceValidationTests(unittest.TestCase):
    """All cases are explicitly synthetic validation fixtures, not SDK runs."""
    def setUp(self):
        self.audit=synthetic_audit()
        self.audit.update(balanced=False,owned_scope_balanced=True,input_kind='owned-memory-buffer',
            encoded_bytes=100,sdk_received_file_descriptor=False,buffer_released_after_handles=True)
        self.audit['file'].update(scan_completed=False,scan_errno=13,scan_stat_failures=1)
    def reject(self):
        with self.assertRaises(ValueError):validate_image_resource_audit(self.audit,proof='owned-memory')
    def test_synthetic_memory_scope_does_not_pass_old_full_scan(self):
        with self.assertRaises(ValueError):validate_image_resource_audit(self.audit)
        self.assertEqual(validate_image_resource_audit(self.audit,proof='owned-memory')['released'],5)
    def test_synthetic_buffer_ends_early(self):self.audit['buffer_released_after_handles']=False;self.reject()
    def test_synthetic_descriptor_handoff_not_allowed(self):self.audit['sdk_received_file_descriptor']=True;self.reject()
    def test_synthetic_empty_memory_input(self):self.audit['encoded_bytes']=0;self.reject()
    def test_synthetic_oversized_memory_input(self):self.audit['encoded_bytes']=64*1024*1024+1;self.reject()
    def test_synthetic_full_scan_cannot_be_faked(self):self.audit['balanced']=True;self.reject()
    def test_synthetic_scan_failure_needs_errno(self):self.audit['file']['scan_errno']=0;self.reject()
    def test_synthetic_wrong_source_kind(self):self.audit['input_kind']='file-descriptor';self.reject()
    def test_synthetic_memory_release_failure(self):self.audit['handles'][3]['release_code']=7;self.reject()
    def test_synthetic_actual_complete_scan_can_be_reported(self):
        self.audit['balanced']=True;self.audit['file'].update(scan_completed=True,scan_errno=0,scan_stat_failures=0)
        self.assertEqual(validate_image_resource_audit(self.audit,proof='owned-memory')['created'],5)

class NoDecoderControlValidationTests(unittest.TestCase):
    def setUp(self):
        self.control=dict(execution_completed=True,samples=[dict(sample=i,elapsed_ms=25*i,count_excluding_scan_descriptor=3,
            identities=['0:19:3:8630','1:19:3:8630','3:fstat-error:13']) for i in range(41)])
    def validate(self):
        from gx_application_validation import validate_no_decoder_control
        return validate_no_decoder_control(self.control)
    def reject(self):
        with self.assertRaises(ValueError):self.validate()
    def test_synthetic_fixed_control(self):self.assertEqual(self.validate()['samples'],41)
    def test_synthetic_count_changes_recorded_not_attributed(self):
        self.control['samples'][10]['identities'].append('29:8:9000:49663');self.control['samples'][10]['count_excluding_scan_descriptor']=4
        result=self.validate();self.assertEqual(result['descriptor_count_changes'],2);self.assertFalse(result['descriptor_ownership_attributed'])
    def test_synthetic_missing_sample(self):self.control['samples'].pop();self.reject()
    def test_synthetic_count_and_rows_disagree(self):self.control['samples'][0]['count_excluding_scan_descriptor']=4;self.reject()
    def test_synthetic_duplicate_identity(self):self.control['samples'][0]['identities'][1]='0:19:3:8630';self.reject()
    def test_synthetic_wrongly_labeled_closed_descriptor(self):self.control['samples'][0]['identities'][2]='3:closed-during-snapshot';self.reject()
    def test_synthetic_short_duration(self):self.control['samples'][-1]['elapsed_ms']=999;self.reject()
    def test_synthetic_error_without_errno(self):self.control['samples'][0]['identities'][2]='3:fstat-error:0';self.reject()

if __name__=='__main__':unittest.main()
