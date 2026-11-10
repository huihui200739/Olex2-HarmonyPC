"""Exact25 all24 glyph/source/pixel gates; shader stage names matched unambiguously."""
from pathlib import Path
_BASE=Path(__file__).with_name('olex_font_resample_window_validation.py');_text=_BASE.read_text()
replacements=(
 ("versionName='0.22.24',versionCode=222400","versionName='0.22.25',versionCode=222500"),
 ("'olex-font-resource-call-inputs.json','olex-font-resample-inputs.json')","'olex-font-resource-call-inputs.json','olex-font-resample-inputs.json','font-raster-flush-inputs.json')"),
 ("(source/'olex-font-resample-inputs.json').read_text())['version']=='0.22.24'","(source/'font-raster-flush-inputs.json').read_text())['version']=='0.22.25'"),
 ('import hashlib,json,struct,zipfile','import hashlib,json,struct,zipfile,re'),
 ("shaders=list(folder.glob(f'actual-shader-run-{run}-{prefix}-*.txt'))", "shaders=[p for p in folder.glob(f'actual-shader-run-{run}-{prefix}-*.txt') if re.fullmatch(r'actual-shader-run-'+str(run)+'-'+re.escape(prefix)+r'-[0-9]+[.]txt',p.name)]"))
for old,new in replacements:
 if _text.count(old)!=1:raise ValueError('Changed frozen24 analytical recipe: '+old)
 _text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate25_unchanged_resource_pixels_exact_shader_stage_names',__file__=str(_BASE))
exec(compile(_text,str(_BASE)+' [exact25 identity/manifest and exact shader stage names]','exec'),_namespace)
IDENTITY=_namespace['IDENTITY'];validate=_namespace['validate'];validate_source=_namespace['validate_source']
