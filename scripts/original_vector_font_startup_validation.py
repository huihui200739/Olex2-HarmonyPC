"""Same ten genuine initialized-owner startup gates, only for exact candidate20."""
from original_vector_font_tool_profiles import startup_verifier_source
_namespace=dict(__name__='candidate20_exact_initialized_startup_verifier')
exec(compile(startup_verifier_source(),'candidate20_exact_initialized_startup_verifier','exec'),_namespace)
validate_runtime_snapshot=_namespace['validate_runtime_snapshot']
