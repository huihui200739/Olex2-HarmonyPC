#!/usr/bin/env python3
"""Candidate-only schema 2 extension with retained fixed-recipe PNG headers.

C/C++ source-only graphics build, fixed vendor and every native DSO.

Traceability only: not authenticated provenance, reproducible bytes or full port.
Schema 1 remains supported by build_provenance.py for historical packages.
"""
import argparse,hashlib,json,shlex,shutil,struct,subprocess,sys,tarfile,zipfile
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
import build_provenance as old
from vendor_sources import verify_vendor
require,record,canonical,digest=old.require,old.record,old.canonical,old.digest
SCHEMA=2
SOURCE_DIRS=old.SOURCE_DIRS+('vendor',)
TOOL_ROLES=old.TOOL_ROLES|{'clang'}
RECIPES=old.RECIPES+('scripts/build_provenance_graphics.py','scripts/build_provenance_png.py','scripts/vendor_sources.py','vendor.lock.json')
EMBEDDED=old.EMBEDDED
METADATA_DIR='.olex2-build-metadata'
LIBRARIES={'libs/arm64-v8a/libolex2native.so','libs/arm64-v8a/libolex2core_graphics.so','libs/arm64-v8a/libc++_shared.so'}
LIMITS=old.LIMITS+['Includes fixed third-party C/C++ and all packaged native libraries; not complete original renderer certification.', 'Does not directly hash installed target code when the OS denies shell access.']

def snapshot_files(root):
 output=[]
 for directory in SOURCE_DIRS:
  base=root/directory;require(base.is_dir(),'Missing snapshot directory: '+directory)
  for path in sorted(base.rglob('*')):
   parts=path.relative_to(root).parts
   if directory!='vendor' and (any(part in old.EXCLUDED_DIRS for part in parts) or parts[-1]=='local.properties' or path==root/old.GENERATED_INPUT):continue
   require(not path.is_symlink(),'Symlink in snapshot: '+str(path))
   if path.is_file():output.append(record(path,path.relative_to(root).as_posix()))
 return sorted(output,key=lambda item:item['path'])

def capture(stage,workspace,tools,sdk,destination):
 stage,workspace,sdk=stage.resolve(),workspace.resolve(),sdk.resolve()
 vendor=verify_vendor(stage);require((stage/'vendor.lock.json').read_bytes()==(workspace/'vendor.lock.json').read_bytes(),'Staged vendor lock differs')
 files=snapshot_files(stage)
 for item in files:
  original=workspace/item['path']
  if original.is_file():require(record(original,item['path'])==item,'Snapshot differs from source: '+item['path']);item['origin']='workspace'
  else:
   require(item['path'].startswith('app/') and original.name=='oh-package-lock.json5','Unexpected generated source: '+item['path']);item['origin']='resolved-local-dependency-lock'
 lock=json.loads((workspace/'source.lock.json').read_text());metadata=[record(old.relative_file(workspace,name),name) for name in sorted(set(RECIPES)|{p['file'] for p in lock['patches']})]
 for item in metadata:
  target=stage/METADATA_DIR/item['path'];target.parent.mkdir(parents=True,exist_ok=True)
  shutil.copy2(old.relative_file(workspace,item['path']),target)
  require(record(target,item['path'])==item,'Metadata copy differs from source')
 for patch in lock['patches']:require(record(workspace/patch['file'],patch['file'])['sha256']==patch['sha256'],'Patch differs from source lock')
 require(record(stage/'upstream/LICENCE.txt','licence')['sha256']==lock['license_sha256'],'Upstream licence differs')
 identity=json.loads((stage/'app/AppScope/app.json5').read_text())['app']
 require(len(tools)==len({name for name,_ in tools}) and {name for name,_ in tools}==TOOL_ROLES,'Missing or unexpected build tool roles')
 tool_records=[dict(role=role,filename=path.resolve(strict=True).name,**{k:v for k,v in record(path.resolve(strict=True),role).items() if k!='path'}) for role,path in sorted(tools)]
 manifest=dict(schema_version=2,kind='olex2-diagnostic-build-inputs',identity={k:identity[k] for k in ('bundleName','versionName','versionCode')},full_port_complete=False,upstream_commit=lock['commit'],files=files,snapshot_sha256=digest(canonical(files)),metadata=metadata,tools=tool_records,sdk=json.loads((sdk/'oh-uni-package.json').read_text()),vendor=vendor,vendor_lock=json.loads((stage/'vendor.lock.json').read_text()),vendor_lock_text=(stage/'vendor.lock.json').read_text(),limits=LIMITS)
 destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(canonical(manifest));return manifest

