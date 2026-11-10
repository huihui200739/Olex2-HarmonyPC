"""Isolated synthetic logging fixtures only; never alters runtime evidence."""
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from app_report_log import parse_app_reports
class FragmentReportTests(unittest.TestCase):
    def parts(self,value,key='10-0',size=15):
        text=json.dumps(value,ensure_ascii=False)
        fragments=[text[i:i+size] for i in range(0,len(text),size)]
        return [f'prefix OLEX2_APP_FRAGMENT {key} {i} {len(fragments)} {part}' for i,part in enumerate(fragments)]
    def test_plain_report(self):self.assertEqual(parse_app_reports('x OLEX2_APP_REPORT {"synthetic":true}'),[{'synthetic':True}])
    def test_complete_fragmented_unicode(self):
        value={'synthetic':'中文状态'*70};self.assertEqual(parse_app_reports('\n'.join(self.parts(value))),[value])
    def test_partial_fragment_not_emitted(self):self.assertEqual(parse_app_reports('\n'.join(self.parts({'synthetic':123})[:-1])),[])
    def test_inconsistent_counts_rejected(self):
        lines=self.parts({'synthetic':123});lines[0]=lines[0].replace(' 0 2 ',' 0 3 ');self.assertEqual(parse_app_reports('\n'.join(lines)),[])
    def test_reordered_parts_and_exact_duplicates(self):
        value={'synthetic':True,'payload':'x'*35};lines=self.parts(value);self.assertEqual(parse_app_reports('\n'.join([lines[-1]]+list(reversed(lines))+[lines[0]])),[value])
    def test_conflicting_duplicate_rejected(self):
        lines=self.parts({'synthetic':True});self.assertEqual(parse_app_reports('\n'.join([lines[0],lines[0]+'X']+lines[1:])),[])
    def test_corrupt_json_and_invalid_bounds(self):
        self.assertEqual(parse_app_reports('OLEX2_APP_REPORT broken\nOLEX2_APP_FRAGMENT 1-0 0 0 {}\nOLEX2_APP_FRAGMENT 2-0 0 65 {}'),[])
    def test_distinct_reports_return_completion_order(self):
        a={'synthetic':1};b={'synthetic':2};self.assertEqual(parse_app_reports('\n'.join(self.parts(a,'11-0')+self.parts(b,'12-0'))),[a,b])
if __name__=='__main__':unittest.main()
