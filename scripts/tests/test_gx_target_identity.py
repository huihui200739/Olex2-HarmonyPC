"""Explicitly synthetic public device identity text; never an actual target."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from gx_target_identity import validate_pc_emulator_identity
class TargetIdentityTests(unittest.TestCase):
    def setUp(self):self.text='ARCH\naarch64\nTYPE\n2in1\nMODEL\nemulator\nAPI\n24\nSYSTEM\nOpenHarmony-6.1.1.125\n'
    def reject(self):
        with self.assertRaises(ValueError):validate_pc_emulator_identity(self.text)
    def test_synthetic_correct_pc(self):self.assertEqual(validate_pc_emulator_identity(self.text)['api'],'24')
    def test_synthetic_phone(self):self.text=self.text.replace('2in1','phone');self.reject()
    def test_synthetic_wrong_arch(self):self.text=self.text.replace('aarch64','armv7');self.reject()
    def test_synthetic_physical_device(self):self.text=self.text.replace('emulator','physical');self.reject()
    def test_synthetic_other_api(self):self.text=self.text.replace('\n24\n','\n23\n');self.reject()
    def test_synthetic_ambiguous_marker(self):self.text+='ARCH\naarch64\n';self.reject()
    def test_synthetic_missing_value(self):self.text=self.text.replace('2in1\n','');self.reject()
    def test_synthetic_normal_output_whitespace(self):
        self.text='\n'.join(' '+line+'  ' for line in self.text.splitlines());self.assertEqual(validate_pc_emulator_identity(self.text)['device_type'],'2in1')
if __name__=='__main__':unittest.main()
