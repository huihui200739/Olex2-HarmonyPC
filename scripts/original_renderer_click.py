#!/usr/bin/env python3
import argparse,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--target',required=True);p.add_argument('--layout',type=Path,required=True);p.add_argument('--id',required=True);p.add_argument('--hdc',required=True);a=p.parse_args()
def attrs(x):
 if isinstance(x,dict):
  if 'attributes' in x:yield x['attributes']
  for v in x.values():yield from attrs(v)
 elif isinstance(x,list):
  for v in x:yield from attrs(v)
match=[n for n in attrs(json.loads(a.layout.read_text())) if n.get('id')==a.id and n.get('visible')=='true' and n.get('enabled')=='true'];assert len(match)==1,match
x1,y1,x2,y2=map(int,re.findall(r'-?\d+',match[0]['bounds']));result=subprocess.run([a.hdc,'-t',a.target,'shell',f'uitest uiInput click {(x1+x2)//2} {(y1+y2)//2}'],text=True,capture_output=True,check=True);a.layout.with_name(a.id+'-input.log').write_text(result.stdout+result.stderr);print(result.stdout)