def elf_sections(data):
 require(data[:6]==b'\x7fELF\x02\x01','Expected little-endian ELF64')
 require(struct.unpack_from('<H',data,18)[0]==183,'Expected target AArch64 ELF')
 offset=struct.unpack_from('<Q',data,40)[0];size,count,names=struct.unpack_from('<HHH',data,58)
 require(size==64 and 0<names<count and offset+size*count<=len(data),'Malformed ELF section table')
 headers=[struct.unpack_from('<IIQQQQIIQQ',data,offset+i*size) for i in range(count)]
 strings=headers[names];strings=data[strings[4]:strings[4]+strings[5]];out={}
 for header in headers:
  name=strings[header[0]:].split(b'\0',1)[0].decode();payload=b'' if header[1]==8 else data[header[4]:header[4]+header[5]]
  require(header[1]==8 or len(payload)==header[5],'Truncated ELF section')
  out[name]=(header,payload)
 return out

def allocated_sections(data):
 return {name:dict(type=h[1],flags=h[2],address=h[3],size=h[5],sha256=digest(payload)) for name,(h,payload) in elf_sections(data).items() if h[2]&2}

def elf_symbols_and_needed(data):
 sections=elf_sections(data);needed=[];exports=[]
 strings=sections.get('.dynstr',(None,b''))[1]
 for pos in range(0,len(sections.get('.dynamic',(None,b''))[1]),16):
  kind,value=struct.unpack_from('<qQ',sections['.dynamic'][1],pos)
  if kind==1:needed.append(strings[value:].split(b'\0',1)[0].decode())
 symbols=sections.get('.dynsym',(None,b''))[1]
 for pos in range(0,len(symbols),24):
  name,info,other,section,value,size=struct.unpack_from('<IBBHQQ',symbols,pos)
  if section and info>>4 in (1,2) and other&3 in (0,3):exports.append(strings[name:].split(b'\0',1)[0].decode())
 return sorted(exports),sorted(needed)

GENERATED_PNG_HEADERS={
 'pinned-libpng/pnglibconf.h':('libpng-configuration',('vendor/libpng/CMakeLists.txt','cmake/Olex2PinnedPng.cmake','cmake/FindZLIB.cmake')),
 'pinned-zlib/zconf.h':('zlib-configuration',('vendor/zlib/CMakeLists.txt','vendor/zlib/zconf.h','cmake/Olex2PinnedPng.cmake')),
}

def generated_header_spec(path,files):
 parts=Path(path).parts
 require(len(parts)>5 and parts[:3]==('app','entry','.cxx') and '..' not in parts and Path(path).as_posix()==path,'Generated header is outside the isolated native build')
 suffix='/'.join(parts[-2:]);require(suffix in GENERATED_PNG_HEADERS,'Unknown generated native header: '+path)
 generator,recipes=GENERATED_PNG_HEADERS[suffix]
 require(all(name in files for name in recipes),'Generated header lacks its fixed source recipes')
 return generator,[{k:files[name][k] for k in ('path','bytes','sha256')} for name in recipes]

def validate_generated_headers(records,files):
 require(isinstance(records,list),'Missing generated-header records list')
 if not records:return {} # Existing schema-2 builds have no generated headers.
 old.check_records(records);found={};suffixes=set()
 for item in records:
  generator,recipes=generated_header_spec(item['path'],files)
  require(item.get('generator')==generator and item.get('recipe_inputs')==recipes,'Generated header recipe binding differs')
  suffix='/'.join(Path(item['path']).parts[-2:]);require(suffix not in suffixes,'Duplicate generated header role');suffixes.add(suffix)
  found[item['path']]=item
 require(not records or suffixes==set(GENERATED_PNG_HEADERS),'Missing generated PNG/zlib configuration header')
 return found

