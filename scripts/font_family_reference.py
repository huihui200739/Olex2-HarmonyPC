"""Independent strict description/catalog model; never claims font rasterization."""
import re

def require(value,message):
    if not value:raise ValueError(message)

def name(value,limit,empty=False):
    require(type(value) is str and (bool(value) or empty), 'Invalid font name')
    try:raw=value.encode('utf-8')
    except UnicodeError as error:raise ValueError('Invalid UTF8 font name') from error
    require(len(raw)<=limit and all(ord(c)>=32 and ord(c)!=127 for c in value),'Invalid bounded font name')
    return value

def description(family,style,points=18,fixed=False):
    name(family,512);name(style['name'],256,True)
    require(type(points) is int and 1<=points<=128 and type(fixed) is bool,'Invalid font size/fixed mode')
    for key,low,high in (('weight_code',0,1000),('width_code',1,9),('slant_code',0,2)):
        require(type(style[key]) is int and low<=style[key]<=high,'Invalid SDK raw style code')
    fields=['ohos-family','v1',str(points),str(style['weight_code']),str(style['width_code']),str(style['slant_code']),family.encode().hex(),style['name'].encode().hex()]
    return ':'.join(fields)+(':fixed' if fixed else '')

def parse_description(text):
    require(type(text) is str and len(text)<=1650,'Invalid bounded family description')
    parts=text.split(':')
    require(len(parts) in (8,9) and parts[:2]==['ohos-family','v1'] and (len(parts)==8 or parts[8]=='fixed'),'Invalid family description format')
    require(all(re.fullmatch(r'0|[1-9][0-9]{0,3}',item) for item in parts[2:6]),'Noncanonical style numbers')
    require(all(len(item)%2==0 and re.fullmatch('[0-9a-f]*',item) is not None for item in parts[6:8]),'Noncanonical encoded name')
    try:family,label=[bytes.fromhex(item).decode('utf-8') for item in parts[6:8]]
    except (UnicodeError,ValueError) as error:raise ValueError('Invalid encoded UTF8 font name') from error
    style=dict(name=label,weight_code=int(parts[3]),width_code=int(parts[4]),slant_code=int(parts[5]))
    points=int(parts[2]);fixed=len(parts)==9
    require(description(family,style,points,fixed)==text,'Family font description does not round trip')
    return dict(family=family,style=style,points=points,fixed=fixed)

def validate_catalog(value):
    require(value['source']=='OH_Drawing_FontMgr' and value['closest_match_used'] is False and value['weight_codes_are_SDK_raw_values'] is True,'Unexpected font catalog contract')
    families=value['families'];require(type(families) is list and len(families)<=4096,'Invalid bounded family inventory')
    total=0
    for index,family in enumerate(families):
        require(type(family['index']) is int and family['index']==index,'Native catalog family index differs')
        name(family['name'],512);styles=family['styles'];require(type(styles) is list and len(styles)<=1024,'Invalid bounded style inventory')
        total+=len(styles);require(total<=16384,'Native style inventory too large')
        for ordinal,style in enumerate(styles):
            require(type(style['index']) is int and style['index']==ordinal,'Native style ordinal differs')
            description(family['name'],style)
    return families

def probe_selection(value):
    families=validate_catalog(value)
    first=next((f for f in families if len(f['styles'])>1),None)
    require(first is not None,'Two actual styles in one installed family required')
    other=next((f for f in families if f['name']!=first['name'] and f['styles']),None)
    require(other is not None,'Two actual installed families required')
    return [(first,first['styles'][0])]*3+[(other,other['styles'][0]),(first,first['styles'][1])]
