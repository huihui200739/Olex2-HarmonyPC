"""Expected identity comes from the exact retained package, never the live app."""
import json
import re
import zipfile
from pathlib import Path

def package_identity(hap):
    with zipfile.ZipFile(Path(hap)) as package:
        app=json.loads(package.read('module.json'))['app']
    identity={key:app[key] for key in ('bundleName','versionName','versionCode')}
    if (not isinstance(identity['bundleName'],str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+',identity['bundleName'])
        or not isinstance(identity['versionName'],str) or not identity['versionName']
        or type(identity['versionCode']) is not int or identity['versionCode']<=0):
        raise ValueError('Package has no valid explicit identity')
    return identity

def installed_identity(expected):
    return dict(expected,error='',source='bundleManager.getBundleInfoForSelfSync')