def native_evidence(stage,sdk,ninja,files):
 databases=list((stage/'app/entry/.cxx').rglob('compile_commands.json'));require(len(databases)==1,'Expected one native compilation database')
 database=databases[0];build=database.parent.resolve();commands=json.loads(database.read_text());require(commands,'Empty native compilation database')
 file_map={item['path']:item for item in files};compiled=[];objects=set()
 for command in commands:
  directory=Path(command['directory']).resolve();require(directory.is_relative_to(build),'Unexpected compile command directory')
  source=Path(command['file']);source=source if source.is_absolute() else directory/source
  label=old.normalize_path(source,stage,sdk);require(label.startswith('project/'),'Native source not from snapshot');item=record(source,label.removeprefix('project/'))
  require(all(item[k]==file_map.get(item['path'],{}).get(k) for k in item),'Compiled native source differs from capture')
  arguments=command.get('arguments') or shlex.split(command['command']);role='clang' if source.suffix=='.c' else 'clang++'
  require(source.suffix in ('.c','.cc','.cpp','.cxx'),'Unsupported native source language')
  require(arguments and Path(arguments[0]).name==role and Path(arguments[0]).resolve()==(sdk/'llvm/bin'/role).resolve(),'Unexpected native compiler')
  output=Path(command.get('output') or arguments[arguments.index('-o')+1]);output=output if output.is_absolute() else directory/output
  require(output.resolve().is_relative_to(build),'Native object outside build tree');obj=output.resolve().relative_to(build).as_posix();require(obj not in objects,'Duplicate native object');objects.add(obj)
  compiled.append(dict(**item,compiler_role=role,object=obj,command=normalize_command(shlex.join(arguments),stage,sdk)))
 compiled.sort(key=lambda item:item['path']);require(len(compiled)==len({i['path'] for i in compiled}),'Duplicate compiled native source')
 result=subprocess.run([str(ninja),'-C',str(build),'-t','deps'],capture_output=True,text=True,check=True);dependencies={};actual_objects=set()
 for line in result.stdout.splitlines():
  if not line:continue
  if not line.startswith(' '):
   require('#deps ' in line and '(VALID)' in line,'Missing or stale native dependency record')
   path=Path(line.split(': #deps',1)[0]);path=path if path.is_absolute() else build/path;require(path.resolve().is_relative_to(build),'Dependency object outside build tree');actual_objects.add(path.resolve().relative_to(build).as_posix())
  else:
   path=Path(line.strip());path=path if path.is_absolute() else build/path;label=old.normalize_path(path,stage,sdk);dependencies[label]=record(path.resolve(),label)
 require(actual_objects==objects,'Native dependency records do not cover every actual compiled object')
 for item in compiled:require('project/'+item['path'] in dependencies,'Compiled source missing from dependency evidence')
 generated=[]
 for label,item in dependencies.items():
  if label.startswith('project/'):
   relative=label.removeprefix('project/');original=file_map.get(relative)
   if original is None:
    require((stage/relative).resolve().is_relative_to(build),'Uncaptured dependency outside native build: '+relative)
    generator,recipes=generated_header_spec(relative,file_map)
    generated.append(dict(path=relative,bytes=item['bytes'],sha256=item['sha256'],generator=generator,recipe_inputs=recipes))
   else:require(all(item[k]==original[k] for k in ('bytes','sha256')),'Native project dependency differs from capture: '+relative)
 generated.sort(key=lambda r:r['path']);validate_generated_headers(generated,file_map)
 metadata=[record(path,path.relative_to(stage).as_posix()) for path in sorted(build.iterdir()) if path.name in ('compile_commands.json','CMakeCache.txt','build.ninja','.ninja_log','.ninja_deps')]
 return dict(compiled_sources=compiled,dependencies=[dependencies[p] for p in sorted(dependencies)],generated_headers=generated,object_count=len(objects),build_metadata=metadata),build,result.stdout

def normalize_command(text,stage,sdk):
 for prefix in sorted({str(stage),str(stage).replace('/private/tmp/','/tmp/')},key=len,reverse=True):
  text=text.replace(prefix,'$PROJECT')
 return text.replace(str(sdk),'$NATIVE_SDK')

