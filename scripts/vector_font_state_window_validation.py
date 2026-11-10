"""Exact21 real GLint mask observations; legacy and raw boolean values retained."""
import hashlib
import json
from pathlib import Path
from original_vector_font_window_validation import require

ROOT=Path(__file__).resolve().parents[1]


def validated_mask(gpu):
 for key in ('color_write','color_write_raw_boolean','color_write_integer'):
  values=gpu.get(key)
  require(isinstance(values,list) and len(values)==4 and all(type(v) is int and v in (0,1) for v in values),
          'Missing or invalid independent mask observation: '+key)
 return gpu['color_write_integer']==[1,1,1,1]


_base=ROOT/'scripts/original_vector_font_window_validation.py'
_text=_base.read_text()
for old,new in (("versionName='0.22.20',versionCode=222000","versionName='0.22.21',versionCode=222100"),
                ("gpu['color_write']==[1,1,1,1]","validated_mask(gpu)")):
 if _text.count(old)!=1:raise ValueError('Unexpected frozen20 state profile: '+old)
 _text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate21_same_native_and_vector_gates_explicit_integer_mask',__file__=str(_base),validated_mask=validated_mask)
exec(compile(_text,str(_base)+' [exact21 actual GLint mask]','exec'),_namespace)
IDENTITY=_namespace['IDENTITY']


def validate(folder,previous=None,source=None):
 require(source is not None,'Bind exact21 source')
 source,folder=Path(source),Path(folder)
 manifest=json.loads((source/'vector-font-state-inputs.json').read_text())
 require(manifest['version']=='0.22.21','Only exact21 mask-query candidate accepted')
 for name,row in manifest['files'].items():
  data=(source/name).read_bytes()
  require(len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],'Frozen21 mask capture changed: '+name)
 result=_namespace['validate'](folder,previous,source)
 result['mask_observations']=[]
 for case in result['vector_cases']:
  for route in ('actual','reference'):
   gpu=json.loads((folder/f'actual-gpu-run-{result["run"]}-vector-{case["label"]}-{route}.json').read_text())
   result['mask_observations'].append(dict(label=case['label'],route=route,
       legacy_boolean=gpu['color_write'],raw_symbol_boolean=gpu['color_write_raw_boolean'],
       actual_integer=gpu['color_write_integer'],boolean_integer_agree=gpu['color_write']==gpu['color_write_integer']))
 result['mask_query_scope']='Actual GLint mask must be all1; both boolean APIs retained verbatim, not silently rewritten. No SDK or GL4ES changes.'
 return result
