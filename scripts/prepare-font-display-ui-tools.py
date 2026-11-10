#!/usr/bin/env python3
"""Prepare exact28 own212 UI tools, without changing frozen native inputs."""
import argparse,hashlib,json,shutil
from pathlib import Path
from font_display_tool_profiles import ui_preparation_source,strict_ui_install_source
from font_display_window_validation import validate_source
ROOT=Path(__file__).resolve().parents[1]

def prepare(source,out,hap,host_log):
    validate_source(source)
    baseline=ROOT/'scripts/prepare-texture-context-ui-tools.py'
    text=ui_preparation_source()
    namespace=dict(__name__='candidate28_exact212_tool_recipe',__file__=str(baseline))
    exec(compile(text,str(baseline)+' [exact28 identity/input]','exec'),namespace)
    namespace['prepare'](source,out,hap,host_log)
    runner=out/'scripts/run-app-smoke.py';runner.write_text(strict_ui_install_source(runner.read_text()))
    shutil.copy2(ROOT/'scripts/hdc_install_validation.py',out/'scripts/hdc_install_validation.py')
    path=out.resolve()/'tool-profile.json';manifest=json.loads(path.read_text())
    manifest['candidate_profile']=dict(prior='0.22.14',current='0.22.28',prior_212_checks_not_removed=True,
        native_inputs_unchanged=True,strict_install_acknowledgement_required=True,
        recipe_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),
        profile_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    manifest['files']={str(p.relative_to(out)):dict(sha256=hashlib.sha256(p.read_bytes()).hexdigest(),bytes=p.stat().st_size)
        for parent in (out/'scripts',out/'profile-baseline',out/'app') for p in sorted(parent.rglob('*'))
        if p.is_file() and '__pycache__' not in p.parts}
    path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    (out/'executed-preparation-profile.py').write_text(text)
    for name in ('prepare-font-display-ui-tools.py','font_display_tool_profiles.py'):
        (out/name).write_bytes((ROOT/'scripts'/name).read_bytes())

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','output','hap','host-log'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();prepare(a.source,a.output,a.hap,a.host_log)