def finalize(stage,sdk,ninja,hap,inputs,output,evidence_directory):
 stage,sdk=stage.resolve(),sdk.resolve();manifest=json.loads(inputs.read_text());require(manifest['schema_version']==2,'Unsupported input schema')
 actual=snapshot_files(stage);require(actual==[{k:i[k] for k in ('path','bytes','sha256')} for i in manifest['files']],'Source snapshot changed during build');verify_vendor(stage)
 native,build,deps=native_evidence(stage,sdk,ninja,manifest['files'])
 with zipfile.ZipFile(hap) as z:
  require(z.read(EMBEDDED)==inputs.read_bytes(),'Packaged input manifest differs from capture');members=old.zip_records(z);identity=json.loads(z.read('module.json'))['app'];require(all(identity[k]==v for k,v in manifest['identity'].items()),'Packaged identity mismatch')
  library_bytes={name:z.read(name) for name in z.namelist() if name.endswith('.so')}
 require(set(library_bytes)==LIBRARIES,'Unexpected native package graph')
 evidence_directory.mkdir(parents=True,exist_ok=False);libraries=[]
 (evidence_directory/'ninja-dependencies.txt').write_text(deps)
 for item in native.get('generated_headers',[]):
  target=evidence_directory/'generated-headers'/Path(item['path']).parent.name/Path(item['path']).name
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(stage/item['path'],target)
  require(record(target,item['path'])=={k:item[k] for k in ('path','bytes','sha256')},'Generated header changed after compilation')
 if native.get('generated_headers'):
  for target in ('png_genfiles','zlibstatic'):
   output_commands=subprocess.run([str(ninja),'-C',str(build),'-t','commands',target],capture_output=True,text=True,check=True).stdout
   (evidence_directory/(target+'-generation-commands.txt')).write_text(output_commands)

 for item in native['build_metadata']:
  destination=evidence_directory/'native-build'/Path(item['path']).name;destination.parent.mkdir(exist_ok=True);shutil.copy2(stage/item['path'],destination)
 for target in ('olex2native','olex2core_graphics'):
  result=subprocess.run([str(ninja),'-C',str(build),'-t','commands',target],capture_output=True,text=True,check=True)
  (evidence_directory/(target+'-commands.txt')).write_text(result.stdout)
  links=[line for line in result.stdout.splitlines() if ' -shared ' in line and '/lib'+target+'.so' in line]
  require(len(links)==1,'Cannot identify actual source-built link command: '+target)
  arguments=shlex.split(links[0]);produced=Path(arguments[arguments.index('-o')+1]);produced=produced if produced.is_absolute() else build/produced;produced=produced.resolve()
  require(produced.is_relative_to(stage/'app/entry/build') or produced.is_relative_to(stage/'app/entry/.cxx'),'Native output is not in fresh build tree')
  original=produced.read_bytes();name='libs/arm64-v8a/lib'+target+'.so';packaged=library_bytes[name];sections=allocated_sections(original)
  require(sections==allocated_sections(packaged),'Packaged native allocated sections differ from actual linked output: '+target)
  shutil.copy2(produced,evidence_directory/produced.name);exports,needed=elf_symbols_and_needed(packaged)
  libraries.append(dict(path=name,origin='compiled',package=dict(bytes=len(packaged),sha256=digest(packaged)),build_output=record(produced,produced.relative_to(stage).as_posix()),allocated_sections=sections,exports=exports,needed=needed,link_command=normalize_command(links[0],stage,sdk)))
 runtime_name='libs/arm64-v8a/libc++_shared.so';packaged=library_bytes[runtime_name];sections=allocated_sections(packaged)
 candidates=sorted((sdk/'llvm/lib/aarch64-linux-ohos').rglob('libc++_shared.so'));matches=[]
 for path in candidates:
  if allocated_sections(path.read_bytes())==sections:matches.append(path)
 require(matches,'Packaged C++ runtime differs from actual target SDK');source=matches[0]
 libraries.append(dict(path=runtime_name,origin='native-sdk',package=dict(bytes=len(packaged),sha256=digest(packaged)),sdk_source=record(source,source.relative_to(sdk).as_posix()),allocated_sections=sections))
 validate_graph(libraries,(stage/'cmake/original_renderer.exports').read_text())
 # Keep the exact source AND the original recipe/patch/lock bytes captured before
 # compilation. Metadata cannot be recovered later from a changed workspace.
 with tarfile.open(evidence_directory/'source-snapshot.tar.gz','w:gz') as archive:
  for item in manifest['files']:archive.add(stage/item['path'],arcname=item['path'],recursive=False)
  for item in manifest['metadata']:
   path=stage/METADATA_DIR/item['path']
   require(record(path,item['path'])==item,'Build metadata changed during compilation')
   archive.add(path,arcname=METADATA_DIR+'/'+item['path'],recursive=False)
 source_metadata=dict(schema_version=2,kind='olex2-diagnostic-build-result',created_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),full_port_complete=False,input_manifest_sha256=digest(inputs.read_bytes()),hap=record(hap,hap.name),members=members,native=native,native_libraries=libraries,packaged_native_build_matches=[item['path'] for item in libraries],retained_evidence=[record(path,path.relative_to(evidence_directory).as_posix()) for path in sorted(evidence_directory.rglob('*')) if path.is_file()],limits=LIMITS)
 output.write_bytes(canonical(source_metadata));return source_metadata

