#!/usr/bin/env python3
"""Materialize fixed, unmodified PNG dependencies from retained official archives."""
import argparse, hashlib, json, tarfile
from pathlib import Path, PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
PACKAGES=(
 ('libpng','pnggroup/libpng','v1.6.59','cd952f49f95bb27154ae77dbb103032d95f6e580','libpng-cd952f49.tar.gz','7e148caf27025d7a38dfea58e1ed91d56a002a8c9d4f3375ff4eacba74c82863'),
 ('zlib','madler/zlib','v1.4','99623ef7097352da7993ba614161fd6a311e046e','zlib-99623ef7.tar.gz','b19d5724a253e50dee58620937ed1ca4e1fa2e6305baa18375afd56f2a2b9355'))
def prepare(destination,archives):
 destination=Path(destination); vendor=destination/'vendor'
 if vendor.exists():raise ValueError('Retain existing vendor inputs; select a new destination')
 lock=dict(schema_version=1,scope='Fixed unmodified PNG decoder dependencies; no complete application or release qualification',packages=[],extra_files=[])
 for name,repo,tag,revision,archive_name,archive_sha in PACKAGES:
  path=Path(archives)/archive_name
  if hashlib.sha256(path.read_bytes()).hexdigest()!=archive_sha:raise ValueError('Archive SHA differs: '+name)
  files=[]
  with tarfile.open(path,'r:gz') as tar:
   members=tar.getmembers();prefix=PurePosixPath(members[0].name).parts[0]
   for item in members:
    parts=PurePosixPath(item.name).parts
    if parts[0]!=prefix or '..' in parts or not (item.isdir() or item.isfile()):raise ValueError('Unexpected archive member: '+item.name)
    if not item.isfile():continue
    relative=PurePosixPath(*parts[1:]).as_posix()
    target=vendor/name/relative;target.parent.mkdir(parents=True,exist_ok=True)
    data=tar.extractfile(item).read();target.write_bytes(data)
    files.append(dict(path=relative,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
  lock['packages'].append(dict(name=name,repository=repo,tag=tag,revision=revision,archive_sha256=archive_sha,directory='vendor/'+name,excluded_archive_prefixes=[],local_changes=[],files=sorted(files,key=lambda r:r['path'])))
 destination.mkdir(parents=True,exist_ok=True);(destination/'vendor.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
 return dict(root=str(destination),packages=[dict(name=p['name'],tag=p['tag'],files=len(p['files'])) for p in lock['packages']],third_party_source_modified=False)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--destination',type=Path,default=ROOT/'experiments/gx-png-image');p.add_argument('--archives',type=Path,default=ROOT/'out/gx-application-evaluation/png-dependencies');a=p.parse_args();print(json.dumps(prepare(a.destination,a.archives),indent=2))
