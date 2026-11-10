"""Isolated synthetic build fixtures; never modify runtime evidence or the SDK."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'build_provenance.py'
spec = importlib.util.spec_from_file_location('build_provenance', SCRIPT)
bp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bp)


class BuildProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='synthetic-provenance-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.workspace = self.root / 'workspace'
        self.stage = self.root / 'stage'
        self.sdk = self.root / 'sdk'
        for root in (self.workspace, self.stage):
            for directory in bp.SOURCE_DIRS:
                (root / directory).mkdir(parents=True)
        self.put('app/AppScope/app.json5', json.dumps({'app': {
            'bundleName': 'isolated.synthetic', 'versionName': 'test', 'versionCode': 1}}))
        self.put('native/synthetic.cpp', '// synthetic\n')
        self.put('native/synthetic.h', '// synthetic header\n')
        self.put('upstream/LICENCE.txt', 'isolated licence fixture\n')
        self.put('cmake/fixture.cmake', '# synthetic\n')
        lock = dict(commit='synthetic-commit', patches=[],
                    license_sha256=bp.digest(b'isolated licence fixture\n'))
        for path, data in (('source.lock.json', json.dumps(lock)),
                           ('scripts/build-app.sh', '# isolated recipe'),
                           ('scripts/build_provenance.py', '# isolated capture'),
                           ('scripts/check-source.py', '# isolated source checker')):
            target = self.workspace / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(data)
        self.sdk.mkdir()
        (self.sdk / 'oh-uni-package.json').write_text('{"apiVersion":"synthetic"}')
        self.tool = self.sdk / 'isolated-tool'
        self.tool.write_text('isolated tool identifier; never executed')
        self.inputs = self.stage / bp.GENERATED_INPUT
        self.hap = self.root / 'synthetic.hap'
        self.sidecar = self.root / 'synthetic.provenance.json'
        self.manifest = self.capture()
        self.write_fixture()

    def put(self, path, data):
        for root in (self.workspace, self.stage):
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(data)

    def capture(self, **kwargs):
        return bp.capture(self.stage, self.workspace, [(role, self.tool) for role in bp.TOOL_ROLES],
                          self.sdk, self.inputs, **kwargs)

    def write_fixture(self):
        members = {'module.json': bp.canonical({'app': self.manifest['identity']}),
                   bp.EMBEDDED: bp.canonical(self.manifest),
                   'libs/arm64-v8a/libolex2native.so': b'isolated-not-an-executable',
                   'resources/rawfile/OLEX2-LICENCE.txt': b'isolated licence fixture\n'}
        self.write_zip(members)
        files = {item['path']: item for item in self.manifest['files']}
        source = {k: files['native/synthetic.cpp'][k] for k in ('path', 'bytes', 'sha256')}
        source['command'] = 'synthetic command, not executed'
        deps = []
        for path in ('native/synthetic.cpp', 'native/synthetic.h'):
            item = files[path]
            deps.append(dict(path='project/' + path, bytes=item['bytes'], sha256=item['sha256']))
        with zipfile.ZipFile(self.hap) as z:
            records = bp.zip_records(z)
        self.result = dict(schema_version=1, kind='olex2-diagnostic-build-result',
                           full_port_complete=False,
                           input_manifest_sha256=bp.digest(bp.canonical(self.manifest)),
                           hap=bp.record(self.hap, self.hap.name), members=records,
                           native=dict(compiled_sources=[source], dependencies=deps, object_count=1),
                           packaged_native_build_matches=['app/entry/build/synthetic/libolex2native.so'])
        self.save_result()

    def write_zip(self, members):
        with zipfile.ZipFile(self.hap, 'w') as z:
            for path, data in members.items():
                z.writestr(path, data)

    def save_result(self):
        self.sidecar.write_bytes(bp.canonical(self.result))

    def verify(self):
        return bp.verify(self.hap, self.sidecar, self.workspace)

    def test_valid_fixture_bound_to_workspace(self):
        result = self.verify()
        self.assertTrue(result['passed'])
        self.assertTrue(result['workspace_matches'])
        self.assertFalse(result['full_port_complete'])
        self.assertEqual(result['compiled_native_sources'], 1)

    def test_historical_artifact_without_workspace_check_is_explicit(self):
        self.assertFalse(bp.verify(self.hap, self.sidecar)['workspace_matches'])

    def test_changed_hap_is_not_accepted(self):
        with self.hap.open('ab') as f:
            f.write(b'isolated appended bytes')
        with self.assertRaisesRegex(ValueError, 'HAP bytes'):
            self.verify()

    def test_changed_workspace_source_is_not_accepted(self):
        (self.workspace / 'native/synthetic.cpp').write_text('// changed fixture')
        with self.assertRaisesRegex(ValueError, 'Workspace changed'):
            self.verify()

    def test_added_workspace_source_is_not_accepted(self):
        (self.workspace / 'native/added.cpp').write_text('// new fixture')
        with self.assertRaisesRegex(ValueError, 'added or changed'):
            self.verify()

    def test_missing_workspace_header_is_not_accepted(self):
        (self.workspace / 'native/synthetic.h').unlink()
        with self.assertRaisesRegex(ValueError, 'regular input'):
            self.verify()

    def test_changed_recipe_is_not_accepted(self):
        (self.workspace / 'scripts/build-app.sh').write_text('# changed recipe')
        with self.assertRaisesRegex(ValueError, 'Workspace changed'):
            self.verify()

    def test_member_digest_mismatch_is_not_accepted(self):
        self.result['members'][0]['sha256'] = '0' * 64
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'Packaged members'):
            self.verify()

    def test_embedded_manifest_digest_mismatch_is_not_accepted(self):
        self.result['input_manifest_sha256'] = '0' * 64
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'Input manifest digest'):
            self.verify()

    def test_native_source_must_be_in_snapshot(self):
        self.result['native']['compiled_sources'][0]['path'] = 'native/absent.cpp'
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'source not bound'):
            self.verify()

    def test_native_header_must_match_snapshot(self):
        self.result['native']['dependencies'][1]['sha256'] = '0' * 64
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'header not bound'):
            self.verify()

    def test_native_dependency_must_include_compiled_source(self):
        self.result['native']['dependencies'].pop(0)
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'dependency evidence'):
            self.verify()

    def test_object_count_must_match_compiled_sources(self):
        self.result['native']['object_count'] = 2
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'object count'):
            self.verify()

    def test_full_port_claim_is_not_accepted(self):
        self.result['full_port_complete'] = True
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'completion claim'):
            self.verify()

    def test_missing_native_build_match_is_not_accepted(self):
        self.result['packaged_native_build_matches'] = []
        self.save_result()
        with self.assertRaisesRegex(ValueError, 'native build match'):
            self.verify()

    def test_duplicate_input_records_are_not_accepted(self):
        records = copy.deepcopy(self.manifest['files'])
        records.append(records[-1])
        with self.assertRaisesRegex(ValueError, 'sorted and unique'):
            bp.check_records(records)

    def test_path_records_are_relative_and_normalized(self):
        for path in ('../outside', '/absolute', 'a/../b', 'a\\b', '.', './a', 'a//b'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                bp.check_records([dict(path=path, bytes=0, sha256='0' * 64)])

    def test_capture_rejects_source_changed_after_copy(self):
        (self.stage / 'native/synthetic.cpp').write_text('// changed copy')
        with self.assertRaisesRegex(ValueError, 'differs from source'):
            self.capture()

    def test_capture_rejects_unknown_generated_source(self):
        (self.stage / 'native/unexpected.cpp').write_text('// isolated fixture')
        with self.assertRaisesRegex(ValueError, 'Unexpected generated'):
            self.capture()

    def test_capture_records_resolved_local_dependency_lock(self):
        (self.stage / 'app/oh-package-lock.json5').write_text('{"synthetic":true}')
        manifest = self.capture()
        item = next(i for i in manifest['files'] if i['path'] == 'app/oh-package-lock.json5')
        self.assertEqual(item['origin'], 'resolved-local-dependency-lock')

    def test_capture_ignores_outputs_and_self_manifest(self):
        (self.stage / 'app/entry/build').mkdir()
        (self.stage / 'app/entry/build/synthetic.bin').write_text('isolated output')
        manifest = self.capture()
        self.assertEqual(manifest['files'], self.manifest['files'])
        self.assertNotIn(bp.GENERATED_INPUT, [i['path'] for i in manifest['files']])

    def test_source_symlinks_are_not_accepted(self):
        (self.stage / 'native/synthetic.h').unlink()
        (self.stage / 'native/synthetic.h').symlink_to(self.workspace / 'native/synthetic.h')
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            self.capture()

    def test_finalization_rejects_source_changed_during_build(self):
        (self.stage / 'native/synthetic.h').write_text('// changed during build')
        with self.assertRaisesRegex(ValueError, 'changed during build'):
            bp.finalize(self.stage, self.sdk, self.root / 'not-invoked',
                        self.hap, self.inputs, self.sidecar)

    def test_missing_native_compilation_database_is_not_accepted(self):
        with self.assertRaisesRegex(ValueError, 'compilation database'):
            bp.native_evidence(self.stage, self.sdk, self.root / 'not-invoked', self.manifest['files'])


    def test_missing_tool_roles_are_not_accepted(self):
        with self.assertRaisesRegex(ValueError, 'build tool roles'):
            bp.capture(self.stage, self.workspace, [], self.sdk, self.inputs)

    def test_missing_tool_evidence_is_not_accepted(self):
        self.manifest['tools'] = []
        self.write_fixture()
        with self.assertRaisesRegex(ValueError, 'build tool evidence'):
            self.verify()

    def test_unknown_source_origin_is_not_accepted(self):
        self.manifest['files'][0]['origin'] = 'unknown-synthetic'
        self.manifest['snapshot_sha256'] = bp.digest(bp.canonical(self.manifest['files']))
        self.write_fixture()
        with self.assertRaisesRegex(ValueError, 'input origin'):
            self.verify()

    def prepare_native_fixture(self):
        directory = self.stage / 'app/entry/.cxx/default/default/arm64-v8a'
        directory.mkdir(parents=True)
        compiler = self.sdk / 'llvm/bin/clang++'
        compiler.parent.mkdir(parents=True)
        compiler.write_text('isolated compiler path; never executed')
        self.command = dict(directory=str(directory), file=str(self.stage / 'native/synthetic.cpp'),
                            arguments=[str(compiler), '-c', str(self.stage / 'native/synthetic.cpp')])
        self.database = directory / 'compile_commands.json'
        self.database.write_bytes(bp.canonical([self.command]))
        self.deps = 'CMakeFiles/synthetic.o: #deps 2, deps mtime 1 (VALID)\n    ' + str(self.stage / 'native/synthetic.cpp') + '\n    ' + str(self.stage / 'native/synthetic.h') + '\n'

    def native_fixture_evidence(self, output=None):
        with patch.object(bp.subprocess, 'run', return_value=SimpleNamespace(stdout=self.deps if output is None else output)):
            return bp.native_evidence(self.stage, self.sdk, self.tool, self.manifest['files'])

    def test_actual_native_input_shape_with_isolated_ninja_output(self):
        self.prepare_native_fixture()
        evidence = self.native_fixture_evidence()
        self.assertEqual(evidence['object_count'], 1)
        self.assertEqual(len(evidence['dependencies']), 2)
        self.assertIn('$NATIVE_SDK', evidence['compiled_sources'][0]['command'])
        self.assertIn('$PROJECT', evidence['compiled_sources'][0]['command'])

    def test_stale_ninja_dependencies_are_not_accepted(self):
        self.prepare_native_fixture()
        with self.assertRaisesRegex(ValueError, 'stale native'):
            self.native_fixture_evidence(self.deps.replace('(VALID)', '(STALE)'))

    def test_missing_ninja_object_is_not_accepted(self):
        self.prepare_native_fixture()
        with self.assertRaisesRegex(ValueError, 'all compiled objects'):
            self.native_fixture_evidence('')

    def test_native_compile_header_change_is_not_accepted(self):
        self.prepare_native_fixture()
        (self.stage / 'native/synthetic.h').write_text('// changed fixture header')
        with self.assertRaisesRegex(ValueError, 'project dependency'):
            self.native_fixture_evidence()

    def test_unexpected_native_compiler_is_not_accepted(self):
        self.prepare_native_fixture()
        self.command['arguments'][0] = str(self.tool)
        self.database.write_bytes(bp.canonical([self.command]))
        with self.assertRaisesRegex(ValueError, 'Unexpected native compiler'):
            self.native_fixture_evidence()

    def test_duplicate_native_source_is_not_accepted(self):
        self.prepare_native_fixture()
        self.database.write_bytes(bp.canonical([self.command, self.command]))
        with self.assertRaisesRegex(ValueError, 'Duplicate compiled'):
            self.native_fixture_evidence()

    def test_unrecorded_native_project_header_is_not_accepted(self):
        self.prepare_native_fixture()
        path = self.stage / 'native/new-header.h'
        path.write_text('// isolated added header')
        with self.assertRaisesRegex(ValueError, 'project dependency'):
            self.native_fixture_evidence(self.deps + '    ' + str(path) + '\n')


if __name__ == '__main__':
    unittest.main()