def validate_graph(libraries,exports_text):
 require({i['path'] for i in libraries}==LIBRARIES and len(libraries)==len(LIBRARIES),'Unexpected native library graph')
 core=next(i for i in libraries if i['path'].endswith('libolex2core_graphics.so'))
 parent=next(i for i in libraries if i['path'].endswith('libolex2native.so'))
 runtime=next(i for i in libraries if i['path'].endswith('libc++_shared.so'))
 require(core['origin']==parent['origin']=='compiled' and runtime['origin']=='native-sdk','Invalid native origin')
 allowed=sorted(line.strip()[:-1] for line in exports_text.splitlines() if line.strip().startswith(('_Z','olex2_')))
 require(bool(allowed) and len(allowed)==len(set(allowed)) and core['exports']==allowed and all(not name.startswith(('gl','egl')) for name in core['exports']),'Graphics exports differ from explicit host ABI')
 require('libolex2core_graphics.so' in parent['needed'] and 'libolex2native.so' not in core['needed'],'Invalid single-core DSO dependency graph')

def verify_evidence(provenance,manifest,sidecar):
 root=provenance.parent/'build-evidence';records=sidecar['retained_evidence']
 old.check_records(records)
 require(root.is_dir() and not root.is_symlink(),'Missing retained build evidence')
 actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() or p.is_symlink()}
 require(actual=={i['path'] for i in records},'Retained evidence inventory changed')
 for item in records:require(record(old.relative_file(root,item['path']),item['path'])==item,'Retained build evidence bytes changed: '+item['path'])
 expected={i['path']:i for i in manifest['files']}
 expected.update({METADATA_DIR+'/'+i['path']:i for i in manifest['metadata']})
 selected={};seen=set()
 with tarfile.open(root/'source-snapshot.tar.gz','r:gz') as archive:
  for member in archive:
   require(member.isfile() and member.name in expected and member.name not in seen,'Unexpected or duplicate archived input')
   item=expected[member.name];require(member.size==item['bytes'],'Archived input length changed')
   data=archive.extractfile(member).read();require(digest(data)==item['sha256'],'Archived input bytes changed')
   seen.add(member.name)
   if member.name in ('cmake/original_renderer.exports',METADATA_DIR+'/source.lock.json',METADATA_DIR+'/vendor.lock.json'):selected[member.name]=data
 require(seen==set(expected),'Missing archived build inputs')
 require(selected[METADATA_DIR+'/vendor.lock.json']==manifest['vendor_lock_text'].encode(),'Archived vendor lock differs from captured bytes')
 source_lock=json.loads(selected[METADATA_DIR+'/source.lock.json']);require(source_lock['commit']==manifest['upstream_commit'],'Upstream commit differs from source lock')
 metadata={i['path']:i for i in manifest['metadata']}
 for patch in source_lock['patches']:require(metadata[patch['file']]['sha256']==patch['sha256'],'Patch is not bound to upstream source lock')
 validate_graph(sidecar['native_libraries'],selected['cmake/original_renderer.exports'].decode())
 for item in sidecar['native_libraries']:
  if item['origin']=='compiled':
   path=root/Path(item['path']).name
   retained=record(path,item['build_output']['path'])
   require(retained==item['build_output'] and allocated_sections(path.read_bytes())==item['allocated_sections'],'Retained actual native output mismatch')
 generated=validate_generated_headers(sidecar['native'].get('generated_headers',[]),{i['path']:i for i in manifest['files']})
 for item in generated.values():
  retained=root/'generated-headers'/Path(item['path']).parent.name/Path(item['path']).name
  require(record(retained,item['path'])=={k:item[k] for k in ('path','bytes','sha256')},'Retained generated header differs from compiled dependency')
 for item in sidecar['native']['build_metadata']:
  require(record(root/'native-build'/Path(item['path']).name,item['path'])==item,'Raw native build metadata changed')

