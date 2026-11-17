"""Require actual visible bounds to equal original, unclipped control bounds.

Merely testing the already-clipped UI bounds inside a panel misses overflow.
This validates only the named candidate controls, not general DPI/accessibility.
"""
import re

def attributes(tree):
 if isinstance(tree,dict):
  if isinstance(tree.get('attributes'),dict):yield tree['attributes']
  for value in tree.values():yield from attributes(value)
 elif isinstance(tree,list):
  for value in tree:yield from attributes(value)

def rectangle(text):
 if not isinstance(text,str) or not re.fullmatch(r'\[-?\d+,-?\d+\]\[-?\d+,-?\d+\]',text):return None
 values=list(map(int,re.findall(r'-?\d+',text)))
 return values if values[0]<values[2] and values[1]<values[3] else None

def controls_unclipped(tree):
 nodes=list(attributes(tree));details={}
 def unique(identifier):
  matches=[node for node in nodes if node.get('id')==identifier]
  return matches[0] if len(matches)==1 else {}
 panel=unique('olex2-viewport-panel');box=rectangle(panel.get('bounds'))
 if box is None:return False,{'error':'Missing actual viewport panel'}
 ok=True
 for identifier,kind in [('olex2-view-reset','Button'),('olex2-view-zoom','Slider'),('olex2-original-renderer-probe','Button')]:
  node=unique(identifier);bounds=rectangle(node.get('bounds'));original=rectangle(node.get('origBounds'))
  passed=(node.get('visible')=='true' and node.get('enabled')=='true' and node.get('type')==kind and bounds is not None and original==bounds and box[0]<=bounds[0]<bounds[2]<=box[2] and box[1]<=bounds[1]<bounds[3]<=box[3])
  details[identifier]={'passed':passed,'bounds':bounds,'original_bounds':original,'text':node.get('text')};ok=ok and passed
 return bool(ok),details
