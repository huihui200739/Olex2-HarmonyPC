"""Same ten genuine initialized-owner startup gates, only for exact candidate25."""
from font_raster_flush_tool_profiles import startup_verifier_source
_namespace=dict(__name__='candidate25_exact_initialized_startup_verifier')
exec(compile(startup_verifier_source(),'candidate25_exact_initialized_startup_verifier','exec'),_namespace)
validate_runtime_snapshot=_namespace['validate_runtime_snapshot']