def verify(hap,provenance,workspace=None):
 sidecar=json.loads(provenance.read_text());require(sidecar.get('schema_version')==2 and sidecar.get('kind')=='olex2-diagnostic-build-result','Unsupported build result');require(sidecar.get('full_port_complete') is False,'Incorrect completion claim');require(record(hap,hap.name)==sidecar['hap'],'HAP bytes differ from build result')
 with zipfile.ZipFile(hap) as z:
  manifest_bytes=z.read(EMBEDDED);manifest=json.loads(manifest_bytes)
  require(manifest.get('schema_version')==2 and manifest.get('kind')=='olex2-diagnostic-build-inputs' and manifest.get('full_port_complete') is False,'Unsupported input manifest')
  require(digest(manifest_bytes)==sidecar['input_manifest_sha256'],'Input manifest digest mismatch');require(old.zip_records(z)==sidecar['members'],'Packaged members differ from build result');identity=json.loads(z.read('module.json'))['app'];require(all(identity[k]==v for k,v in manifest['identity'].items()),'HAP identity mismatch')
  package_libraries={name:z.read(name) for name in z.namelist() if name.endswith('.so')}
 for records in (manifest['files'],manifest['metadata']):old.check_records(records)
 tools=manifest['tools'];require([item['role'] for item in tools]==sorted(TOOL_ROLES),'Missing or unexpected build tool evidence');old.check_records([dict(path=i['role'],bytes=i['bytes'],sha256=i['sha256']) for i in tools])
 require(set(manifest['identity'])=={'bundleName','versionName','versionCode'},'Incomplete app identity');require(digest(canonical(manifest['files']))==manifest['snapshot_sha256'],'Source snapshot digest mismatch')
 files={i['path']:i for i in manifest['files']};metadata={i['path']:i for i in manifest['metadata']}
 for item in files.values():require(item.get('origin') in ('workspace','resolved-local-dependency-lock'),'Unknown snapshot input origin')
 vendor=manifest['vendor_lock'];raw_lock=manifest['vendor_lock_text'].encode()
 require(json.loads(raw_lock)==vendor and len(raw_lock)==metadata['vendor.lock.json']['bytes'] and digest(raw_lock)==metadata['vendor.lock.json']['sha256'],'Vendor lock differs from recorded metadata')
 vendor_files={}
 for package in vendor['packages']:
  require(package['local_changes']==[] and len(package['revision'])==40,'Unpinned or modified vendor source')
  for item in package['files']:vendor_files[package['directory']+'/'+item['path']]=item
 for item in vendor['extra_files']:vendor_files[item['path']]=item
 require(set(vendor_files)=={path for path in files if path.startswith('vendor/')},'Vendor source inventory differs from fixed lock')
 for path,item in vendor_files.items():require(all(item[k]==files[path][k] for k in ('bytes','sha256')),'Vendor source differs from fixed lock')
 native=sidecar['native'];old.check_records(native['compiled_sources']);old.check_records(native['dependencies']);require(native['object_count']==len(native['compiled_sources'])>0,'Native object count mismatch');deps={i['path']:i for i in native['dependencies']};objects=[]
 for item in native['compiled_sources']:
  require(item['path'] in files and all(item[k]==files[item['path']][k] for k in ('bytes','sha256')),'Compiled native source not bound to snapshot');dep=deps.get('project/'+item['path']);require(dep and all(item[k]==dep[k] for k in ('bytes','sha256')),'Compiled source not bound to dependencies')
  require(item.get('compiler_role')==('clang' if Path(item['path']).suffix=='.c' else 'clang++'),'Compiler role differs from source language');require(bool(item.get('command')) and bool(item.get('object')),'Missing actual compilation command or object');objects.append(item['object'])
 require(len(objects)==len(set(objects)),'Duplicate native object')
 generated=validate_generated_headers(native.get('generated_headers',[]),files)
 require(all('project/'+path in deps for path in generated),'Generated header absent from actual Ninja dependencies')
 for path,item in deps.items():
  if path.startswith('project/'):
   relative=path.removeprefix('project/');original=files.get(relative) or generated.get(relative);require(original and all(item[k]==original[k] for k in ('bytes','sha256')),'Project header not bound to snapshot or retained fixed-recipe generated input')
  else:require(path.startswith('native-sdk/'),'Unexpected native dependency namespace')
 require(set(package_libraries)==LIBRARIES,'Unexpected native package graph')
 libraries=sidecar['native_libraries'];require({i['path'] for i in libraries}==set(package_libraries) and len(libraries)==len(package_libraries),'Missing or duplicate native library evidence')
 for item in libraries:
  data=package_libraries[item['path']];require(item['package']==dict(bytes=len(data),sha256=digest(data)),'Native package bytes mismatch');require(allocated_sections(data)==item['allocated_sections'],'Native allocated sections mismatch')
  if item['origin']=='compiled':
   exports,needed=elf_symbols_and_needed(data);require(item['exports']==exports and item['needed']==needed,'Native reflection mismatch');require(item['build_output']['path'].startswith(('app/entry/build/','app/entry/.cxx/')) and item.get('link_command'),'Native library is not from actual source build')
  else:require(item['origin']=='native-sdk' and item['sdk_source']['path'].startswith('llvm/lib/aarch64-linux-ohos/'),'Unknown native library origin')
 require(sidecar.get('packaged_native_build_matches')==[i['path'] for i in libraries],'Missing native build match evidence')
 verify_evidence(provenance,manifest,sidecar)
 if workspace is not None:
  workspace=workspace.resolve();verify_vendor(workspace)
  for item in manifest['files']+manifest['metadata']:
   if item.get('origin','workspace')=='workspace':require(all(item[k]==record(old.relative_file(workspace,item['path']),item['path'])[k] for k in ('path','bytes','sha256')),'Workspace changed: '+item['path'])
  for current in snapshot_files(workspace):require(current['path'] in files and all(current[k]==files[current['path']][k] for k in ('path','bytes','sha256')),'Workspace added or changed source: '+current['path'])
 return dict(passed=True,schema_version=2,hap_sha256=sidecar['hap']['sha256'],input_manifest_sha256=sidecar['input_manifest_sha256'],snapshot_sha256=manifest['snapshot_sha256'],staged_files=len(files),compiled_native_sources=len(native['compiled_sources']),native_dependency_files=len(deps),generated_header_inputs=len(generated),tools=len(tools),sdk=manifest['sdk'],native_libraries=len(libraries),full_port_complete=False,workspace_matches=workspace is not None,limits=LIMITS)

