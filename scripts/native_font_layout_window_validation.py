"""Exact18 identity profile; all original17 font pixel/resource gates unchanged."""
from pathlib import Path
_BASE=Path(__file__).with_name('native_font_window_validation.py')
text=_BASE.read_text();old="versionName='0.22.17',versionCode=221700";new="versionName='0.22.18',versionCode=221800"
if text.count(old)!=1:raise ValueError('Unexpected font analytical baseline')
namespace=dict(__name__='candidate18_exact_font_pixel_profile',__file__=str(_BASE))
exec(compile(text.replace(old,new,1),str(_BASE)+' [exact18 identity profile]','exec'),namespace)
IDENTITY=namespace['IDENTITY'];validate=namespace['validate']
