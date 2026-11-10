"""Independent checks for installed identity and actual restricted desktop input.

Only the two existing interpreters are exercised: app.ConfigDir() and pc.help.
This does not qualify the full command registry or physical keyboard handling.
"""
from application_runtime_validation import SOURCE as APPLICATION_SOURCE, runtime_matches
from desktop_command_validation import SOURCE as VIEW_SOURCE

SAMPLES = (
    'installed_package_identity_from_runtime', 'installed_package_version_visible',
    'desktop_command_labels_visible', 'desktop_enter_application_interpreter',
    'desktop_enter_view_interpreter', 'drawing_paused_visible_in_records',
)
IDENTITY_SOURCE = 'bundleManager.getBundleInfoForSelfSync'


def identity_matches(identity, expected):
    try:
        return (identity == dict(bundleName=expected['bundleName'], versionName=expected['versionName'],
                                 versionCode=expected['versionCode'], error='', source=IDENTITY_SOURCE)
                and type(identity['versionCode']) is int)
    except (KeyError, TypeError):
        return False


def version_visible_matches(sample, expected):
    try:
        return (identity_matches(sample['identity'], expected)
                and sample['visible_text'] == '开发验证版  /  ' + expected['versionName'])
    except (KeyError, TypeError):
        return False


def labels_match(sample):
    try:
        parent = sample['entry']
        if not (len(parent) == 4 and all(type(v) is int for v in parent)
                and parent[2] > parent[0] and parent[3] > parent[1]):
            return False
        width = parent[2] - parent[0]
        for name, text in (('view', '执行视图'), ('application', '应用函数'), ('help', '命令帮助')):
            item = sample[name]
            box = item['bounds']
            if (item['text'] != text or item['visible'] != 'true' or len(box) != 4
                    or not all(type(v) is int for v in box)
                    or not (parent[0] <= box[0] < box[2] <= parent[2]
                            and parent[1] <= box[1] < box[3] <= parent[3])
                    or box[2] - box[0] < width * (0.85 if name == 'help' else 0.4)):
                return False
        return sample['help']['bounds'][1] >= max(sample[n]['bounds'][3] for n in ('view', 'application'))
    except (KeyError, TypeError, IndexError):
        return False


def enter_matches(sample, route):
    try:
        before, after = sample['before'], sample['after']
        if (sample['observed_input'] != ('app.ConfigDir()' if route == 'application' else 'pc.help')
                or sample['injected_keycode'] != 2054
                or not runtime_matches(before['applicationRuntime'])
                or before['applicationRuntime'] != after['applicationRuntime']):
            return False
        field, other = ('applicationFunction', 'desktopCommand') if route == 'application' else ('desktopCommand', 'applicationFunction')
        old, actual = before[field], after[field]
        if (type(old['sequence']) is not int or actual['sequence'] != old['sequence'] + 1
                or after[other] != before[other]
                or actual['input'] != sample['observed_input'] or actual['passed'] is not True or actual['error'] != ''):
            return False
        if route == 'application':
            return (actual['source'] == APPLICATION_SOURCE and actual['optionsChanged'] is False
                    and actual['text'] == after['applicationRuntime']['configDir'][:-1])  # original BAPP_ConfigDir removes exactly one trailing delimiter
        return (route == 'view' and actual['source'] == VIEW_SOURCE and actual['changed'] is False
                and actual['applied'] is False and actual['dispatched'] == 1 and actual['message'].startswith('pc.mesh sphere|torus')
                and 'New platform view adapters only, not the complete Olex2 command library.' in actual['message'])
    except (KeyError, TypeError):
        return False


def identity_snapshots(value):
    if isinstance(value, dict):
        if 'applicationRuntime' in value:
            yield value.get('installedPackage')
        for child in value.values():
            yield from identity_snapshots(child)
    elif isinstance(value, list):
        for child in value:
            yield from identity_snapshots(child)


def app_shell_evidence_matches(app, expected):
    try:
        checks = {c['name']: c for c in app['checks']}
        if (not app['result'].startswith('passed:') or len(checks) != len(app['checks'])
                or not all(checks[name]['passed'] is True for name in SAMPLES)):
            return False
        get = lambda name: checks[name]['details']
        snapshots = list(identity_snapshots(app))
        return (bool(snapshots) and all(identity_matches(v, expected) for v in snapshots)
                and identity_matches(get(SAMPLES[0])['identity'], expected)
                and version_visible_matches(get(SAMPLES[1]), expected)
                and labels_match(get(SAMPLES[2]))
                and enter_matches(get(SAMPLES[3]), 'application')
                and enter_matches(get(SAMPLES[4]), 'view')
                and get(SAMPLES[5]) == dict(visible_text='绘图已暂停', active_panel='运行记录'))
    except (KeyError, TypeError, AttributeError):
        return False
