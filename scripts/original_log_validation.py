"""Independent byte/accounting checks for actual original log notifications.

Catalog text is a retained original-host metadata reference, not a claim that
all registered macros/functions execute correctly or that legacy code is globally
thread-safe. UI text must match those bytes, not only a native success flag.
"""
import hashlib
import re
LOG_SOURCE='upstream/sdl/log.cpp: actual TLog::OnPost notifications -> bounded UTF-8 in-memory observer; no severity or direct-stream capture claim'
CATALOG_SOURCE='upstream/xlib/xmacro.cpp: genuine LstMac/LstFun replicated under core.* and dispatched by original TEMacroLib; metadata only, full core registry unavailable'
EMPTY_HASH=hashlib.sha256(b'').hexdigest()
NATIVE_ASSERTIONS=1654
SAMPLE_NAMES=('original_log_empty_at_start','original_log_macros_from_actual_core','original_log_macros_visible_in_ui','original_log_clear_preserves_sequence','original_log_empty_is_visible','original_log_functions_from_actual_core','original_log_functions_visible_in_ui','original_log_survives_surface_recreation','original_log_cold_restart_without_replay')

def parse_catalog_log(text):
    if f'PASS original log bridge; {NATIVE_ASSERTIONS} assertions; 5 original severity cases; 2 real core catalog macros; 400 locally serialized caller posts;' not in text:
        raise ValueError('Missing exact original log smoke result')
    out={}
    for kind,count in (('macros',135),('functions',38)):
        rows=re.findall(r'^CATALOG_LINE '+kind+r' (\d+) ([0-9a-f]+)$',text,re.M)
        if len(rows)!=count or [int(i) for i,_ in rows]!=list(range(count)):
            raise ValueError('Catalog ordinal inventory differs')
        pieces=[bytes.fromhex(value).decode('utf-8') for _,value in rows]
        if not all(re.fullmatch(r'[^\n\x00]+ - [^\n\x00]+\n',line) for line in pieces):
            raise ValueError('Catalog raw UTF-8 line framing differs')
        raw=''.join(pieces).encode('utf-8');digest=hashlib.sha256(raw).hexdigest()
        declared=re.findall(r'^CATALOG_DIGEST '+kind+r' (\d+) (\d+) ([0-9a-f]{64})$',text,re.M)
        if declared!=[(str(count),str(len(raw)),digest)]:
            raise ValueError('Declared native catalog digest/bytes differ from raw lines')
        out[kind]=dict(text=raw.decode('utf-8'),sha256=digest,entry_count=count,bytes=len(raw),first=pieces[0],last=pieces[-1])
    return out

def audit_matches(a):
    try:
        if a['attached'] is not True or a['source']!=LOG_SOURCE or a['error']!='' or a['captureFailures']!=0:
            return False
        numeric=('posted','dropped','truncated','captureFailures','clearCount','clearedEntries','entryCount','retainedBytes')
        if not all(type(a[k]) is int and 0<=a[k]<=2**53-1 for k in numeric):
            return False
        if a['entryCount']>256 or a['retainedBytes']>65536 or a['posted']!=a['dropped']+a['clearedEntries']+a['entryCount'] or a['truncated']>a['posted']:
            return False
        if a['clearCount']==0 and a['clearedEntries']!=0:
            return False
        if not isinstance(a['textSha256'],str) or not re.fullmatch('[0-9a-f]{64}',a['textSha256']):
            return False
        if not all(isinstance(a[k],str) and len(a[k].encode('utf-8'))<=4096 for k in ('firstText','lastText')):
            return False
        if a['entryCount']==0:
            return a['retainedBytes']==0 and a['textSha256']==EMPTY_HASH and a['firstText']==a['lastText']==''
        return a['retainedBytes']>0 and a['firstText']!='' and a['lastText']!='' and (a['entryCount']!=1 or a['firstText']==a['lastText'])
    except (KeyError,TypeError,UnicodeError):
        return False

