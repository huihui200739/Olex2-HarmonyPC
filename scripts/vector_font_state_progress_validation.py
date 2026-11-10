"""Re-read exact21 font evidence with all prior layout/PID/resource/pixel gates."""
from pathlib import Path
_BASE=Path(__file__).with_name('native_font_progress_validation.py')
_text=_BASE.read_text()
for old,new in (
 ('from native_font_raster_window_validation import IDENTITY, validate','from vector_font_state_window_validation import IDENTITY, validate'),
 ('def revalidate_window(report, hap_sha):','def revalidate_window(report, hap_sha, source):'),
 ('current = validate(folder, previous)','current = validate(folder, previous, source)'),
 ('Only successful exact19 font window evidence accepted','Only successful exact21 font window evidence accepted'),
 ("('native_font_raster_window_validation.py', 'native_font_window_validation.py')", "('vector_font_state_window_validation.py', 'original_vector_font_window_validation.py', 'native_font_raster_window_validation.py', 'native_font_window_validation.py')")):
 if _text.count(old)!=1:raise ValueError('Unexpected original font progress profile: '+old)
 _text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate21_exact_native_vector_font_progress',__file__=str(_BASE))
exec(compile(_text,str(_BASE)+' [exact21 source-bound native+vector]', 'exec'),_namespace)
revalidate_window=_namespace['revalidate_window']
