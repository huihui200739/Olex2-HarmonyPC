#!/usr/bin/env python3
"""Prepare source32 with source-built CPython inputs, not precompiled libraries."""
import hashlib, json, shutil, struct, tarfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'out/python-runtime-evaluation'
BUILD=BASE/'ohos-builds/20261110-130731-a34f74'
SOURCE=ROOT/'out/gx-application-evaluation/source32'
def sha(data):return hashlib.sha256(data).hexdigest()
def rec(path,label):return dict(path=label,bytes=path.stat().st_size,sha256=sha(path.read_bytes()))
def archive_members(path):
    data=path.read_bytes();assert data[:8]==b'!<arch>\n'
    pos=8;strings=b'';out=[]
    while pos<len(data):
        header=data[pos:pos+60];assert len(header)==60 and header[-2:]==b'`\n'
        name=header[:16].decode().strip();size=int(header[48:58]);payload=data[pos+60:pos+60+size]
        assert len(payload)==size
        if name=='//':strings=payload
        elif name=='/' or name.startswith('/SYM'):pass
        else:
            if name.startswith('#1/'):
                length=int(name[3:]);name=payload[:length].decode().rstrip('\0');payload=payload[length:]
            elif name.startswith('/'):
                name=strings[int(name[1:]):].split(b'/\n',1)[0].decode()
            else:name=name.rstrip('/')
            if name.endswith('.o'):out.append((name,sha(payload)))
        pos+=60+size+(size%2)
    assert pos==len(data)
    return out

