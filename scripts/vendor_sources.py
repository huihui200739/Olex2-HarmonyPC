"""Fixed, unmodified graphics dependency inputs; no downloads or global installs."""
import argparse,json,hashlib
from pathlib import Path,PurePosixPath

def verify_vendor(root):
 root=Path(root).resolve();lock=json.loads((root/'vendor.lock.json').read_text())
 if lock.get('schema_version')!=1:raise ValueError('Unsupported vendor source lock')
 expected={};packages=[]
 for package in lock['packages']:
  if package.get('local_changes')!=[]:raise ValueError('Untracked vendor modifications')
  if len(package['revision'])!=40 or len(package['archive_sha256'])!=64:raise ValueError('Unpinned dependency')
  files=package['files'];labels=[i['path'] for i in files]
  if labels!=sorted(set(labels)):raise ValueError('Vendor files must be sorted and unique')
  for item in files:expected[package['directory']+'/'+item['path']]=item
  packages.append({'name':package['name'],'revision':package['revision'],'archive_sha256':package['archive_sha256'],'files':len(files)})
 for item in lock['extra_files']:expected[item['path']]=item
 for label,item in expected.items():
  parts=PurePosixPath(label).parts
  if not parts or parts[0]!='vendor' or '..' in parts or '\\' in label or PurePosixPath(label).as_posix()!=label:raise ValueError('Invalid vendor path')
  path=root/label
  if any(p.is_symlink() for p in (path,*path.parents) if p!=root.parent) or not path.is_file():raise ValueError('Vendor input is not a regular file: '+label)
  data=path.read_bytes()
  if len(data)!=item['bytes'] or hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Vendor bytes differ from fixed lock: '+label)
 actual={p.relative_to(root).as_posix() for p in (root/'vendor').rglob('*') if p.is_file() or p.is_symlink()}
 if actual!=set(expected):raise ValueError('Unexpected or missing vendor source files: '+str(sorted(actual^set(expected))))
 return {'passed':True,'packages':packages,'files':len(expected),'scope':'Exact fixed source inputs, not complete functionality or legal/release certification'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);a=p.parse_args();print(json.dumps(verify_vendor(a.root),indent=2))
