"""Explicit isolated synthetic traceability fixtures, not runtime/build evidence."""
import copy
import io
import json
from pathlib import Path
import shutil
import struct
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_provenance as old
import build_provenance_graphics as bp
from vendor_sources import verify_vendor


def synthetic_elf(exports=(), needed=()):
    """ELF metadata only: contains no executable code and is never loaded."""
    names = ['', '.shstrtab', '.dynstr', '.dynsym', '.dynamic', '.text']
    shstr = b'\0' + b''.join(n.encode() + b'\0' for n in names[1:])
    strings = b'\0'
    positions = {}
    for name in (*exports, *needed):
        positions[name] = len(strings)
        strings += name.encode() + b'\0'
    symbols = b'\0' * 24 + b''.join(struct.pack('<IBBHQQ', positions[n], 0x12, 0, 5, 0, 0) for n in exports)
    dynamic = b''.join(struct.pack('<qQ', 1, positions[n]) for n in needed) + b'\0' * 16
    payloads = [b'', shstr, strings, symbols, dynamic, b'non-executable synthetic metadata']
    data = bytearray(64)
    data[:16] = b'\x7fELF\x02\x01\x01' + b'\0' * 9
    struct.pack_into('<HHI', data, 16, 3, 183, 1)
    headers = [(0,) * 10]
    for index in range(1, len(names)):
        payload = payloads[index]
        offset = len(data)
        data.extend(payload)
        headers.append((shstr.index(names[index].encode()), [0, 3, 3, 11, 6, 1][index],
                        2 if index >= 2 else 0, 0, offset, len(payload),
                        2 if index in (3, 4) else 0, 0, 1, {3: 24, 4: 16}.get(index, 0)))
    table = len(data)
    data.extend(b''.join(struct.pack('<IIQQQQIIQQ', *h) for h in headers))
    struct.pack_into('<Q', data, 40, table)
    struct.pack_into('<HHH', data, 58, 64, len(headers), 1)
    return bytes(data)


class GraphicsProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='explicit-synthetic-graphics-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.workspace, self.stage, self.sdk = [self.root / p for p in ('workspace', 'stage', 'sdk')]
        for root in (self.workspace, self.stage):
            for directory in bp.SOURCE_DIRS:
                (root / directory).mkdir(parents=True)
        self.put('app/AppScope/app.json5', json.dumps({'app': dict(bundleName='isolated.synthetic', versionName='synthetic', versionCode=1)}))
        self.put('upstream/LICENCE.txt', 'synthetic licence')
        self.put('native/synthetic.cpp', '// synthetic C++')
        self.put('native/synthetic.c', '/* synthetic C */')
        self.put('vendor/synthetic/input.c', '/* fixed synthetic vendor */')
        self.put('cmake/original_renderer.exports', '{\n global:\n olex2_legacy_run;\n local: *;\n};\n')
        vendor = dict(schema_version=1, packages=[dict(name='synthetic-vendor', directory='vendor/synthetic', revision='1'*40,
                      archive_sha256='2'*64, local_changes=[], files=[old.record(self.workspace/'vendor/synthetic/input.c', 'input.c')])], extra_files=[])
        self.put('vendor.lock.json', json.dumps(vendor, indent=3))  # deliberately NOT canonical
        source_lock = dict(commit='isolated-synthetic-commit', patches=[], license_sha256=old.digest(b'synthetic licence'))
        for path in bp.RECIPES:
            target = self.workspace/path
            target.parent.mkdir(parents=True, exist_ok=True)
            if path == 'source.lock.json':
                target.write_text(json.dumps(source_lock))
            elif path != 'vendor.lock.json':
                target.write_text('# synthetic recipe, never executed')
        self.sdk.mkdir()
        (self.sdk/'oh-uni-package.json').write_text('{"apiVersion":"synthetic-only"}')
        self.tool = self.sdk/'synthetic-tool-never-executed'
        self.tool.write_text('synthetic tool marker')
        self.inputs = self.stage/old.GENERATED_INPUT
        self.manifest = bp.capture(self.stage, self.workspace, [(r, self.tool) for r in bp.TOOL_ROLES], self.sdk, self.inputs)
        self.hap, self.provenance = self.root/'synthetic.hap', self.root/'synthetic.provenance.json'
        self.evidence = self.root/'build-evidence'
        self.evidence.mkdir()
        files = {i['path']: i for i in self.manifest['files']}
        compiled = [dict(**{k: files[p][k] for k in ('path','bytes','sha256')}, object=p+'.o', compiler_role=role, command='explicit synthetic command; never executed')
                    for p, role in [('native/synthetic.c','clang'), ('native/synthetic.cpp','clang++')]]
        deps = [dict(path='project/'+i['path'], bytes=i['bytes'], sha256=i['sha256']) for i in compiled]
        self.libraries = {}
        library_records = []
        for name in ('olex2native', 'olex2core_graphics', 'c++_shared'):
            data = synthetic_elf(('olex2_legacy_run',) if name=='olex2core_graphics' else (), ('libolex2core_graphics.so',) if name=='olex2native' else ())
            label = 'libs/arm64-v8a/lib'+name+'.so'
            self.libraries[label] = data
            item = dict(path=label, origin='native-sdk' if name=='c++_shared' else 'compiled',
                        package=dict(bytes=len(data),sha256=old.digest(data)), allocated_sections=bp.allocated_sections(data))
            if item['origin']=='compiled':
                target=self.evidence/Path(label).name;target.write_bytes(data)
                item.update(build_output=old.record(target, 'app/entry/build/synthetic/'+target.name),
                            exports=bp.elf_symbols_and_needed(data)[0], needed=bp.elf_symbols_and_needed(data)[1], link_command='synthetic link never executed')
            else:
                item['sdk_source']=dict(path='llvm/lib/aarch64-linux-ohos/synthetic/libc++_shared.so',bytes=len(data),sha256=old.digest(data))
            library_records.append(item)
        self.archive()
        self.result = dict(schema_version=2,kind='olex2-diagnostic-build-result',full_port_complete=False,
            input_manifest_sha256=old.digest(self.inputs.read_bytes()),native=dict(compiled_sources=compiled, dependencies=deps,object_count=2,build_metadata=[]),
            native_libraries=library_records,packaged_native_build_matches=[i['path'] for i in library_records])
        self.refresh_package();self.refresh_evidence();self.save()

    def put(self,path,text):
        for root in (self.workspace,self.stage):
            target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)

    def archive(self, omit=None, changed=None):
        with tarfile.open(self.evidence/'source-snapshot.tar.gz','w:gz') as archive:
            for item in self.manifest['files']+self.manifest['metadata']:
                metadata=item in self.manifest['metadata']
                name=(bp.METADATA_DIR+'/' if metadata else '')+item['path']
                if name==omit:continue
                source=(self.stage/bp.METADATA_DIR if metadata else self.stage)/item['path']
                if name==changed:
                    data=b'changed synthetic fixture';info=tarfile.TarInfo(name);info.size=len(data);archive.addfile(info,io.BytesIO(data))
                else:archive.add(source,arcname=name,recursive=False)

    def refresh_package(self):
        with zipfile.ZipFile(self.hap,'w') as archive:
            archive.writestr('module.json',old.canonical(dict(app=self.manifest['identity'])))
            archive.writestr(bp.EMBEDDED,old.canonical(self.manifest))
            for name,data in self.libraries.items():archive.writestr(name,data)
        with zipfile.ZipFile(self.hap) as archive:self.result['members']=old.zip_records(archive)
        self.result['hap']=old.record(self.hap,self.hap.name)
        self.result['input_manifest_sha256']=old.digest(old.canonical(self.manifest))

    def refresh_evidence(self):
        self.result['retained_evidence']=[old.record(p,p.relative_to(self.evidence).as_posix()) for p in sorted(self.evidence.rglob('*')) if p.is_file()]

    def save(self):self.provenance.write_bytes(old.canonical(self.result))
    def verify(self,workspace=True):return old.verify(self.hap,self.provenance,self.workspace if workspace else None)
    def reject(self):
        self.save()
        with self.assertRaises((ValueError,KeyError)):self.verify()

    def test_valid_fixture_and_original_vendor_bytes(self):
        self.assertNotEqual(old.canonical(self.manifest['vendor_lock']),self.manifest['vendor_lock_text'].encode())
        self.assertTrue(self.verify()['passed'])
    def test_offline_retained_evidence(self):self.assertTrue(self.verify(False)['passed'])
    def test_missing_evidence(self):shutil.rmtree(self.evidence);self.reject()
    def test_modified_evidence(self):(self.evidence/'libolex2native.so').write_bytes(b'changed');self.reject()
    def test_extra_evidence(self):(self.evidence/'unexpected.txt').write_text('synthetic');self.reject()
    def test_missing_archived_recipe(self):
        self.archive(omit=bp.METADATA_DIR+'/scripts/build-app.sh');self.refresh_evidence();self.reject()
    def test_changed_archived_recipe(self):
        self.archive(changed=bp.METADATA_DIR+'/scripts/build-app.sh');self.refresh_evidence();self.reject()
    def test_changed_retained_native_output_even_with_updated_inventory(self):
        (self.evidence/'libolex2native.so').write_bytes(b'changed');self.refresh_evidence();self.reject()
    def test_wrong_vendor_text(self):
        self.manifest['vendor_lock_text']+=' ';self.refresh_package();self.reject()
    def test_missing_vendor_inventory(self):
        self.manifest['vendor_lock']['packages'][0]['files']=[];self.refresh_package();self.reject()
    def test_modified_workspace_vendor(self):(self.workspace/'vendor/synthetic/input.c').write_text('changed');self.reject()
    def test_extra_workspace_vendor(self):(self.workspace/'vendor/extra.c').write_text('extra');self.reject()
    def test_symlink_vendor(self):
        path=self.workspace/'vendor/synthetic/input.c';path.unlink();path.symlink_to(self.stage/'vendor/synthetic/input.c');self.reject()
    def test_unsupported_vendor_schema(self):
        data=json.loads((self.workspace/'vendor.lock.json').read_text());data['schema_version']=99
        (self.workspace/'vendor.lock.json').write_text(json.dumps(data))
        with self.assertRaises(ValueError):verify_vendor(self.workspace)
    def test_missing_clang(self):
        self.manifest['tools']=[i for i in self.manifest['tools'] if i['role']!='clang'];self.refresh_package();self.reject()
    def test_wrong_compiler_language(self):self.result['native']['compiled_sources'][0]['compiler_role']='clang++';self.reject()
    def test_duplicate_object(self):self.result['native']['compiled_sources'][0]['object']=self.result['native']['compiled_sources'][1]['object'];self.reject()
    def test_missing_dependency(self):self.result['native']['dependencies']=self.result['native']['dependencies'][1:];self.reject()
    def test_wrong_object_count(self):self.result['native']['object_count']=1;self.reject()
    def test_missing_library(self):self.result['native_libraries'].pop();self.reject()
    def test_extra_packaged_library(self):
        self.libraries['libs/arm64-v8a/extra.so']=synthetic_elf();self.refresh_package();self.reject()
    def test_false_build_match_claim(self):self.result['packaged_native_build_matches']=[];self.reject()
    def test_false_completion_claim(self):self.result['full_port_complete']=True;self.reject()
    def test_changed_workspace_recipe(self):(self.workspace/'scripts/build-app.sh').write_text('changed');self.reject()
    def test_leaked_core_export(self):
        libraries=copy.deepcopy(self.result['native_libraries']);libraries[1]['exports'].append('glReadPixels')
        with self.assertRaises(ValueError):bp.validate_graph(libraries,(self.stage/'cmake/original_renderer.exports').read_text())
    def test_duplicate_runtime_dependency(self):
        libraries=copy.deepcopy(self.result['native_libraries']);libraries[0]['needed']=[]
        with self.assertRaises(ValueError):bp.validate_graph(libraries,(self.stage/'cmake/original_renderer.exports').read_text())
    def test_truncated_elf(self):
        with self.assertRaises(ValueError):bp.elf_sections(b'not ELF')
    def test_vendor_file_named_build_is_not_generated_output(self):
        self.put('vendor/synthetic/build','real vendor source file, not app output')
        self.assertIn('vendor/synthetic/build',{i['path'] for i in bp.snapshot_files(self.stage)})
    def test_normalize_temporary_path_alias(self):
        command='/tmp/example/native/x.c /private/tmp/example/native/y.c /sdk/llvm/bin/clang'
        self.assertEqual(bp.normalize_command(command,Path('/private/tmp/example'),Path('/sdk')),'$PROJECT/native/x.c $PROJECT/native/y.c $NATIVE_SDK/llvm/bin/clang')

if __name__=='__main__':unittest.main()
