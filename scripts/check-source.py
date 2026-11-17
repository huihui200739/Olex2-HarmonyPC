#!/usr/bin/env python3
"""Verify the pinned upstream, declared patches, reused geometry header and licence."""
from datetime import datetime
import hashlib
import json
import re
from pathlib import Path
import subprocess
import tempfile
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
lock = json.loads((ROOT / 'source.lock.json').read_text())
checks = []

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT / 'upstream'), *args])

def check(name, condition, **details):
    checks.append({'name': name, 'passed': bool(condition), **details})

try:
    head = git('rev-parse', 'HEAD').decode().strip()
    check('pinned_commit', head == lock['commit'], actual=head, expected=lock['commit'])
    records = [(lock['license_file'], lock['license_sha256'])]
    records += [(r['file'], r['sha256']) for r in lock['patches']]
    records += [('upstream/' + r['file'], r['sha256']) for r in lock['patched_files']]
    for path, expected in records:
        actual = hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
        check(path, actual == expected, sha256=actual, expected_sha256=expected)
    allowed = sorted(r['file'] for r in lock['patched_files'])
    declared_new = sorted(r['file'] for r in lock['patched_files'] if r.get('new_file', False))
    tracked_changes = git('diff', '--name-only', 'HEAD').decode().splitlines()
    untracked = git('ls-files', '--others', '--exclude-standard').decode().splitlines()
    changed = sorted(set(tracked_changes + untracked))
    check('only_declared_upstream_modifications', changed == allowed, actual=changed, expected=allowed)
    unexpected = sorted(set(untracked) - set(declared_new))
    check('no_undeclared_untracked_upstream_sources', not unexpected, actual=unexpected)
    for added in declared_new:
        absent = subprocess.run(['git', '-C', str(ROOT / 'upstream'), 'cat-file', '-e', lock['commit'] + ':' + added], capture_output=True)
        check('new_header_absent_in_pinned_commit:' + added, absent.returncode != 0)
    # Reconstruct only the declared source files, without staging or touching upstream.
    # Reverse applicability alone would not detect extra edits outside patch hunks.
    (ROOT / 'out').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='source-replay-', dir=ROOT / 'out') as directory:
        replay = Path(directory)
        for record in lock['patched_files']:
            if not record.get('new_file', False):
                target = replay / record['file']
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(git('show', lock['commit'] + ':' + record['file']))
        applied = True
        for patch in lock['patches']:
            result = subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'apply',
                '--whitespace=error-all', str(ROOT / patch['file'])], cwd=replay, capture_output=True, text=True)
            check('forward_replay:' + patch['file'], result.returncode == 0, output=result.stdout + result.stderr)
            if result.returncode != 0:
                applied = False
                break
        if applied:
            replay_files = sorted(str(p.relative_to(replay)) for p in replay.rglob('*') if p.is_file())
            check('replay_only_declared_files', replay_files == allowed, actual=replay_files, expected=allowed)
            for record in lock['patched_files']:
                target = replay / record['file']
                check('replay_exact_bytes:' + record['file'], target.is_file() and
                    target.read_bytes() == (ROOT / 'upstream' / record['file']).read_bytes())
            # Patches may intentionally overlap. Reverse the complete chain in
            # reverse order inside this private replay, not against upstream.
            reversed_all = True
            for patch in reversed(lock['patches']):
                result = subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'apply', '--reverse',
                    '--whitespace=error-all', str(ROOT / patch['file'])], cwd=replay, capture_output=True, text=True)
                check('reverse_replay:' + patch['file'], result.returncode == 0, output=result.stdout + result.stderr)
                if result.returncode != 0:
                    reversed_all = False
                    break
            if reversed_all:
                for record in lock['patched_files']:
                    target = replay / record['file']
                    original_matches = not target.exists() if record.get('new_file', False) else target.is_file() and target.read_bytes() == git('show', lock['commit'] + ':' + record['file'])
                    check('reverse_returns_to_pinned_bytes:' + record['file'], original_matches)
    hashed_original = git('show', lock['commit'] + ':sdl/ehashed.h')
    hashed_expected = hashed_original.replace(b'  void init() {\n    basket_n = 0;\n  }', b'  void init() {\n    count = basket_n = 0;\n  }').replace(b'  void Clear() {\n    data.clear();\n  }', b'  void Clear() {\n    data.clear();\n    init();\n  }')
    check('original_hashed_container_counter_fix_only', (ROOT / 'upstream/sdl/ehashed.h').read_bytes() == hashed_expected)
    settings_header = git('show', lock['commit'] + ':sdl/settingsfile.h')
    settings_eol = b'\r\n' if b'\r\n' in settings_header else b'\n'
    settings_header_expected = settings_header.replace(b'protected:' + settings_eol,
        b'protected:' + settings_eol + b'  void LoadSettings_(const olxstr& fileName, bool utf8);' + settings_eol).replace(
        b'  void LoadSettings(const olxstr& fileName);', b'  void LoadSettings(const olxstr& fileName);' + settings_eol +
        b'  // Explicit UTF-8 reader; legacy LoadSettings behavior stays unchanged.' + settings_eol + b'  void LoadUtf8Settings(const olxstr& fileName);')
    settings_original = git('show', lock['commit'] + ':sdl/settingsfile.cpp')
    settings_expected = settings_original.replace(b'void TSettingsFile::LoadSettings(const olxstr& fileName)  {\n',
        b'void TSettingsFile::LoadSettings(const olxstr& fileName)  {\n  LoadSettings_(fileName, false);\n}\nvoid TSettingsFile::LoadUtf8Settings(const olxstr& fileName)  {\n  LoadSettings_(fileName, true);\n}\nvoid TSettingsFile::LoadSettings_(const olxstr& fileName, bool utf8)  {\n').replace(
        b'  TEFile::ReadLines(fileName, Lines);', b'  if (utf8) {\n    const TStrList decoded = TUtf8File::ReadLines(fileName, false);\n    for (size_t i = 0; i < decoded.Count(); i++) {\n      Lines.Add(decoded[i], false);\n    }\n  }\n  else {\n    TEFile::ReadLines(fileName, Lines);\n  }')
    check('original_settings_explicit_utf8_reader_only', (ROOT / 'upstream/sdl/settingsfile.h').read_bytes() == settings_header_expected and
        (ROOT / 'upstream/sdl/settingsfile.cpp').read_bytes() == settings_expected)
    check('original_settings_save_body_unchanged', (ROOT / 'upstream/sdl/settingsfile.cpp').read_bytes().split(b'void TSettingsFile::SaveSettings', 1)[1] ==
        settings_original.split(b'void TSettingsFile::SaveSettings', 1)[1])
    basic_header = git('show', lock['commit'] + ':sdl/bapp.h')
    check('original_basic_app_save_hook_only', (ROOT / 'upstream/sdl/bapp.h').read_bytes() == basic_header.replace(b'  void SaveOptions() const;', b'  virtual void SaveOptions() const;'))
    check('original_basic_app_implementation_unchanged', (ROOT / 'upstream/sdl/bapp.cpp').read_bytes() == git('show', lock['commit'] + ':sdl/bapp.cpp'))
    material_original = git('show', lock['commit'] + ':glib/glmaterial.cpp')
    init_start = material_original.index(b'void TGlMaterial::Init(bool skip) const {')
    init_end = material_original.index(b'//..............................................................................', init_start)
    extracted_init = material_original[init_start:init_end]
    material_expected = material_original[:init_start] + b'#ifndef OLEX2_MATERIAL_VALUES_ONLY\n#include "glmaterial_legacy_init.h"\n#endif\n' + material_original[init_end:]
    check('material_value_methods_preserved_exactly', (ROOT / 'upstream/glib/glmaterial.cpp').read_bytes() == material_expected)
    init_header = (ROOT / 'upstream/glib/glmaterial_legacy_init.h').read_bytes()
    check('desktop_material_init_body_preserved_exactly', init_header[init_header.index(b'void TGlMaterial::Init'):init_header.rindex(b'#endif')] == extracted_init)
    # Independently invert only API/field spelling substitutions, then require
    # exact original control-flow text, including the commented lighting branch.
    portable = (ROOT / 'upstream/glib/materialcommands.h').read_text()
    portable = portable[portable.index('template <class Sink>'):portable.index('EndGlNamespace()')].rstrip() + '\n'
    portable = portable.replace('template <class Sink>\nvoid ApplyMaterialCommands(const TGlMaterial& value, Sink& api, bool skip) {', 'void TGlMaterial::Init(bool skip) const {')
    portable = portable.replace('api.IsEnabled(', 'glIsEnabled(').replace('api.', 'olx_gl::')
    portable = portable.replace('value.GetFlags()', 'Flags').replace('value.', '')
    enum_names = {
        'MaterialCapability::AlphaTest':'GL_ALPHA_TEST', 'MaterialCapability::PointSmooth':'GL_POINT_SMOOTH',
        'MaterialCapability::Blend':'GL_BLEND', 'MaterialCapability::CullFace':'GL_CULL_FACE',
        'MaterialCapability::Lighting':'GL_LIGHTING', 'MaterialCapability::ColorMaterial':'GL_COLOR_MATERIAL',
        'MaterialFace::Front':'GL_FRONT', 'MaterialFace::Back':'GL_BACK', 'MaterialFace::Both':'GL_FRONT_AND_BACK',
        'MaterialField::Ambient':'GL_AMBIENT', 'MaterialField::Diffuse':'GL_DIFFUSE',
        'MaterialField::Specular':'GL_SPECULAR', 'MaterialField::Emission':'GL_EMISSION',
        'MaterialField::Shininess':'GL_SHININESS', 'MaterialField::AmbientAndDiffuse':'GL_AMBIENT_AND_DIFFUSE'}
    for before in sorted(enum_names,key=len,reverse=True): portable=portable.replace(before,enum_names[before])
    check('portable_material_init_preserves_original_command_flow', portable == extracted_init.decode().replace('\r\n','\n').rstrip()+'\n')
    primitive_original = git('show', lock['commit'] + ':glib/glprimitive.cpp').decode().replace('\r\n','\n')
    begin = primitive_original.index('    PrepareColorRendering(GL_TRIANGLES);') + len('    PrepareColorRendering(GL_TRIANGLES);')
    end = primitive_original.index('    EndColorRendering();', begin)
    triangle_header = (ROOT / 'upstream/glib/trianglesubmit.h').read_text()
    portable_triangles = triangle_header[triangle_header.index('{', triangle_header.index('void SubmitPrimitiveTriangles'))+1:triangle_header.index('EndGlNamespace()')]
    portable_triangles = portable_triangles.rsplit('}',1)[0]
    portable_triangles = portable_triangles.replace('sink.IsSelecting()', 'GetRenderer().IsSelecting()')
    for name in ('SetNormal','DrawVertex3c','DrawVertex3','DrawVertex'):
        portable_triangles = portable_triangles.replace('sink.'+name+'(',name+'(')
    check('portable_original_triangle_branch_preserved_exactly', portable_triangles == primitive_original[begin:end])
    line_begin=primitive_original.index('    float LW = 0;',primitive_original.index('  case sgloLines:'))
    line_end=primitive_original.index('    EndColorRendering();',line_begin)
    line_header=(ROOT/'upstream/glib/linesubmit.h').read_text()
    line_body=line_header[line_header.index('{',line_header.index('void SubmitPrimitiveLines'))+1:line_header.index('EndGlNamespace()')].rsplit('}',1)[0]
    line_body=line_body.replace('sink.GetLineWidth(&LW);','olx_gl::get(GL_LINE_WIDTH, &LW);').replace('sink.SetLineWidth(','olx_gl::lineWidth(').replace('sink.IsSelecting()','GetRenderer().IsSelecting()')
    for name in ('DrawVertex2','DrawVertex','SetColor'):
        line_body=line_body.replace('sink.'+name+'(',name+'(')
    check('portable_original_line_branch_preserved_exactly',line_body==primitive_original[line_begin-1:line_end])
    current_primitive=(ROOT/'upstream/glib/glprimitive.cpp').read_text()
    path_header=(ROOT/'upstream/glib/linepathsubmit.h').read_text()
    for branch,mode,name in (('sgloLineStrip','STRIP','SubmitPrimitiveLineStrip'),('sgloLineLoop','LOOP','SubmitPrimitiveLineLoop')):
        begin=primitive_original.index('    float LW = 0;',primitive_original.index('  case '+branch+':'))-1
        end=primitive_original.index('    EndColorRendering();',begin)
        a=path_header.index('{',path_header.index('void '+name))+1;cursor=a;depth=1
        while depth:
            depth+=(path_header[cursor]=='{')-(path_header[cursor]=='}');cursor+=1
        body=path_header[a:cursor-1]
        body=body.replace('sink.GetLineWidth(&LW)','olx_gl::get(GL_LINE_WIDTH, &LW)').replace('sink.SetLineWidth(','olx_gl::lineWidth(').replace('sink.IsSelecting()','GetRenderer().IsSelecting()')
        for command in ('SetColor','DrawVertex'):body=body.replace('sink.'+command+'(',command+'(')
        check('portable_original_line_path_branch_preserved_exactly:'+branch,body==primitive_original[begin:end])
        start=current_primitive.index('  case '+branch+': {');stop=current_primitive.index('\n  case ',start+1)
        expected='  case '+branch+': {\n    PrepareColorRendering(GL_LINE_'+mode+');\n    LineSubmissionSink sink(*this);\n    '+name+'(Vertices, Colors, Params, sink);\n    EndColorRendering();\n    break;\n  }'
        check('desktop_original_line_path_wrapper_preserved:'+branch,current_primitive[start:stop]==expected)
    polygon_begin=primitive_original.index('    if (Normals.Count() == 1)',primitive_original.index('  case sgloPolygon:'))
    polygon_end=primitive_original.index('    EndColorRendering();',polygon_begin)
    polygon_header=(ROOT/'upstream/glib/polygonsubmit.h').read_text()
    polygon_body=polygon_header[polygon_header.index('{',polygon_header.index('void SubmitPrimitivePolygon'))+1:polygon_header.index('EndGlNamespace()')].rsplit('}',1)[0]
    polygon_body=polygon_body.replace('sink.IsSelecting()','GetRenderer().IsSelecting()').replace('sink.SetNormal(','SetNormal(').replace('sink.DrawVertex(','DrawVertex(')
    check('portable_original_polygon_branch_preserved_exactly',polygon_body==primitive_original[polygon_begin-1:polygon_end])
    basis_original=git('show',lock['commit']+':sdl/ebasis.h').decode().replace('\r\n','\n')
    basis_current=(ROOT/'upstream/sdl/ebasis.h').read_text()
    def method_body(text,signature):
        start=text.index('{',text.index(signature))+1
        end=text.index('\n  }',start)
        return text[start:end]
    for getter,copy,array in [('GetMData','CopyMData','FMData'),('GetMDataT','CopyMDataT','FMDataT')]:
        old=method_body(basis_original,'const float* '+getter+'()')
        expected=old.replace(array,'data').replace('    return data;','')
        actual=method_body(basis_current,'void '+copy+'(float* data)')
        normalize=lambda text:re.sub(r'\s+','',text)
        check('owned_basis_export_preserves_original_assignments:'+copy,normalize(actual)==normalize(expected))
        check('legacy_basis_getter_retains_static_buffer:'+getter,normalize(method_body(basis_current,'const float* '+getter+'()'))==copy+'('+array+');return'+array+';')
    check('original_basis_rotation_implementation_unmodified',(ROOT/'upstream/sdl/ebasis.cpp').read_bytes()==git('show',lock['commit']+':sdl/ebasis.cpp'))
    reused_color = 'glib/gloption.cpp'
    check('unmodified_upstream_color_implementation', (ROOT / 'upstream' / reused_color).read_bytes() == git('show', lock['commit'] + ':' + reused_color))
    reused = 'sdl/esphere.h'
    original = git('show', lock['commit'] + ':' + reused)
    current = (ROOT / 'upstream' / reused).read_bytes()
    check('unmodified_upstream_geometry_header', current == original,
          path='upstream/' + reused, sha256=hashlib.sha256(current).hexdigest())
except Exception as error:
    check('verification_completed', False, error=f'{type(error).__name__}: {error}')
report = {'checked_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),
          'checks': checks, 'passed': bool(checks) and all(c['passed'] for c in checks)}
(ROOT / 'reports/source-integrity.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(0 if report['passed'] else 1)
