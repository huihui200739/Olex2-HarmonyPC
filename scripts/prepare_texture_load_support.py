"""Exact current-process UI capture profile for the existing GX diagnostic owner."""
def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Unexpected capture source shape: '+old[:100])
    return text.replace(old, new, 1)

def capture_profile(root):
    text = (root/'scripts/original_renderer_capture.py').read_text()
    text = once(text, "r.get('originalCoreMacroCount')==135 and r.get('originalCoreFunctionCount')==38",
                "r.get('originalCoreMacroCount')==193 and r.get('originalCoreFunctionCount')==46")
    text = once(text, 'from renderer_probe_identity import package_identity,installed_identity',
                'from renderer_probe_identity import package_identity,installed_identity\nfrom app_report_log import parse_app_reports')
    text = once(text, '  d=json.loads(last)',
                "  d=json.loads(last)\n  live_pid=shell('pidof '+BUNDLE).strip()\n  current_logs=shell('hilog -x -P '+live_pid+' -T Olex2Native') if live_pid.isdigit() else ''\n  active_reports=parse_app_reports(current_logs)")
    text = once(text, "  if d.get('graphics',{}).get('frames',0)>0",
                "  if d.get('installedPackage')==installed_identity(expected_identity) and d in active_reports and d.get('graphics',{}).get('frames',0)>0")
    return once(text, "(out/'native-verification.json').write_text(last);",
                "(out/'process-scoped-report.log').write_text(current_logs);(out/'native-verification.json').write_text(last);")
