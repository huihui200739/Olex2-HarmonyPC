"""Isolated evidence-recording guards; synthetic records are not emulator observations."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from native_font_progress_validation import (
    IDENTITY, bound, canonical, check_bound, instruction_words,
    revalidate_window, validate_disassembly)


class NativeFontProgressTests(unittest.TestCase):
    SAMPLE = '  1000: 0e20cc63 fmla v3.2s, v3.2s, v0.2s\n  1004: d65f03c0 ret\n'

    def write(self, root, value):
        path = Path(root) / 'result.json'
        path.write_text(json.dumps(value))
        return path

    def window(self):
        return dict(passed=True, identity=copy.deepcopy(IDENTITY),
                    target='127.0.0.1:5555', hap_sha256='synthetic-hash',
                    full_port_complete=False, functional_completion_percent=None,
                    full_ui_qualification=False, rounds=[])

    def reject(self, value):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):
                revalidate_window(self.write(folder, value), 'synthetic-hash')

    def test_bound_exact_bytes_and_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bytes'
            path.write_bytes(b'\x00synthetic\xff')
            record = bound(path, folder)
            self.assertEqual(record['bytes'], 11)
            self.assertEqual(check_bound(record, folder), path)

    def test_changed_bound_artifact_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bytes'
            path.write_bytes(b'old')
            record = bound(path, folder)
            path.write_bytes(b'new')
            with self.assertRaises(ValueError):
                check_bound(record, folder)

    def test_outside_evidence_path_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'inside'
            root.mkdir()
            path = Path(folder) / 'outside'
            path.write_text('synthetic')
            with self.assertRaises(ValueError):
                bound(path, root)

    def test_disassembly_instruction_headers_are_ignored(self):
        other = '/synthetic/other/output.so: file format elf64\n' + self.SAMPLE
        result = validate_disassembly(self.SAMPLE, other, 0x1000, 0x1008)
        self.assertEqual(result['instructions'], 2)
        self.assertEqual(result['viewport_fmla_address'], 0x1000)

    def test_changed_actual_opcode_rejected(self):
        with self.assertRaises(ValueError):
            validate_disassembly(self.SAMPLE, self.SAMPLE.replace('0e20cc63', '0e20cc62'), 0x1000, 0x1008)

    def test_function_range_mismatch_rejected(self):
        with self.assertRaises(ValueError):
            validate_disassembly(self.SAMPLE, self.SAMPLE, 0x1000, 0x1004)

    def test_missing_float32_vector_fmla_rejected(self):
        changed = self.SAMPLE.replace('.2s', '.2d')
        with self.assertRaises(ValueError):
            validate_disassembly(changed, changed, 0x1000, 0x1008)

    def test_empty_disassembly_rejected(self):
        self.assertEqual(instruction_words('no instructions'), [])
        with self.assertRaises(ValueError):
            validate_disassembly('', '', 0x1000, 0x1008)

    def test_wrong_candidate_rejected(self):
        value = self.window()
        value['identity']['versionName'] = '0.22.16'
        self.reject(value)

    def test_wrong_emulator_rejected(self):
        value = self.window()
        value['target'] = 'unselected-synthetic-target'
        self.reject(value)

    def test_wrong_hap_rejected(self):
        value = self.window()
        value['hap_sha256'] = 'different-synthetic-hash'
        self.reject(value)

    def test_partial_font_cannot_be_full_ui_qualification(self):
        value = self.window()
        value['full_ui_qualification'] = True
        self.reject(value)

    def test_partial_font_cannot_be_full_port_or_percent(self):
        value = self.window()
        value['full_port_complete'] = True
        value['functional_completion_percent'] = 100
        self.reject(value)

    def test_exactly_three_actual_rounds_required(self):
        self.reject(self.window())
        value = self.window()
        value['rounds'] = [{}, {}]
        self.reject(value)

    def test_canonical_rejects_nonfinite_values(self):
        with self.assertRaises(ValueError):
            canonical(dict(synthetic=float('nan')))


if __name__ == '__main__':
    unittest.main()
