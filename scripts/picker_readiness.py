"""Observe actual picker readiness before sending any text, without a bypass."""
import time
SEARCH_ID='FileManager_SearchBarComp_Input'
def source_visible(nodes,uri):
 return any(n.get('id')==uri and n.get('type')=='Text' and n.get('visible')=='true' for n in nodes)
def wait_for_source_or_focused_search(nodes,uri,inspect,focus,*,timeout=20,clock=time.monotonic,pause=time.sleep):
 deadline=clock()+timeout;attempt=0
 while True:
  if source_visible(nodes,uri):return nodes,dict(state='exact-exported-uri-visible',observations=attempt+1)
  search=[n for n in nodes if n.get('id')==SEARCH_ID and n.get('type')=='TextInput' and n.get('visible')=='true' and n.get('enabled')=='true']
  if len(search)>1:raise RuntimeError('System picker search input is not unique')
  if search and search[0].get('focused')=='true':return nodes,dict(state='search-focus-observed',observations=attempt+1)
  if clock()>=deadline:raise RuntimeError('System document picker did not expose the exact exported URI or confirmed search focus')
  if search:focus(nodes)
  pause(.25);attempt+=1;nodes=inspect(attempt)
