"""Labelled, isolated synthetic UI-state fixtures; not device observations."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from picker_readiness import SEARCH_ID,wait_for_source_or_focused_search
URI='file://synthetic-isolated-fixture/own-export.options'
SOURCE=dict(id=URI,type='Text',visible='true')
SEARCH=dict(id=SEARCH_ID,type='TextInput',visible='true',enabled='true',focused='false')
class ReadinessTests(unittest.TestCase):
 def run_fixture(self,frames,timeout=2):
  now=[0.];events=[]
  def inspect(i):events.append(('inspect',i));return frames[min(i,len(frames)-1)]
  def focus(nodes):events.append(('focus',len(nodes)))
  def pause(t):now[0]+=t
  result=wait_for_source_or_focused_search(frames[0],URI,inspect,focus,timeout=timeout,clock=lambda:now[0],pause=pause)
  return result,events
 def test_exact_visible_source_needs_no_search(self):
  (nodes,result),events=self.run_fixture([[SOURCE]]);self.assertEqual(nodes,[SOURCE]);self.assertEqual(result['state'],'exact-exported-uri-visible');self.assertEqual(events,[])
 def test_visible_search_without_focus_is_not_ready(self):
  (_,result),events=self.run_fixture([[SEARCH],[dict(SEARCH,focused='true')]]);self.assertEqual(result['observations'],2);self.assertEqual(result['state'],'search-focus-observed');self.assertEqual(events[0][0],'focus')
 def test_loaded_file_can_appear_after_header(self):
  (_,result),events=self.run_fixture([[SEARCH],[SEARCH,SOURCE]]);self.assertEqual(result['state'],'exact-exported-uri-visible');self.assertEqual(result['observations'],2)
 def test_wrong_file_cannot_replace_exact_uri(self):
  with self.assertRaises(RuntimeError):self.run_fixture([[dict(SOURCE,id=URI+'-wrong')]],timeout=.5)
 def test_unfocused_search_fails_by_deadline(self):
  with self.assertRaises(RuntimeError):self.run_fixture([[SEARCH]],timeout=.5)
 def test_duplicate_search_fails_closed(self):
  with self.assertRaises(RuntimeError):self.run_fixture([[SEARCH,SEARCH]])
 def test_disabled_focused_search_is_not_ready(self):
  with self.assertRaises(RuntimeError):self.run_fixture([[dict(SEARCH,enabled='false',focused='true')]],timeout=.5)
 def test_hidden_exact_source_is_not_ready(self):
  with self.assertRaises(RuntimeError):self.run_fixture([[dict(SOURCE,visible='false')]],timeout=.5)
if __name__=='__main__':unittest.main(verbosity=2)
