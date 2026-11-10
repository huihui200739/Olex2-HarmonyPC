#!/usr/bin/env python3
"""Exact candidate20 identity/input profile of all existing212 diagnostic gates."""
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def once(text,old,new):
 if text.count(old)!=1:raise ValueError('Unexpected frozen212 recipe: '+old[:100])
 return text.replace(old,new,1)
def prepare(source,out,hap,host_log):
 baseline=ROOT/'scripts/prepare-texture-context-ui-tools.py';text=baseline.read_text()
 text=once(text,"identity['versionName']!='0.22.14' or identity['versionCode']!=221400","identity['versionName']!='0.22.20' or identity['versionCode']!=222000")
 text=once(text,"source/'texture-context-inputs.json'","source/'original-vector-font-inputs.json'")
 text=once(text,"manifest=dict(version='0.22.14'","manifest=dict(version='0.22.20'")
 namespace=dict(__name__='candidate20_exact212_tool_recipe',__file__=str(baseline))
 exec(compile(text,str(baseline)+' [exact19 identity/input]','exec'),namespace);namespace['prepare'](source,out,hap,host_log)
 path=out.resolve()/'tool-profile.json';manifest=json.loads(path.read_text())
 manifest['identity_only_profile']=dict(prior='0.22.14',current='0.22.20',prior_212_checks_not_removed=True,recipe_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),profile_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('source','output','hap','host-log'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();prepare(a.source,a.output,a.hap,a.host_log)