def prepare():
    assert not SOURCE.exists(),'Do not overwrite a previous source32 candidate'
    status=json.loads((BUILD/'build-result.json').read_text());assert status['built']
    observed=json.loads((BASE/'runtime-bundles'/BUILD.name/'target-runtime-check-v1/result.json').read_text());assert observed['passed']
    for item in status['artifacts']:assert rec(BUILD/item['path'],item['path'])==item
    archive=BASE/'downloads/Python-3.11.18.tar.xz'
    assert sha(archive.read_bytes())=='9120e0a653868f6e3698d36cdd99e5a58903ec9469e448f21d98f7b16d8fa290'
    tag=json.loads((BASE/'cpython-v3.11.18-tag-resolution.json').read_text())
    assert tag['tag']=='v3.11.18' and tag['object']['type']=='commit'
    shutil.copytree(ROOT/'out/gx-application-evaluation/source31',SOURCE)
    # Fresh release archive bytes only, never host-generated pyc files.
    vendor=SOURCE/'vendor/cpython'
    vendor.mkdir()
    with tarfile.open(archive) as t:
        for member in t.getmembers():
            parts=Path(member.name).parts
            assert parts[0]=='Python-3.11.18' and '..' not in parts and not member.issym() and not member.islnk()
            destination=vendor.joinpath(*parts[1:])
            if member.isdir():destination.mkdir(parents=True,exist_ok=True)
            elif member.isfile():
                destination.parent.mkdir(parents=True,exist_ok=True)
                destination.write_bytes(t.extractfile(member).read())
                destination.chmod(member.mode)
            else:raise ValueError('Unexpected archive member '+member.name)
    # Identify exactly the objects in libpython and the two bundled dependency
    # archives by matching actual object bytes to successful compiler commands.
    commands={}
    for line in (BUILD/'compiler-commands.jsonl').read_text().splitlines():
        x=json.loads(line);args=x.get('command',[])
        if x.get('returncode')!=0 or '-c' not in args or '-o' not in args:continue
        cwd=Path(x['cwd']);obj=(cwd/args[args.index('-o')+1]).resolve()
        if not obj.is_file() or obj.suffix!='.o' or obj.name.startswith('conftest'):continue
        assert obj.read_bytes()[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',obj.read_bytes(),18)[0]==183
        commands[obj]=x
    indexed={}
    for path,x in commands.items():indexed.setdefault((path.name,sha(path.read_bytes())),[]).append((path,x))
    selected={};libraries=['libpython3.11.a','Modules/_decimal/libmpdec/libmpdec.a','Modules/expat/libexpat.a']
    for name in libraries:
        for member,digest in archive_members(BUILD/'build'/name):
            matches=indexed.get((member,digest),[]);assert len(matches)==1,(name,member,len(matches))
            path,x=matches[0];selected[path]=x
    generated=SOURCE/'native/python-build';generated.mkdir()
    mapping={}
    def input_path(path):
        path=path.resolve()
        if path.is_relative_to(BUILD/'source/Python-3.11.18'):return vendor/path.relative_to(BUILD/'source/Python-3.11.18')
        if path.is_relative_to(BUILD/'build'):
            new=generated/path.relative_to(BUILD/'build')
            if path.is_file() and not new.exists():new.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,new)
            return new
        raise ValueError('Input outside verified Python build: '+str(path))
    def normalize(value,cwd):
        return value.replace(str(BUILD/'source/Python-3.11.18'),'${OLEX2_ROOT}/vendor/cpython').replace(str(BUILD/'build'),'${OLEX2_ROOT}/native/python-build')
    compile_records=[];cmake=['# Generated from actual OHOS archive-member and compiler-command evidence.','add_library(olex2_python STATIC)']
    for obj,x in sorted(selected.items()):
        args=x['command'];cwd=Path(x['cwd']);source_tokens=[a for a in args if a.endswith('.c') and (cwd/a).is_file()];assert len(source_tokens)==1,args;source=(cwd/source_tokens[0]).resolve();new=input_path(source)
        dep=Path(str(obj)+'.d');assert dep.is_file(),str(dep)
        # All paths in this project/build have no spaces; compiler Make deps
        # escape continuations. Do not silently ignore a generated dependency.
        dependencies=dep.read_text().replace('\\\n',' ').split(':',1)[1].split()
        for name in dependencies:
            path=(cwd/name).resolve()
            if path.is_relative_to(BUILD/'build'):input_path(path)
            elif path.is_relative_to(BUILD/'source/Python-3.11.18'):
                assert rec(path,'input')==rec(input_path(path),'input')
        flags=[];i=1
        while i<len(args):
            a=args[i]
            if a in ('-o','-MF'):i+=2;continue
            if a=='-c' or a==source_tokens[0]:i+=1;continue
            if a in ('-MD','-MMD','-g'):i+=1;continue
            if a.startswith(('--target=','--sysroot=')):i+=1;continue
            if a=='-I':
                flags.append('-I'+normalize(str((cwd/args[i+1]).resolve()),cwd));i+=2;continue
            if a.startswith('-I'):
                flags.append('-I'+normalize(str((cwd/a[2:]).resolve()),cwd));i+=1;continue
            flags.append(normalize(a,cwd));i+=1
        rel=new.relative_to(SOURCE).as_posix()
        def quote(value):return '"'+value.replace('\\','\\\\').replace('"','\\"')+'"'
        cmake.append('target_sources(olex2_python PRIVATE "${OLEX2_ROOT}/'+rel+'")')
        cmake.append('set_source_files_properties("${OLEX2_ROOT}/'+rel+'" PROPERTIES COMPILE_OPTIONS '+quote(';'.join(flags))+')')
        compile_records.append(dict(source=rel,object=str(obj.relative_to(BUILD)),source_sha256=sha(new.read_bytes()),source_command=x['command'],cmake_options=flags))
    cmake+=['target_include_directories(olex2_python PUBLIC "${OLEX2_ROOT}/vendor/cpython/Include" "${OLEX2_ROOT}/native/python-build")','target_link_libraries(olex2_python PUBLIC dl m z pthread)','set_target_properties(olex2_python PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)']
    (SOURCE/'cmake/Olex2PythonRuntime.cmake').write_text('\n'.join(cmake)+'\n')
    (generated/'source-build-map.json').write_text(json.dumps(dict(archive_sha256=sha(archive.read_bytes()),upstream_release_commit=tag['object']['sha'],compiled_sources=compile_records,application_integration_complete=False),ensure_ascii=False,indent=2)+'\n')
    # Every selected C input must be rebuilt by the HAP's own CMake/Ninja;
    # never copy the evaluated precompiled libpython into this candidate.
    assert not list(vendor.rglob('*.o')) and not list(vendor.rglob('*.so')) and not list(vendor.rglob('*.a'))
    lock=json.loads((SOURCE/'vendor.lock.json').read_text())
    lock['packages'].append(dict(name='cpython',repository='https://github.com/python/cpython',revision=tag['object']['sha'],archive_sha256=sha(archive.read_bytes()),directory='vendor/cpython',excluded_archive_prefixes=[],files=[rec(p,p.relative_to(vendor).as_posix()) for p in sorted(vendor.rglob('*')) if p.is_file()],local_changes=[]))
    (SOURCE/'vendor.lock.json').write_text(json.dumps(lock,ensure_ascii=False,indent=2)+'\n')
    raw=SOURCE/'app/entry/src/main/resources/rawfile'
    bundle=BASE/'runtime-bundles'/BUILD.name
    shutil.copy2(bundle/'runtime/lib/python311.zip',raw/'python311.zip')
    shutil.copy2(vendor/'LICENSE',raw/'CPYTHON-LICENSE.txt')
    # Native bridge/lifecycle and UI integration are the next required edits;
    # the installed31 and backup/latest-source pointer are intentionally intact.
    result=dict(source=str(SOURCE.relative_to(ROOT)),prepared=True,source_only=True,python_compilation_units=len(compile_records),vendor_files=len(lock['packages'][-1]['files']),stdlib_files=851,hap_built=False,hap_installed=False,original_bridge_executed=False,application_integration_complete=False,full_port_complete=False)
    (BASE/'source32-preparation-v2.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':prepare()
