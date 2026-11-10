"""Isolated MOCKED CLI tests; no HDC, installation or real cold start occurs."""
import contextlib,copy,hashlib,io,json,sys,tempfile,types,unittest
from pathlib import Path
from unittest import mock
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
class StartupCliTests(unittest.TestCase):
    def execute_mocked(self,mode,post=False,connection_failure=False,wrong_device=False):
        raw=json.loads((HERE/'fixtures/gx_memory_observation.json').read_text())
        raw['_fixture_origin']={'kind':'clearly isolated mocked CLI replay; NOT target execution'}
        if post:
            raw['no_decoder_control']['stage']='before-first-image-api'
            raw['post_decode_control']=copy.deepcopy(raw['no_decoder_control']);raw['post_decode_control']['stage']='after-capability-probes'
        data=(json.dumps(raw,ensure_ascii=False)+'\n').encode();digest=hashlib.sha256(data).hexdigest()
        identity=dict(bundleName='com.example.olex2harmonypc',versionName='0.22.5',versionCode=220500)
        with tempfile.TemporaryDirectory(prefix='clearly-synthetic-startup-cli-',dir=ROOT/'out') as directory:
            temp=Path(directory);(temp/'scripts').mkdir();hap=temp/'synthetic.hap';hap.write_bytes(b'clearly isolated synthetic test bytes, NOT a real package')
            active=False;self.mock_install_attempts=[]
            def simulated_command(args,**kwargs):
                nonlocal active
                self.assertEqual(args[1:3],['-t','127.0.0.1:5555'])
                if connection_failure:return types.SimpleNamespace(returncode=0,stdout='[Fail][E001005] Device not found or connected',stderr='')
                operation=args[3:];output=''
                if operation[0]=='install':
                    self.mock_install_attempts.append(operation);output='clearly synthetic mocked install'
                elif operation[:2]==['file','recv']:Path(operation[3]).write_bytes(data)
                elif operation[0]=='shell':
                    text=operation[1]
                    if text.startswith('echo ARCH;'):
                        output='ARCH\naarch64\nTYPE\n'+('phone' if wrong_device else '2in1')+'\nMODEL\nemulator\nAPI\n24\nSYSTEM\nOpenHarmony-6.1.1.125\n'
                    elif text.startswith('bm dump'):output=json.dumps(dict(versionName=identity['versionName'],versionCode=identity['versionCode'],applicationInfo=identity))
                    elif text.startswith('aa force-stop'):active=False
                    elif text.startswith('aa start'):active=True
                    elif text.startswith('pidof'):output=str(raw['pid']) if active else ''
                    elif text.startswith('cat '):output=data.decode()
                    elif text.startswith('sha256sum '):output=digest+'  clearly-synthetic-native-result.json'
                    else:raise AssertionError('Unexpected mocked command '+text)
                else:raise AssertionError('Unexpected mocked operation '+str(operation))
                return types.SimpleNamespace(returncode=0,stdout=output,stderr='')
            provenance=types.ModuleType('build_provenance');provenance.verify=mock.Mock(return_value={'clearly_synthetic_build_provenance':True})
            package=types.ModuleType('renderer_probe_identity');package.package_identity=mock.Mock(return_value=identity)
            argv=['run-gx-startup-audit.py','--target','127.0.0.1:5555','--source',str(temp),'--hap',str(hap),
                  '--fixtures',str(HERE/'fixtures/gx_synthetic_images.json'),'--runs','1',*mode]
            output=io.StringIO();code=(ROOT/'scripts/run-gx-startup-audit.py').read_text()
            with mock.patch.dict(sys.modules,{'build_provenance':provenance,'renderer_probe_identity':package}),mock.patch('subprocess.run',side_effect=simulated_command),mock.patch.object(sys,'argv',argv),contextlib.redirect_stdout(output):
                with self.assertRaises(SystemExit) as ended:exec(compile(code,str(temp/'scripts/runner.py'),'exec'),{'__name__':'__main__','__file__':str(temp/'scripts/runner.py')})
            summary=json.loads(output.getvalue());result=json.loads((Path(summary['report_directory'])/'result.json').read_text())
            return ended.exception.code,result
    def test_mocked_memory_proof_not_overwritten_by_build_provenance_dict(self):
        code,result=self.execute_mocked(['--owned-memory-resource-proof'])
        self.assertEqual(code,0);self.assertTrue(result['passed']);self.assertEqual(result['resource_proof'],'owned-memory')
        self.assertTrue(result['build_provenance']['clearly_synthetic_build_provenance'])
        self.assertFalse(result['runs'][0]['owned_resources']['complete_descriptor_metadata_scan_passed'])
    def test_mocked_full_metadata_proof_still_rejects_missing_access(self):
        code,result=self.execute_mocked(['--require-resource-audit']);self.assertEqual(code,1);self.assertFalse(result['passed'])
    def test_mocked_post_control_cannot_be_omitted(self):
        code,result=self.execute_mocked(['--owned-memory-resource-proof','--require-post-decode-control']);self.assertEqual(code,1);self.assertFalse(result['passed'])
    def test_mocked_zero_exit_hdc_connection_error_is_rejected(self):
        code,result=self.execute_mocked(['--owned-memory-resource-proof'],connection_failure=True)
        self.assertEqual(code,1);self.assertFalse(result['passed']);self.assertEqual(result['runs'],[])
        self.assertIn('Device not found or connected',result['error'])
    def test_mocked_wrong_device_rejected_before_installation(self):
        code,result=self.execute_mocked(['--owned-memory-resource-proof'],wrong_device=True)
        self.assertEqual(code,1);self.assertFalse(result['passed']);self.assertEqual(self.mock_install_attempts,[])
        self.assertIn('ARM64 PC/2in1 API24',result['error'])
    def test_mocked_explicit_pre_and_post_stages(self):
        code,result=self.execute_mocked(['--owned-memory-resource-proof','--require-post-decode-control'],post=True)
        self.assertEqual(code,0);self.assertEqual(result['runs'][0]['post_decode_control']['samples'],41)
if __name__=='__main__':unittest.main()
