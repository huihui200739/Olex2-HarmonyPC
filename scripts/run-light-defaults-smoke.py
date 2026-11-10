#!/usr/bin/env python3
"""Run only the CPU light-default test on one explicitly named HDC target."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import uuid
from zoneinfo import ZoneInfo
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',required=True)
p.add_argument('--hdc',default='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc')
p.add_argument('--sdk',type=Path,default=Path('/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native'))
a=p.parse_args();token=uuid.uuid4().hex[:8]
report=ROOT/'reports/emulator'/('light-defaults-'+datetime.now(ZoneInfo('Asia/Shanghai')).strftime('%Y%m%d-%H%M%S')+'-'+token)
report.mkdir(parents=True);remote='/data/local/tmp/olex2-light-defaults-'+token
result=dict(target=a.target,scope='CPU original-light constructor/default/setter only; no GL context',physical_pc_verified=False,full_port_complete=False,checks=[],runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
def hdc(*args):
    done=subprocess.run([a.hdc,'-t',a.target,*map(str,args)],capture_output=True,text=True,timeout=60)
    if done.returncode:raise RuntimeError(done.stdout+done.stderr)
    return done.stdout+done.stderr
def shell(command):return hdc('shell',command)
try:
    shell('mkdir '+shlex.quote(remote))
    paths=[ROOT/'out/ohos-arm64/olex2_light_defaults_smoke',a.sdk/'llvm/lib/aarch64-linux-ohos/libc++_shared.so']
    for source in paths:
        retained=report/source.name;shutil.copy2(source,retained);sha=hashlib.sha256(retained.read_bytes()).hexdigest()
        (report/(source.name+'-transfer.log')).write_text(hdc('file','send',retained,remote+'/'+source.name))
        target_sha=shell('sha256sum '+shlex.quote(remote+'/'+source.name)).split()[0]
        result['checks'].append(dict(name=source.name+' target bytes match',passed=sha==target_sha,sha256=sha,target_sha256=target_sha))
        if sha!=target_sha:raise ValueError('Target CPU test input bytes differ')
    shell('chmod 700 '+shlex.quote(remote+'/olex2_light_defaults_smoke'))
    for run in range(1,4):
        command='cd '+shlex.quote(remote)+' && LD_LIBRARY_PATH=. ./olex2_light_defaults_smoke'
        marker='LIGHT_DEFAULTS_EXIT_'+token+'_'+str(run)
        output=shell('( '+command+' ); code=$?; printf "\\n'+marker+'=%s\\n" "$code"')
        (report/('run-'+str(run)+'.log')).write_text(output)
        result['checks'].append(dict(name='original light defaults repeat '+str(run),passed='PASS original light defaults/setter' in output and marker+'=0' in output))
    result['passed']=all(i['passed'] for i in result['checks'])
except Exception as error:result.update(passed=False,error=str(error))
finally:
    try:result['cleanup']=shell('rm -f '+remote+'/olex2_light_defaults_smoke '+remote+'/libc++_shared.so; rmdir '+remote)
    except Exception as error:result['cleanup_error']=str(error)
    (report/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'],report_directory=str(report),checks=len(result['checks'])),ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
