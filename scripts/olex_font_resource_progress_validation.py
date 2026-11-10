"""Offline exact23 resource evidence; re-read all actual pixels, metrics and PID captures."""
from pathlib import Path
_BASE=Path(__file__).with_name('native_font_progress_validation.py')
_text=_BASE.read_text()
for old,new in (
 ('from native_font_raster_window_validation import IDENTITY, validate','from olex_font_resource_window_validation import IDENTITY, validate'),
 ('def revalidate_window(report, hap_sha):','def revalidate_window(report, hap_sha, source):'),
 ('current = validate(folder, previous)','current = validate(folder, previous, source)'),
 ('Only successful exact19 font window evidence accepted','Only successful exact23 font window evidence accepted'),
 ("('native_font_raster_window_validation.py', 'native_font_window_validation.py')", "('olex_font_resource_window_validation.py', 'olex_font_resource_fixtures.py', 'vector_font_state_window_validation.py', 'original_vector_font_window_validation.py', 'native_font_raster_window_validation.py', 'native_font_window_validation.py')")):
 if _text.count(old)!=1:raise ValueError('Unexpected frozen prior font progress recipe: '+old)
 _text=_text.replace(old,new,1)
_namespace=dict(__name__='candidate23_exact_original_resource_evidence',__file__=str(_BASE))
exec(compile(_text,str(_BASE)+' [exact23 original resource source-bound]', 'exec'),_namespace)
revalidate_window=_namespace['revalidate_window']
