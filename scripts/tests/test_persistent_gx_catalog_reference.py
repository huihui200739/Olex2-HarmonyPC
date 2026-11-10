"""Real CPU metadata fixture plus explicitly mutated isolated rejection cases."""
import hashlib,re,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from persistent_gx_catalog_reference import parse_catalog_log,MARKER
class GenuineGXCatalogParser(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.text=(Path(__file__).parent/'fixtures/persistent_gx_catalog_host.log').read_text()
 def rejected(self,text):
  with self.assertRaises(ValueError):parse_catalog_log(text)
 def test_actual_cpu_reference(self):
  d=parse_catalog_log(self.text);self.assertEqual((d['macros']['entry_count'],d['functions']['entry_count']),(196,52))
  for value in d.values():self.assertEqual(hashlib.sha256(value['text'].encode()).hexdigest(),value['sha256'])
 def test_missing_or_duplicate_completion(self):
  self.rejected(self.text.replace(MARKER,''));self.rejected(self.text+'\n'+MARKER+'\n')
 def test_root_is_not_recursive_inventory(self):
  self.rejected(self.text.replace('root=193 recursive=196','root=196 recursive=196'))
  self.rejected(self.text.replace('root=46 recursive=52','root=46 recursive=46'))
 def test_actual_notification_accounting(self):
  for field in ('dropped=0','truncated=0','posted=196','entries=196','bytes=13859'):
   self.rejected(self.text.replace(field,field.split('=')[0]+'=1',1))
 def test_missing_observation(self):self.rejected(re.sub(r'^GX_CATALOG_OBSERVATION macros .*\n','',self.text,flags=re.M))
 def test_missing_ordinal(self):self.rejected(re.sub(r'^GX_CATALOG_LINE macros 195 .*\n','',self.text,flags=re.M))
 def test_duplicate_ordinal(self):self.rejected(self.text.replace('GX_CATALOG_LINE macros 1 ','GX_CATALOG_LINE macros 0 ',1))
 def test_noncanonical_ordinal_order(self):
  a=re.search(r'^GX_CATALOG_LINE macros 0 .*$',self.text,re.M)[0];b=re.search(r'^GX_CATALOG_LINE macros 1 .*$',self.text,re.M)[0]
  self.rejected(self.text.replace(a,'TEMP_ROW',1).replace(b,a,1).replace('TEMP_ROW',b,1))
 def test_changed_native_digest(self):self.rejected(self.text.replace('1d4d55f328bb554e58f27a3def809ece27dac3b78ca854227b75db22acaa4f2f','0'*64))
 def test_invalid_utf8(self):self.rejected(re.sub(r'(^GX_CATALOG_LINE macros 0 )[^\n]+',r'\g<1>ff',self.text,flags=re.M))
 def test_signature_framing(self):self.rejected(re.sub(r'(^GX_CATALOG_LINE macros 0 )[^\n]+',r'\g<1>616263',self.text,flags=re.M))
 def test_old_tx_prefix_not_accepted(self):self.rejected(self.text.replace('GX_CATALOG_LINE','CATALOG_LINE'))
 def test_wrong_host_assertion_marker(self):self.rejected(self.text.replace('276 assertions','275 assertions'))
 def test_extra_catalog_row(self):self.rejected(self.text+'GX_CATALOG_LINE functions 52 6162630a\n')
if __name__=='__main__':unittest.main()
