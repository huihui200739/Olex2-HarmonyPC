"""Strict new27/new28 overlay checks, never runtime qualification."""
import hashlib
import json
from pathlib import Path

FILES27=frozenset('''native/display_font_metrics.h
app/entry/src/main/ets/model/FontDisplayMetrics.ts
app/entry/src/main/ets/model/MainWindowFontMetrics.ets
native/original_renderer_host.h
native/harmony_glyph_raster.h
native/harmony_glyph_raster.cpp
native/platform_font_factory.h
native/platform_font_factory.cpp
native/platform_window_scene.h
native/original_font_window_probe.cpp
native/family_font_window_probe.cpp
app/entry/src/main/cpp/shared_original_probe.h
app/entry/src/main/cpp/shared_original_probe.cpp
app/entry/src/main/cpp/renderer.h
app/entry/src/main/cpp/renderer.cpp
app/entry/src/main/cpp/napi_init.cpp
app/entry/src/main/cpp/types/libolex2native/index.d.ts
app/entry/src/main/ets/entryability/EntryAbility.ets
app/entry/src/main/ets/pages/Index.ets
app/AppScope/app.json5'''.splitlines())
FILES28=frozenset(('native/family_font_window_probe.cpp','app/entry/src/main/cpp/napi_init.cpp',
                   'app/entry/src/main/cpp/types/libolex2native/index.d.ts','app/AppScope/app.json5'))

def _require(value,message):
    if not value:raise ValueError(message)

def _files(source,meta,expected):
    _require(isinstance(meta.get('files'),dict) and meta['files'].keys()==expected,'Exact overlay file set required')
    for name,record in meta['files'].items():
        path=source/name
        _require(path.resolve().is_relative_to(source.resolve()) and not path.is_symlink(),'Overlay path escaped source')
        data=path.read_bytes()
        _require(len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],'Changed exact overlay input: '+name)

def validate_source27(source,app_version='0.22.27'):
    source=Path(source).resolve();meta=json.loads((source/'font-display-inputs.json').read_text())
    _require(meta['version']=='0.22.27' and meta['base']=='0.22.26' and meta['original_font_requests_per_action']==20,'Exact27 underlay metadata required')
    _require(meta['base_manifest_sha256']==hashlib.sha256((source/'font-family-inputs.json').read_bytes()).hexdigest(),'Exact26 family underlay manifest changed')
    # New28 explicitly overrides four27 files, but must not rewrite the frozen27 metadata.
    expected=FILES27 if app_version=='0.22.27' else FILES27-FILES28
    if app_version=='0.22.27':_files(source,meta,FILES27)
    else:
        _require(meta['files'].keys()==FILES27,'Exact27 overlay file set required')
        reduced=dict(files={name:meta['files'][name] for name in expected});_files(source,reduced,expected)
    app=json.loads((source/'app/AppScope/app.json5').read_text())['app']
    code={'0.22.27':222700,'0.22.28':222800}[app_version]
    _require(app['bundleName']=='com.example.olex2harmonypc' and app['versionName']==app_version and app['versionCode']==code,'Actual display candidate identity differs')
    return source

def validate_source28(source):
    source=validate_source27(source,app_version='0.22.28');meta=json.loads((source/'font-display-telemetry-inputs.json').read_text())
    _require(meta['version']=='0.22.28' and meta['base']=='0.22.27' and meta['original_font_requests_per_action']==20,'Exact28 telemetry metadata required')
    _require(meta['base_manifest_sha256']==hashlib.sha256((source/'font-display-inputs.json').read_bytes()).hexdigest(),'Frozen27 display underlay manifest changed')
    _files(source,meta,FILES28);return source
