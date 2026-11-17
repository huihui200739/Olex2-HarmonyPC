"""Parse actual app reports, requiring complete consistent bounded fragments."""
import json
import re

def parse_app_reports(output):
    records=[]
    fragments={}
    for line in output.splitlines():
        if 'OLEX2_APP_REPORT ' in line:
            try: records.append(json.loads(line.split('OLEX2_APP_REPORT ',1)[1]))
            except (ValueError,TypeError): pass
        elif 'OLEX2_APP_FRAGMENT ' in line:
            match=re.fullmatch(r'([0-9]+-[0-9]+) (\d+) (\d+) (.*)',line.split('OLEX2_APP_FRAGMENT ',1)[1])
            if not match: continue
            key,index,count,text=match[1],int(match[2]),int(match[3]),match[4]
            if not 1<=count<=64 or not 0<=index<count or len(text)>900: continue
            value=fragments.setdefault(key,{'count':count,'parts':{},'invalid':False,'emitted':False})
            if value['count']!=count or (index in value['parts'] and value['parts'][index]!=text):value['invalid']=True
            value['parts'][index]=text
            if not value['invalid'] and not value['emitted'] and len(value['parts'])==count:
                value['emitted']=True
                try: records.append(json.loads(''.join(value['parts'][i] for i in range(count))))
                except (ValueError,TypeError): pass
    return records
