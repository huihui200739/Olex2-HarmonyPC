"""Strict public identity gate for the explicitly selected PC test emulator."""
PC_IDENTITY_QUERY='echo ARCH; uname -m; echo TYPE; param get const.product.devicetype; echo MODEL; param get const.product.model; echo API; param get const.ohos.apiversion; echo SYSTEM; param get const.ohos.fullname'
def validate_pc_emulator_identity(raw):
    if not isinstance(raw,str):raise ValueError('Missing explicit target identity')
    lines=[row.strip() for row in raw.splitlines()]
    result={}
    for marker,key in [('ARCH','architecture'),('TYPE','device_type'),('MODEL','model'),('API','api'),('SYSTEM','system')]:
        if lines.count(marker)!=1:raise ValueError('Missing/ambiguous target identity marker: '+marker)
        index=lines.index(marker)
        if index+1>=len(lines) or not lines[index+1] or lines[index+1] in ('ARCH','TYPE','MODEL','API','SYSTEM'):
            raise ValueError('Missing target identity value: '+marker)
        result[key]=lines[index+1]
    if (result['architecture'],result['device_type'],result['model'],result['api'])!=('aarch64','2in1','emulator','24'):
        raise ValueError('This evaluation requires the selected ARM64 PC/2in1 API24 emulator')
    return result
