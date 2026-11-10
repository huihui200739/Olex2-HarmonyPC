#!/usr/bin/env python3
"""Write a bounded integration summary only after rechecking retained evidence."""
import json
import re
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo
from build_provenance import record,verify,require
from original_renderer_integration_validation import validate_application
from renderer_probe_identity import package_identity
from vendor_sources import verify_vendor
ROOT=Path(__file__).resolve().parents[1]
def load(path):return json.loads((ROOT/path).read_text())
def retained(path):return record(ROOT/path,path)
def test_log(path):
    text=(ROOT/path).read_text()
    matches=re.findall(r'^Ran (\d+) tests in [\d.]+s$',text,re.M)
    require(len(matches)==1 and re.search(r'^OK$',text,re.M),'Incomplete/failed regression log: '+path)
    return dict(cases_passed=int(matches[0]),evidence=retained(path))

def main():
    hap=ROOT/'out/app/entry-default-unsigned.hap'
    identity=package_identity(hap);expected=load('app/AppScope/app.json5')['app']
    require(identity['versionName']=='0.22.0' and identity['versionCode']==220000,'This evidence writer is explicitly scoped to 0.22.0; revalidate new releases separately')
    require(all(identity[k]==expected[k] for k in identity),'Package identity differs from source')
    proof=verify(hap,hap.with_suffix('.provenance.json'),ROOT)
    app=load('reports/app/last-run.json');package=load('reports/app/package-verification.json')
    require(package['passed'] and package['build_provenance']==proof and package['hap_sha256']==app['hap']['sha256']==proof['hap_sha256'],'Current app/package/source evidence differs')
    integration=validate_application(app,ROOT,identity)
    require(integration==package['original_window_integration'],'Independent actual-window recheck differs')
    consistency=load('reports/adaptation-consistency.json')
    require(consistency['passed'] and len(consistency['checks'])==329,'Current consistency checks have not passed')
    vendor=verify_vendor(ROOT);sidecar=load('out/app/entry-default-unsigned.provenance.json')
    host='reports/app/graphics-0-22-verified-host-clean-build.log'
    require('100% tests passed, 0 tests failed out of 18' in (ROOT/host).read_text(),'Missing fresh 18-test host run')
    sources=['CMakeLists.txt','source.lock.json','vendor.lock.json','cmake/Olex2GraphicsRuntime.cmake','cmake/GLUSources.cmake','cmake/original_renderer.exports',
        'native/original_renderer_bridge.cpp','native/original_renderer_host.h','native/tests/light_defaults_smoke.cpp','native/tests/texture_header_compile.cpp',
        'patches/0015-include-explicit-gl-texture-interface.patch','patches/0016-initialize-default-light-exponent.patch',
        'scripts/build-app.sh','scripts/build_provenance.py','scripts/build_provenance_graphics.py','scripts/vendor_sources.py',
        'scripts/renderer_probe_identity.py','scripts/layout_validation.py','scripts/original_renderer_capture.py','scripts/original_renderer_click.py',
        'scripts/original_renderer_receive.py','scripts/original_renderer_validation.py','scripts/original_renderer_integration_validation.py','scripts/original_ui_checks.json',
        'scripts/run-app-smoke.py','scripts/run-light-defaults-smoke.py','scripts/check-app-package.py','scripts/check-adaptation-status.py','scripts/report-port-coverage.py',
        'scripts/test-original-renderer-validation.py','scripts/tests/test_build_provenance_graphics.py','scripts/tests/test_renderer_probe_identity.py',
        'scripts/tests/test_renderer_integration_summary.py','scripts/tests/test_status_consistency.py','scripts/write-original-renderer-integration-status.py',
        'app/AppScope/app.json5','app/entry/src/main/cpp/CMakeLists.txt','app/entry/src/main/cpp/renderer.cpp','app/entry/src/main/cpp/renderer.h',
        'app/entry/src/main/cpp/napi_init.cpp','app/entry/src/main/cpp/shared_original_probe.cpp','app/entry/src/main/cpp/shared_original_probe.h',
        'app/entry/src/main/cpp/fpe_shader_adapter.h','app/entry/src/main/ets/pages/Index.ets']
    light=load('reports/emulator/light-defaults-20261109-134219-6a8f99a0/result.json')
    require(light['passed'] and len(light['checks'])==5,'Light default target checks not passed')
    result=dict(checked_at=datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(timespec='seconds'),version=identity['versionName'],identity=identity,
        scope='Source integration into the diagnostic application only; not full Olex2/renderer certification',production_source_integrated=True,
        uses_experimental_prebuilt_dso=False,full_port_complete=False,functional_completion_percent=None,physical_pc_verified=False,
        physical_keyboard_mouse_verified=False,release_signing_verified=False,full_original_application_entry_integrated=False,
        full_original_ui_integrated=False,default_main_view_uses_complete_original_renderer=False,python_and_plugins_integrated=False,
        hap=retained('out/app/entry-default-unsigned.hap'),build_directory=str(Path((ROOT/'out/app/latest-build-directory.txt').read_text().strip()).relative_to(ROOT)),
        provenance=proof,original_window=integration,app_evidence=app['report_directory']+'/result.json',
        host=dict(ctest_cases_passed=18,build_log=host),light_defaults=dict(target_checks_passed=5,target_evidence='reports/emulator/light-defaults-20261109-134219-6a8f99a0/result.json'),
        vendor=vendor,patches=len(load('source.lock.json')['patches']),coverage=load('reports/port-coverage.json'),
        regressions=dict(python=test_log('out/all-python-tests-0.22-final-attempt1.log'),original_window_validator=test_log('out/original-window-validator-regressions-0.22.log')),
        consistency_checks_passed=len(consistency['checks']),native_library_export_counts={i['path']:len(i.get('exports',[])) for i in sidecar['native_libraries'] if i['origin']=='compiled'},
        sources=[retained(p) for p in sorted(sources)],
        verification_records=[retained(p) for p in ('reports/app/package-verification.json','reports/adaptation-consistency.json','reports/source-integrity.json')],
        retained_failures=['out/app-build-0.22-attempt1.log','out/schema2-synthetic-regressions-0.22.log','out/all-python-tests-0.22-attempt1.log',
            'out/package-check-0.22-initial-serialization-mismatch.log','out/package-check-0.22-catalog-framing-mismatch.log','out/status-consistency-0.22-before-patch-inventory-update.json'],
        limits=['Only three synthetic primitives in original renderer; default main view is existing GLES3 adapter.',
            'Four distinct PIDs and final same-process run 1/2/3; not long-duration or all-resource validation.',
            'Initial capability shader failures and limited generated FPE adaptation remain retained, not universal shader support.',
            'Standalone command-line desktop GL127/EGL3 failures remain partial; not replaced by window results.',
            'Target diagnostic data hashes matched transfers; direct installed protected HAP/DSO hashing was denied and is not claimed.',
            'C/C++ build traceability and packaged SHF_ALLOC matches do not certify reproducible bytes or formal signatures.'])
    require(all((ROOT/p).is_file() for p in result['retained_failures']),'Missing retained initial failure')
    (ROOT/'reports/original-renderer-integrated-status.json').write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n')
    print(identity['versionName'],integration['total_ui_checks'],len(result['sources']),'source snapshots; full port remains incomplete')
if __name__=='__main__':main()