def main():
 p=argparse.ArgumentParser(description=__doc__);subs=p.add_subparsers(dest='action',required=True)
 cap=subs.add_parser('capture')
 for name in ('stage','workspace','sdk','output'):cap.add_argument('--'+name,type=Path,required=True)
 cap.add_argument('--tool',action='append',default=[])
 end=subs.add_parser('finalize')
 for name in ('stage','sdk','ninja','hap','inputs','output','evidence-directory'):end.add_argument('--'+name,type=Path,required=True)
 check=subs.add_parser('verify')
 for name in ('hap','provenance'):check.add_argument('--'+name,type=Path,required=True)
 check.add_argument('--workspace',type=Path)
 a=p.parse_args()
 try:
  if a.action=='capture':result=capture(a.stage,a.workspace,[(role,Path(path)) for role,path in (v.split('=',1) for v in a.tool)],a.sdk,a.output);print('Captured source/vendor files:',len(result['files']))
  elif a.action=='finalize':result=finalize(a.stage,a.sdk,a.ninja,a.hap,a.inputs,a.output,a.evidence_directory);print('Bound actual C/C++ sources:',len(result['native']['compiled_sources']),'libraries:',len(result['native_libraries']))
  else:print(json.dumps(verify(a.hap,a.provenance,a.workspace),indent=2))
 except (ValueError,KeyError,OSError,TypeError,AssertionError,struct.error,subprocess.SubprocessError,zipfile.BadZipFile) as error:print('Graphics build provenance failed:',error,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