def empty_audit_matches(a,fresh=False):
    return audit_matches(a) and a['entryCount']==0 and (not fresh or all(a[k]==0 for k in ('posted','dropped','truncated','clearCount','clearedEntries')))

def catalog_sample_matches(d,reference,kind):
    try:
        a=d['log'];c=d['catalog'];r=reference[kind]
        return (audit_matches(a) and c['passed'] is True and c['error']=='' and c['source']==CATALOG_SOURCE and c['input']==('core.LstMac' if kind=='macros' else 'core.LstFun') and
            all(type(c[k]) is int and c[k]>=0 for k in ('sequence','exposedMacros','postedBefore','postedAfter')) and c['sequence']>0 and c['exposedMacros']==2 and
            c['postedAfter']-c['postedBefore']==r['entry_count'] and a['posted']==c['postedAfter'] and a['dropped']==a['truncated']==0 and
            (a['entryCount'],a['retainedBytes'],a['textSha256'],a['firstText'],a['lastText'])==(r['entry_count'],r['bytes'],r['sha256'],r['first'],r['last']))
    except (KeyError,TypeError):
        return False

def visible_catalog_matches(d,reference,kind):
    try:
        text=d['visible_text']
        return catalog_sample_matches(d,reference,kind) and isinstance(text,str) and text==reference[kind]['text'] and hashlib.sha256(text.encode('utf-8')).hexdigest()==d['log']['textSha256']
    except (KeyError,TypeError,UnicodeError):
        return False

def cleared_sample_matches(d):
    try:
        a=d['before'];b=d['after']
        return audit_matches(a) and empty_audit_matches(b) and b['posted']==a['posted'] and b['clearCount']==a['clearCount']+1 and b['clearedEntries']==a['clearedEntries']+a['entryCount'] and all(b[k]==a[k] for k in ('dropped','truncated','captureFailures'))
    except (KeyError,TypeError):
        return False

def restart_sample_matches(d):
    try:
        c=d['catalog'];return empty_audit_matches(d['log'],fresh=True) and str(d['prior_pid']).isdigit() and str(d['new_pid']).isdigit() and d['prior_pid']!=d['new_pid'] and c==dict(sequence=0,passed=False,exposedMacros=0,postedBefore=0,postedAfter=0,input='',error='',source='')
    except (KeyError,TypeError):
        return False

def app_original_log_evidence_matches(app,reference):
    try:
        if not isinstance(app,dict) or not isinstance(app.get('result'),str) or not app['result'].startswith('passed:'):
            return False
        checks={c['name']:c for c in app['checks']}
        if len(checks)!=len(app['checks']) or not all(checks[name]['passed'] is True for name in SAMPLE_NAMES):
            return False
        d=lambda name:checks['original_log_'+name]['details']
        return (empty_audit_matches(d('empty_at_start')['log'],fresh=True) and d('empty_at_start')['catalog']==dict(sequence=0,passed=False,exposedMacros=0,postedBefore=0,postedAfter=0,input='',error='',source='') and
            catalog_sample_matches(d('macros_from_actual_core'),reference,'macros') and visible_catalog_matches(d('macros_visible_in_ui'),reference,'macros') and
            cleared_sample_matches(d('clear_preserves_sequence')) and d('empty_is_visible')['visible_text']=='本次会话暂无原版日志' and empty_audit_matches(d('empty_is_visible')['log']) and
            catalog_sample_matches(d('functions_from_actual_core'),reference,'functions') and visible_catalog_matches(d('functions_visible_in_ui'),reference,'functions') and
            d('survives_surface_recreation')['log']==d('functions_from_actual_core')['log'] and d('survives_surface_recreation')['graphics']['surfaceReady'] is True and d('survives_surface_recreation')['graphics']['initialized'] is True and
            restart_sample_matches(d('cold_restart_without_replay')))
    except (KeyError,TypeError):
        return False
